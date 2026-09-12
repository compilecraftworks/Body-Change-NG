#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/FaceSkinSerialization.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    void Check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
    struct Stream
    {
        std::vector<char> bytes;
        std::size_t position{};
        template<class T> bool Write(const T& value)
        {
            auto begin = reinterpret_cast<const char*>(&value);
            bytes.insert(bytes.end(), begin, begin + sizeof(T));
            return true;
        }
        template<class T> bool Read(T& value)
        {
            if (position + sizeof(T) > bytes.size()) return false;
            std::memcpy(&value, bytes.data() + position, sizeof(T));
            position += sizeof(T);
            return true;
        }
        bool WriteText(const std::string& text)
        {
            Write(static_cast<std::uint32_t>(text.size()));
            bytes.insert(bytes.end(), text.begin(), text.end());
            return true;
        }
        bool ReadText(std::string& text)
        {
            std::uint32_t size{};
            if (!Read(size) || size > 1024 || position + size > bytes.size()) return false;
            text.assign(bytes.data() + position, size);
            position += size;
            return true;
        }
    };
}

int main()
try {
    using namespace bcn::face_skin;
    const auto path = CacheOverridePath("textures/BodyChangeNG/Cache/skin-face/ABC/head.dds");
    Check(path && *path == "textures\\BodyChangeNG\\Cache\\skin-face\\ABC\\head.dds",
        "face NiOverride must receive resource path, not native TXST path");
    Check(!CacheOverridePath("BodyChangeNG/Cache/skin-face/ABC/head.dds"), "unvalidated cache input accepted");
    Check(!CacheOverridePath("textures/BodyChangeNG/Cache/../head.dds"), "cache traversal accepted");
    Check(!CanRestoreChannel(0, false) && !CanRestoreChannel(1, false), "blank essential Default accepted");
    Check(CanRestoreChannel(3, false) && CanRestoreChannel(0, true), "optional detail removal blocked");
    Check(!RequiredTextureReady(0, true, false, false), "path readback alone marked diffuse loaded");
    Check(!RequiredTextureReady(1, true, true, false), "renderer-less normal texture marked loaded");
    Check(RequiredTextureReady(0, true, true, true), "loaded diffuse rejected");
    Check(RequiredTextureReady(3, false, false, false), "absent optional detail rejected");
    Check(kChannels == std::array<std::uint8_t, 5>{0,1,2,3,7}, "face-only channels changed / tint included");
    std::string operations;
    const std::string testDDS = "textures/original/detail.dds";
    Check(WriteLegacyString(testDDS, true,
        [&] { operations += 'W'; }, [&] { operations += 'R'; return testDDS; },
        [&] { operations += 'S'; }) && operations == "WRS",
        "legacy face persistence did not use RaceMenu's read-back interned string");
    operations.clear();
    Check(WriteLegacyString(testDDS, false,
        [&] { operations += 'W'; }, [&] { operations += 'R'; return testDDS; },
        [&] { operations += 'S'; }) && operations == "W", "NPC transient face created a persistent override");
    operations.clear();
    Check(!WriteLegacyString(testDDS, true,
        [&] { operations += 'W'; }, [&] { operations += 'R'; return "wrong.dds"; },
        [&] { operations += 'S'; }) && operations == "WR", "failed read-back overwrote a valid persistent key");
    operations.clear();
    Check(!WriteLegacyString("", true,
        [&] { operations += 'W'; }, [&] { operations += 'R'; return testDDS; },
        [&] { operations += 'S'; }) && operations.empty(), "legacy immediate path loaded an empty string");
    Check(Persistent(true) && !Persistent(false), "RSV player/NPC persistence contract changed");
    // Production gate: no face writes between selection and engine completion.
    RebuildGate gate;
    Check(gate.CanApply(false, true, false), "unchanged body cannot retry face without a rebuild");
    gate.Request();
    Check(!gate.CanApply(false, true, false), "deferred face ran before rebuild dispatch was supplied");
    Check(!gate.Begin(false, false) && gate.requested, "missing dispatch consumed the rebuild request");
    Check(!gate.Begin(true, true), "engine rebuild overlapped an outstanding face call");
    Check(gate.Begin(false, true) && gate.inFlight, "drained face did not release the rebuild");
    Check(!gate.CanApply(false, true, false), "dispatch acceptance was mistaken for 3D completion");
    for (int i = 0; i < 100; ++i) {
        gate.Request();
        Check(!gate.Begin(false, true), "repeated selection dispatched concurrent rebuilds");
    }
    gate.Complete();
    Check(gate.Blocked(), "old rebuild event released a newer pending selection early");
    Check(gate.Begin(false, true), "latest coalesced selection lost its required rebuild");
    Check(!gate.Begin(false, true), "one pending selection dispatched two rebuilds");
    gate.Complete();
    Check(gate.CanApply(false, true, false), "latest face not released by rebuild event");
    Check(!gate.CanApply(true, true, false), "two face batches ran together");
    Check(!gate.CanApply(false, true, true), "delayed event pump repeated a completed face batch");
    Check(!gate.CanApply(false, false, false), "addon-only refresh created an empty face selection");
    // Default is a real (empty-path) selection, not 'no selection'.
    gate.Request();
    Check(!gate.CanApply(false, true, false) && gate.Begin(false, true), "Default bypassed the barrier");
    gate.Complete();
    Check(gate.CanApply(false, true, false), "Default was never released");
    RebuildGate otherActor;
    gate.Request();
    Check(otherActor.CanApply(false, true, false), "player barrier blocked a separate NPC");
    gate = {}; // Session reset/forget discards the gate, not the serialized selection.
    Check(gate.CanApply(false, true, false), "session reset retained a dead rebuild barrier");
    Check(OwnsActiveBatch(1, 1, 10, 10), "active batch rejected");
    Check(!OwnsActiveBatch(1, 2, 10, 10), "callback from previous load modified new session");
    Check(!OwnsActiveBatch(1, 1, 10, 11), "detached actor callback ended newly attached actor batch");
    Check(RestoreVisible("", "", "", "").empty(), "absent original detail must restore empty, not previous pack");
    const auto originalDetail = CaptureVisiblePath("",
        "data\\TEXTURES\\actors\\character\\Female\\FemaleHeadDetail_Age40.dds");
    Check(originalDetail == "textures\\actors\\character\\female\\femaleheaddetail_age40.dds",
        "empty TXST discarded the actual original complexion texture");
    Check(CaptureVisiblePath("old/path.dds", "Textures/actual/path.dds") == "textures\\actual\\path.dds",
        "stale property took precedence over the loaded texture");
    Check(CaptureVisiblePath("original/head.dds", "BSShader_DefNormalMap") == "original/head.dds",
        "engine default texture name was saved as a loadable DDS");
    Check(CaptureVisiblePath("", "BSShader_DefNormalMap").empty() &&
        ReloadableTexturePath("Textures/../head.dds").empty() &&
        ReloadableTexturePath("C:/textures/head.dds").empty(), "invalid texture resource accepted");
    for (int i = 0; i < 100; ++i) {
        const std::string selected = "textures/BodyChangeNG/Cache/skin-face/pack" + std::to_string(i) + "/detail.dds";
        Check(RestoreVisible(selected, selected, "", originalDetail) == originalDetail,
            "detail-pack to no-detail-pack transition loads an empty string");
        Check(RestoreVisible("rsv/newdetail.dds", selected, "", originalDetail) == "rsv/newdetail.dds",
            "baseline fallback overwrote a later provider's detail");
    }
    Check(!Owns("", "") && !Owns("rsv/head.dds", "bcng/head.dds"), "foreign key ownership");
    Check(ResolveReapply(true, false) == ReapplyAction::complete &&
        ResolveReapply(false, true) == ReapplyAction::wait &&
        ResolveReapply(false, false) == ReapplyAction::applyFace,
        "same body skin marked an incomplete face as applied or restarted an in-flight face batch");
    Check(Owns("textures\\bodychangeng\\cache\\skin-face\\ABC\\femalehead.dds",
        "Data/Textures/BodyChangeNG/Cache/skin-face/abc/FemaleHead.dds"),
        "RaceMenu string interning spelling treated a BCNG key as a foreign provider");
    Check(!Owns("other/skin-face/abc/head.dds", "BodyChangeNG/Cache/skin-face/abc/head.dds") &&
        !Owns("BodyChangeNG/Cache/skin-face/def/head.dds", "BodyChangeNG/Cache/skin-face/abc/head.dds"),
        "ownership ignored the full cache namespace or selected pack identity");
    Check(RestoreVisible("textures/BCNG/head.dds", "bcng/HEAD.dds", "rsv/original.dds", "nif/head.dds") ==
        "rsv/original.dds", "Default kept BCNG as its own foreign replacement after case/root normalization");
    Check(RestoreVisible("rsv/new.dds", "bcng/head.dds", "rsv/old.dds", "nif/head.dds") == "rsv/new.dds",
        "Default destroyed a later provider");
    Check(RestoreVisible("bcng/head.dds", "bcng/head.dds", "rsv/old.dds", "nif/head.dds") == "rsv/old.dds",
        "Default lost original override");
    Check(RestoreVisible("", "", "", "nif/head.dds") == "nif/head.dds", "NPC baseline lost");

    Baseline female{.actor=0x14, .base=7, .node="FemaleHeadNord", .female=true};
    female.visible[0] = "actors/character/female/femalehead.dds";
    female.saved[0] = "actors/character/RSV/nordfemale/femalehead.dds";
    female.owned[0] = "BodyChangeNG/Cache/skin-face/selected/head.dds";
    female.pending[0] = "BodyChangeNG/Cache/skin-face/next/head.dds";
    female.touched = 1;
    female.visible[3] = originalDetail;
    Baseline male{.actor=0x12345, .base=0x6789, .node="MaleHeadNord", .female=false};
    male.visible[0] = "actors/character/male/malehead.dds";
    male.touched = 1;
    Stream stream;
    Check(WriteBaselines({female, male}, [&](const auto& v) { return stream.Write(v); },
        [&](const auto& v) { return stream.WriteText(v); }), "baseline save");
    const auto restored = ReadBaselines([&](auto& v) { return stream.Read(v); },
        [&](auto& v) { return stream.ReadText(v); });
    Check(restored && restored->size()==2, "baseline load");
    Check((*restored)[0].owned == female.owned && (*restored)[0].saved == female.saved &&
        (*restored)[0].visible[3] == originalDetail &&
        (*restored)[0].pending == female.pending && (*restored)[0].touched == 1 && (*restored)[0].female,
        "save during persistent dispatch lost ownership");
    for (const auto& current : {female.owned[0], female.pending[0]}) {
        Check(Owns(current, OwnedValue((*restored)[0], 0, current)),
            "save on either side of persistent dispatch must restore original, not preserve BCNG as foreign");
    }
    Check((*restored)[1].owned[0].empty() && (*restored)[1].visible == male.visible && !(*restored)[1].female,
        "NPC live-only baseline became a persistent override");
    for (int i=0; i<100; ++i) {
        auto next = "selected/" + std::to_string(i) + ".dds";
        female.owned[0] = next;
        Check(RestoreVisible(next, female.owned[0], female.saved[0], female.visible[0]) == female.saved[0],
            "repeated selections lost original baseline");
        Check(male.visible[0] == "actors/character/male/malehead.dds", "another NPC was modified");
    }
    for (std::size_t length{}; length<stream.bytes.size(); ++length) {
        Stream truncated{.bytes=std::vector<char>(stream.bytes.begin(), stream.bytes.begin()+length)};
        Check(!ReadBaselines([&](auto& v) { return truncated.Read(v); },
            [&](auto& v) { return truncated.ReadText(v); }), "partial record accepted");
    }
    Stream oversized;
    oversized.Write(kMaxBaselines + 1);
    Check(!ReadBaselines([&](auto& v) { return oversized.Read(v); },
        [&](auto& v) { return oversized.ReadText(v); }), "unbounded baseline allocation");
    std::cout << "Face skin policy / restoration serialization tests passed (not an engine integration test)\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
