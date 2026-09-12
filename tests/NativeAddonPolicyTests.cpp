#include "BodyChangeNG/NativeAddonPolicy.h"
#include "BodyChangeNG/NativeAddonRuntime.h"
#include "BodyChangeNG/NativeAddonPatterns.h"
#include <iostream>
#include <optional>
#include <utility>
#include <stdexcept>
#include <thread>

namespace
{
    void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

    struct ReferenceProbe
    {
        int refs{ 1 }, destructions{};
        void IncRefCount() { Check(refs > 0, "acquired a dead TXST"); ++refs; }
        void DecRefCount() { Check(refs > 0, "double TXST release"); if (--refs == 0) ++destructions; }
    };
    struct ProbeLease
    {
        ReferenceProbe* pointer{};
        explicit ProbeLease(ReferenceProbe* p) : pointer(p) { if (pointer) pointer->IncRefCount(); }
        ProbeLease(const ProbeLease& other) : ProbeLease(other.pointer) {}
        ProbeLease(ProbeLease&& other) noexcept : pointer(std::exchange(other.pointer, nullptr)) {}
        ~ProbeLease() { if (pointer) pointer->DecRefCount(); }
    };
}

int main()
{
    using namespace bcn::native_addon;
    try {
        std::array<std::uint8_t, 32> imports{};
        std::uint64_t sleepPointer = 0x7FFB12345678;
        std::memcpy(imports.data()+8, &sleepPointer, 8);
        Check(FindUniqueImport(imports, 0x1000, sleepPointer) == 0x1008U,
            "import identity did not resolve without an Address Library data ID");
        Check(!FindUniqueImport(imports, 0x1000, sleepPointer+1) &&
            !FindUniqueImport(imports, UINT32_MAX-8, sleepPointer) &&
            !FindUniqueImport(std::span{imports}.first(31), 0x1000, sleepPointer),
            "invalid or overflowing IAT accepted");
        std::memcpy(imports.data()+24, &sleepPointer, 8);
        Check(!FindUniqueImport(imports, 0x1000, sleepPointer), "ambiguous OS import accepted");
        // Synthetic rebasing exercises the production matcher, not uncaptured
        // game executables. Only externally relocated operands may move.
        for (const auto group : { std::span{patterns::se}, std::span{patterns::ae} }) {
            for (const auto& pattern : group) {
                for (const std::uint32_t base : { 0x1000U, 0x40000U, 0x2100000U }) {
                    std::vector<std::uint8_t> code(pattern.bytes.begin(), pattern.bytes.end());
                    const auto resolve = [](std::uint64_t id) -> std::optional<std::uint32_t> {
                        return static_cast<std::uint32_t>(id * 16U + 0x300000U);
                    };
                    for (const auto& operand : pattern.operands) {
                        const auto delta = static_cast<std::int32_t>(std::int64_t{*resolve(operand.targetID)} +
                            operand.addend - base - operand.instructionEnd);
                        std::memcpy(code.data() + operand.offset, &delta, 4);
                    }
                    Check(MatchRelocatedCode(pattern, code, base, resolve), "relocated reference rejected");
                    Check(!MatchRelocatedCode(pattern, std::span{code}.first(code.size()-1), base, resolve),
                        "truncated native ownership code accepted");
                    for (std::size_t i{}; i < code.size(); ++i) {
                        code[i] ^= 1U;
                        Check(!MatchRelocatedCode(pattern, code, base, resolve),
                            "changed opcode/field/constant/callee accepted");
                        code[i] ^= 1U;
                    }
                    if (!pattern.operands.empty()) {
                        Check(!MatchRelocatedCode(pattern, code, base,
                            [](std::uint64_t) -> std::optional<std::uint32_t> { return {}; }),
                            "unresolved native dependency accepted");
                    }
                }
            }
        }
        for (const auto prefix : { directVisitorPrefix, seRecursiveVisitorPrefix, aeRecursiveVisitorPrefix }) {
            for (std::uint8_t opcode : { std::uint8_t{0xE8}, std::uint8_t{0xE9} }) {
                std::vector<std::uint8_t> caller(0x600, 0xCC);
                const std::size_t offset = 0x17F;
                std::memcpy(caller.data()+offset, prefix.data(), prefix.size());
                const auto site = offset + prefix.size();
                caller[site] = opcode;
                auto delta = static_cast<std::int32_t>(0x7000 - (0x4000 + site + 5));
                std::memcpy(caller.data()+site+1, &delta, 4);
                Check(FindVisitorBranch(caller, 0x4000, prefix, opcode, 0x7000) == 0x4000+site,
                    "bounded visitor callsite rejected");
                Check(!FindVisitorBranch(caller, 0x4000, prefix, opcode, 0x7001),
                    "foreign visitor hook accepted");
                const std::size_t duplicate = 0x350;
                std::memcpy(caller.data()+duplicate, prefix.data(), prefix.size());
                caller[duplicate+prefix.size()] = opcode;
                delta = static_cast<std::int32_t>(0x7000 - (0x4000 + duplicate + prefix.size() + 5));
                std::memcpy(caller.data()+duplicate+prefix.size()+1, &delta, 4);
                Check(!FindVisitorBranch(caller, 0x4000, prefix, opcode, 0x7000),
                    "ambiguous visitor callsite accepted");
            }
        }
        Check(AcceptSlot(22, 1U << 22), "slot52 rejected");
        Check(!AcceptSlot(2, 1U << 22) && !AcceptSlot(22, 4), "body slot admitted");
        std::array<std::byte, 0x78 * 42> parts{};
        const auto before = parts;
        VisitorContext original{ 22, 0, parts.data(), 123, 0 };
        int texture{};
        {
            ScopedSupply supplied(original, &texture);
            const void* read{};
            std::memcpy(&read, supplied.context.parts + 0x18, sizeof(read));
            Check(read == &texture, "private TXST not supplied");
            Check(supplied.context.index == 0 && supplied.context.actorHandle == 123,
                "visitor identity changed");
            Check(parts == before && original.index == 22 && original.parts == parts.data(),
                "engine-owned BIPOBJECT was modified");
        }
        Check(parts == before, "borrowed pointer escaped into engine data");

        const Paths provider{ "default_d.dds", "default_n.dds", "default_s.dds" };
        Paths skinA{}, skinB{};
        skinA[0] = "A_d.dds";
        skinA[1] = "A_n.dds";
        skinB[0] = "B_d.dds";
        const auto a = ResolvePaths(provider, skinA);
        const auto b = ResolvePaths(provider, skinB);
        Check(a[0] == "A_d.dds" && a[1] == "A_n.dds" && a[2] == provider[2],
            "partial genital pack replaced an unrelated channel");
        Check(b[0] == "B_d.dds" && b[1] == provider[1],
            "switching skin retained a missing channel from previous pack");
        Check(ResolvePaths(provider, {}) == provider, "Default did not select provider paths");

        Baselines baselines;
        const BaselineIdentity erfA{ 100, 0x10, 0x20, Channel::futanari, "CBBE Schlong" };
        const BaselineIdentity erfB{ 200, 0x10, 0x20, Channel::futanari, "CBBE Schlong" };
        Check(baselines.Put(erfA, provider), "first baseline was not stored");
        Check(!baselines.Put(erfA, provider), "same baseline caused duplicate mutation");
        Check(baselines.Diffuse(erfA) == "default_d.dds", "baseline diffuse was lost");
        Check(baselines.Diffuse(erfB).empty(), "baseline leaked to another actor");
        auto providerB = provider;
        providerB[0] = "other_default_d.dds";
        baselines.Put(erfB, providerB);
        baselines.Retain(200, Channel::futanari,
            { Target{ 0x30, 0x40, { "TRX Schlong" } } });
        Check(baselines.Diffuse(erfB).empty(),
            "replaced addon baseline accumulated after target change");
        baselines.Put(erfB, providerB);
        baselines.Forget(100);
        Check(baselines.Diffuse(erfA).empty() && baselines.Diffuse(erfB) == providerB[0],
            "actor baseline cleanup affected another actor");
        baselines.Reset();
        Check(baselines.Diffuse(erfB).empty(), "baseline reset leaked a value snapshot");

        // Uses the same adoption helper as the real backend. The native
        // material/renderer retain independent intrusive references; releasing
        // the visitor's lease alone cannot invalidate either one.
        for (int repeat{}; repeat < 1000; ++repeat) {
            ReferenceProbe form;
            std::optional<ProbeLease> material, renderer;
            {
                auto visitor = AdoptFactoryTexture<ReferenceProbe, ProbeLease>(&form);
                Check(form.refs == 1, "factory's initial reference leaked during adoption");
                material.emplace(visitor);
                renderer.emplace(visitor);
            }
            Check(form.refs == 2 && form.destructions == 0, "visitor freed retained TXST");
            material.reset(); // selection change / unequip / 3D replacement
            Check(form.refs == 1 && form.destructions == 0, "renderer retained a dead TXST");
            renderer.reset();
            Check(form.refs == 0 && form.destructions == 1, "old TXST leaked or was deleted twice");
        }

        Selections store;
        Selection first{ Channel::maleGenitals, {{123, 456, {"Genitals"}}}, {} };
        first.overrides[0] = "skin_A.dds";
        Selection second = first;
        second.overrides[0] = "skin_B.dds";
        store.Publish(100, first);
        Check(!store.Publish(100, first), "same source/selection requested duplicate native refresh");
        store.Publish(200, second); // same ARMO/ARMA, different actor and skin
        auto lease = store.Get(100);
        std::weak_ptr<const Selection> previous = lease;
        store.Publish(100, second);
        Check(lease->overrides[0] == "skin_A.dds", "publication mutated active reader");
        Check(store.Get(100)->overrides[0] == "skin_B.dds", "repeat choice ignored");
        Check(store.Get(200)->overrides[0] == "skin_B.dds", "shared ARMO propagated selection");
        lease.reset();
        Check(previous.expired(), "old selection leaked after last reader");

        store.Publish(100, first);
        std::jthread writer([&] {
            for (int i{}; i < 2000; ++i) store.Publish(100, i % 2 ? first : second);
        });
        for (int i{}; i < 2000; ++i) {
            auto value = store.Get(100);
            Check(value && (value->overrides[0] == "skin_A.dds" ||
                           value->overrides[0] == "skin_B.dds"), "partial snapshot observed");
        }
        writer.join();
        lease = store.Get(100);
        previous = lease;
        store.Forget(100); // unload/forget does not mutate persistent ActorState
        Check(!store.Get(100) && store.Get(200), "actor removal affected another actor");
        store.Reset();
        Check(!store.Get(200) && !previous.expired(), "reset invalidated an in-flight lease");
        lease.reset();
        Check(previous.expired(), "reset retained obsolete snapshot");
        std::cout << "Native addon policy tests passed (not engine/render tests)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
