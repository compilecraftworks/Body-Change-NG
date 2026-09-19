#include "BodyChangeNG/RemovalPreparation.h"
#include "BodyChangeNG/SkinTransactionPolicy.h"
#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool condition, const char* message)
{ if (!condition) throw std::runtime_error(message); }

int main() try
{
    using namespace bcn::removal;
    for (bool current : {false, true}) for (bool empty : {false, true}) {
        Check(bcn::native_skin::CanRunDefaultRestore(current, empty) == (empty && current),
            "shared-base cleanup suppressed a face or overrode a newer selection");
    }
    for (unsigned count : {1U, 2U, 32U, 256U}) {
        unsigned generation{}, faces{}, bodyRefreshes{};
        std::vector<unsigned> tickets;
        for (unsigned i{}; i < count; ++i) {
            if (!bcn::native_skin::ReuseDefaultGeneration(generation != 0U, true, true)) ++generation;
            tickets.push_back(generation);
        }
        bool attached = true;
        for (const auto ticket : tickets) if (bcn::native_skin::CanRunDefaultRestore(ticket == generation, true)) {
            ++faces;
            if (attached) ++bodyRefreshes;
            attached = false;
        }
        Check(faces == count && bodyRefreshes == 1U, "shared reset lost a face or repeated a body refresh");
        ++generation; // Intervening Apply followed by another Default.
        for (const auto ticket : tickets)
            Check(!bcn::native_skin::CanRunDefaultRestore(ticket == generation, true), "old reset revived");
    }
    Check(!bcn::native_skin::ReuseDefaultGeneration(true, false, true) &&
        !bcn::native_skin::ReuseDefaultGeneration(true, true, false), "changed intent reused a generation");
    for (unsigned mask{}; mask < 32; ++mask) {
        Status status{ .phase = Phase::running, .pendingActors = mask & 1U,
            .nativeSkinPending = (mask & 2U) != 0, .morphsPending = (mask & 4U) != 0,
            .tintPending = (mask & 8U) != 0, .facePending = (mask & 16U) != 0 };
        Check(CleanupComplete(status) == (mask == 0), "unfinished cleanup reported ready");
        for (auto phase : {Phase::idle, Phase::queued, Phase::incomplete, Phase::failed}) {
            status.phase = phase;
            Check(!CleanupComplete(status), "unverified state reported ready");
        }
    }
    struct Binding { std::array<std::string, 8> originalPaths, live; };
    std::array<Binding, 2> privateGraph;
    for (auto& binding : privateGraph) {
        binding.originalPaths.fill("provider/original.dds");
        binding.originalPaths[4].clear(); // An absent optional map stays absent.
    }
    const auto provider = privateGraph;
    auto& tngCachedChild = privateGraph; // Value model of shared private ARMAs.
    for (int failure = -1; failure < 16; ++failure) {
        for (auto& binding : privateGraph) binding.live.fill("BodyChangeNG/Cache/selected.dds");
        int writes{};
        const bool ok = bcn::skin_transaction::RestoreOriginalTextures(privateGraph,
            [&](Binding& binding, std::size_t index, const std::string& path) {
                if (writes++ == failure) return false;
                binding.live[index] = path;
                return true;
            });
        Check(writes == 16 && ok == (failure == -1), "clone cleanup stopped on first failure");
        for (int i{}; i < 16; ++i) {
            Check(tngCachedChild[i / 8].live[i % 8] == (i == failure ?
                "BodyChangeNG/Cache/selected.dds" : provider[i / 8].originalPaths[i % 8]),
                "cached TNG child retained a stale selection");
            Check(privateGraph[i / 8].originalPaths[i % 8] == provider[i / 8].originalPaths[i % 8],
                "cleanup changed the provider baseline");
        }
    }
    bcn::face_skin::Baseline face;
    face.owned[0] = "textures/BodyChangeNG/Cache/skin-face/old.dds";
    face.pending[0] = "textures/BodyChangeNG/Cache/skin-face/new.dds";
    for (auto path : {face.owned[0], face.pending[0]})
        Check(bcn::face_skin::Owns(path, bcn::face_skin::OwnedValue(face, 0, path)),
            "unloaded saved face key escaped ownership verification");
    for (std::string path : {"", "other-mod/head.dds", "provider/original.dds"})
        Check(!bcn::face_skin::Owns(path, bcn::face_skin::OwnedValue(face, 0, path)),
            "unloaded cleanup claimed another provider's saved key");
    std::cout << "Removal preparation policy tests passed (not in-game cleanup proof)\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
