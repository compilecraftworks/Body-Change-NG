#include "BodyChangeNG/RenderedOutfit.h"
#include "BodyChangeNG/AppearanceWork.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

// Compile the actual optional-API consumer against small game/task boundaries.
// No Skyrim, SFS DLL, SKSE or RaceMenu is loaded by this executable.
namespace RE
{
    class Actor;
    inline std::unordered_map<std::uint32_t, std::shared_ptr<Actor>> actors;
    struct Handle { std::uint32_t id; std::shared_ptr<Actor> get() const { return actors.contains(id) ? actors.at(id) : nullptr; } };
    class Actor
    {
    public:
        std::uint32_t id;
        bool preview{}, correction{};
        std::uint32_t GetFormID() const { return id; }
        Handle GetHandle() const { return {id}; }
    };
    struct TESForm
    {
        template<class T> static T* LookupByID(std::uint32_t id)
        { return actors.contains(id) ? actors.at(id).get() : nullptr; }
    };
}
namespace SKSE
{
    struct MessagingInterface
    {
        struct Message { const char* sender{}; std::uint32_t type{}, dataLen{}; void* data{}; };
        bool RegisterListener(const char*, void (*)(Message*)) { return true; }
    };
    inline MessagingInterface* GetMessagingInterface() { static MessagingInterface value; return &value; }
}
namespace bcn
{
    struct Settings
    {
        static Settings& Get() { static Settings value; return value; }
        bool enabled{true};
        bool OutfitCorrectionEnabled() const { return enabled; }
    };
    struct OutfitRefit
    {
        static OutfitRefit& Get() { static OutfitRefit value; return value; }
        unsigned evaluations{};
        void ProcessActor(RE::Actor* actor) { ++evaluations; (void)rendered_outfit::Read(actor); }
    };
    namespace racemenu
    {
        bool HasOutfitCorrection(RE::Actor* actor) { return actor->correction; }
        bool HasActivePreview(RE::Actor* actor) { return actor->preview; }
    }
    namespace frame_tasks
    {
        inline bool inTask{true}, active{true};
        inline std::unordered_map<std::uint32_t, std::function<void()>> jobs;
        bool Active() { return active; }
        bool InGameTask() { return inTask; }
        bool Queue(std::uint32_t actor, std::function<void()> job, std::uint32_t, appearance::WorkChannel)
        { jobs.insert_or_assign(actor, std::move(job)); return true; }
        void Pump() { auto batch = std::move(jobs); jobs.clear(); for (auto& [id, work] : batch) work(); }
    }
}
#define BCNG_RENDERED_OUTFIT_TEST
#include "../src/BodyChangeNG/RenderedOutfit.cpp"

namespace
{
    namespace ro = bcn::rendered_outfit;
    namespace abi = sfs::rendered_outfit_api;
    using abi::Status;
    unsigned calls{}, passed{};
    Status reply = Status::Ready;
    std::vector<abi::Item> outfit;
    std::uint64_t epoch = 1, revision = 1, scene = 1;
    bool badVersion{}, badActor{}, badCount{}, grow{}, changingCapacity{};
    abi::Status __cdecl Query(std::uint32_t id, abi::Snapshot* header,
        abi::Item* items, std::uint32_t capacity, std::uint32_t stride)
    {
        ++calls;
        if (stride != sizeof(abi::Item) || header->structSize != sizeof(*header)) std::abort();
        *header = {sizeof(*header), badVersion ? 2U : 1U, epoch, revision, scene,
            badActor ? id + 1U : id, reply, static_cast<std::uint32_t>(outfit.size()), 0U, 0U, 0U};
        if (reply != Status::Ready) return reply;
        if (changingCapacity) {
            header->requiredCount = capacity + 1U;
            return header->status = Status::BufferTooSmall;
        }
        if (capacity < outfit.size()) {
            if (grow) { grow = false; outfit.resize(outfit.size() + 3U); ++revision; }
            return header->status = Status::BufferTooSmall;
        }
        std::copy(outfit.begin(), outfit.end(), items);
        if (badCount) header->requiredCount = capacity + 1U;
        return reply;
    }
    abi::Status __cdecl GameTaskQuery(std::uint32_t id, abi::Snapshot* header,
        abi::Item* items, std::uint32_t capacity, std::uint32_t stride)
    {
        return Query(id, header, items, capacity, stride);
    }
    void Check(bool value, const char* text)
    {
        if (!value) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
        ++passed;
    }
    void Send(RE::Actor* actor, std::uint32_t reasons = abi::StateChanged)
    {
        abi::Changed event{sizeof(abi::Changed), 1U, epoch, revision, scene,
            actor ? actor->id : 0U, reply, reasons, 0U};
        SKSE::MessagingInterface::Message message{abi::kSender, abi::kChangedMessage, sizeof(event), &event};
        ro::OnChange(&message);
    }
    struct Rules
    {
        std::unordered_set<std::string> blacklistedOutfitNames, blacklistedPlugins, forcedOutfitNames;
        std::unordered_set<std::uint32_t> blacklistedFormIDs, forcedFormIDs;
    };
}

int main()
{
    ro::Reader reader;
    unsigned exportLookups{};
    Check(ro::ResolveQuery([&](const char* name) -> abi::Query {
        ++exportLookups;
        return std::string_view{name} == abi::kGameTaskQueryExport ? GameTaskQuery : Query;
    }) == GameTaskQuery && exportLookups == 1U, "prefer task-phase export over cached OS-thread API");
    Check(ro::ResolveQuery([](const char* name) -> abi::Query {
        return std::string_view{name} == abi::kQueryExport ? Query : nullptr;
    }) == Query, "older SFS export remains supported");
    Check(ro::ResolveQuery([](const char*) -> abi::Query { return nullptr; }) == nullptr,
        "missing exports do not invent an API");
    Check(reader.Read(nullptr, 0x14).route == ro::Route::worn && calls == 0, "absent API keeps worn path without query");
    Check(reader.Read(Query, 0).route == ro::Route::invalidActor && calls == 0, "actor reference zero rejected");
    Check(reader.Read(Query, 0x14).route == ro::Route::rendered, "Ready empty is rendered, never worn fallback");
    for (auto status : {Status::NotManaged, Status::NotReady, Status::WrongThread,
            Status::InvalidArgument, Status::InvalidActor, static_cast<Status>(100)}) {
        reply = status;
        const auto expected = status == Status::NotManaged ? ro::Route::worn :
            status == Status::InvalidActor ? ro::Route::invalidActor : ro::Route::defer;
        Check(reader.Read(Query, 0x14).route == expected, "status routing");
    }
    reply = Status::Ready;
    badVersion = true;
    Check(reader.Read(Query, 0x14).route == ro::Route::defer, "unknown ABI cannot look like naked");
    badVersion = false; badActor = true;
    Check(reader.Read(Query, 0x14).route == ro::Route::defer, "wrong actor snapshot rejected");
    badActor = false; badCount = true;
    Check(reader.Read(Query, 0x14).route == ro::Route::defer, "Ready count overflow rejected");
    badCount = false;
    outfit.resize(20U); grow = true;
    auto view = reader.Read(Query, 0x14);
    Check(view.route == ro::Route::rendered && view.items.size() == 23U && view.snapshot.revision == revision,
        "capacity retries return one complete latest revision");
    const auto before = calls; changingCapacity = true;
    Check(reader.Read(Query, 0x14).route == ro::Route::defer && calls == before + 3U,
        "capacity churn bounded without partial outfit");
    changingCapacity = false; outfit.resize(1000U);
    Check(reader.Read(Query, 0x14).items.size() == 1000U, "large outfit not truncated by scratch retention limit");
    outfit.clear();
    view = reader.Read(Query, 0x14);
    auto planned = std::optional{ro::VersionOf(view.snapshot)};
    Check(ro::CanApply(planned, view), "matching planned stamp");
    ++scene;
    Check(!ro::CanApply(planned, reader.Read(Query, 0x14)), "same outfit new scene invalidates queued plan");
    ++epoch;
    Check(!ro::CanApply(planned, reader.Read(Query, 0x14)), "load epoch invalidates queued plan");
    abi::Changed event{sizeof(abi::Changed), 1U, epoch, revision, scene, 0x14, reply, abi::SceneChanged, 0U};
    Check(ro::DecodeChange(&event, sizeof(event)).has_value(), "borrowed payload decoded");
    Check(!ro::DecodeChange(&event, sizeof(event) - 1U), "short payload rejected");
    event.apiVersion = 2U;
    Check(!ro::DecodeChange(&event, sizeof(event)), "unknown payload ABI rejected");

    using Identity = bcn::outfit_refit_evaluation::ArmorIdentity;
    Rules rules;
    std::unordered_map<std::string, std::string> mappings{{"Actual Armor", "Actual-Refit"},
        {"Registered Dress", "Dress-Refit"}, {"Chest Layer", "Layer-Refit"}};
    std::unordered_map<std::uint32_t, Identity> armors{
        {100U, {"Actual Armor", "Actual.esp", 100U}},
        {200U, {"Registered Dress", "Display.esp", 200U}},
        {50U, {"Chest Layer", "Layer.esp", 50U}},
        {300U, {"Force Ring", "Rings.esp", 300U}}
    };
    const auto resolve = [&](const abi::Item& item) -> std::optional<Identity> {
        const auto id = ro::RuleForm(item);
        return armors.contains(id) ? std::optional{armors.at(id)} : std::nullopt;
    };
    const abi::Item actual{100U, 100U, 4U, 4U, abi::Actual, 0U};
    const abi::Item registered{200U, 200U, 4U, 4U, abi::Registered, 0U};
    const abi::Item ring{300U, 300U, 1U << 6, 1U << 6, abi::Actual, 0U};
    rules.forcedFormIDs.insert(100U);
    rules.blacklistedPlugins.insert("Display.esp");
    auto decision = ro::EvaluateVisible(std::span{&registered, 1}, rules, mappings, resolve);
    Check(!decision.eligible && !decision.forced && decision.preset == "Dress-Refit",
        "registered outfit uses its own plugin/name; hidden actual force ID ignored");
    decision = ro::EvaluateVisible(std::span{&actual, 1}, rules, mappings, resolve);
    Check(decision.eligible && decision.forced && decision.preset == "Actual-Refit",
        "visible actual uses actual armor identity");
    rules.blacklistedPlugins.clear(); rules.blacklistedFormIDs.insert(200U);
    Check(!ro::EvaluateVisible(std::span{&registered, 1}, rules, mappings, resolve).eligible,
        "registered form exclusion uses registered ID");
    rules.blacklistedFormIDs.clear(); rules.blacklistedOutfitNames.insert("Registered Dress");
    Check(!ro::EvaluateVisible(std::span{&registered, 1}, rules, mappings, resolve).eligible,
        "registered name exclusion");
    rules.forcedOutfitNames.insert("Force Ring");
    outfit = {registered, ring};
    decision = ro::EvaluateVisible(outfit, rules, mappings, resolve);
    Check(!decision.eligible && decision.forced, "visible force accessory overrides exclusion, all slots covered");
    outfit.clear();
    decision = ro::EvaluateVisible(outfit, rules, mappings, resolve);
    Check(!decision.eligible && !decision.forced && decision.preset.empty(), "both hidden clear breast/nipple correction");
    rules.blacklistedOutfitNames.clear();
    outfit = {{50U, 50U, 1U << 16, 1U << 16, abi::Registered, 0U}, registered};
    Check(ro::EvaluateVisible(outfit, rules, mappings, resolve).preset == "Dress-Refit",
        "slot32 precedes slot46 independent of API/FormID order");
    std::reverse(outfit.begin(), outfit.end());
    Check(ro::EvaluateVisible(outfit, rules, mappings, resolve).preset == "Dress-Refit", "mapping order deterministic");
    auto effective = registered; effective.visibleSlots = 1U << 6;
    Check(!ro::EvaluateVisible(std::span{&effective, 1}, rules, mappings, resolve).eligible,
        "effective slots win over declared body slot");
    auto clone = registered; clone.formID = 0xFF001234U;
    Check(ro::EvaluateVisible(std::span{&clone, 1}, rules, mappings, resolve).preset == "Dress-Refit",
        "known dynamic original means displayed item's original, not inventory armor");
    clone.flags = abi::OriginalUnknown;
    Check(ro::RuleForm(clone) == clone.formID, "unknown original is not invented");
    outfit = {clone, registered, actual};
    Check(ro::EvaluateVisible(outfit, rules, mappings, resolve).eligible,
        "unresolved item does not block the other visible armors");

    // Actual callback, request coalescing, validation, cleanup, thread gate.
    auto actor = std::make_shared<RE::Actor>(0x14);
    auto other = std::make_shared<RE::Actor>(0x12345);
    RE::actors[actor->id] = actor; RE::actors[other->id] = other;
    ro::Initialize(); // no SFSCore loaded: do not force load a plugin
    Check(!ro::Available(), "optional initialization without SFS");
    ro::g_query.store(GameTaskQuery);
    bcn::frame_tasks::inTask = false;
    const auto offThreadCalls = calls;
    Check(ro::Read(actor.get()).route == ro::Route::defer && calls == offThreadCalls,
        "consumer never queries SFS from render thread");
    Check(!ro::ValidateApply(actor.get()) && calls == offThreadCalls,
        "task-phase export also stays behind the apply-time game-task gate");
    bcn::frame_tasks::inTask = true;
    outfit = {registered};
    (void)ro::Read(actor.get()); (void)ro::Read(other.get());
    Check(ro::ValidateApply(actor.get()), "queued morph validates final source version");
    ++revision; outfit.clear();
    Check(!ro::ValidateApply(actor.get()) && bcn::frame_tasks::jobs.size() == 1U,
        "hide during queue wait cancels stale mapping and requests replan");
    bcn::frame_tasks::Pump();
    Check(ro::ValidateApply(actor.get()), "replanned managed-empty can clear correction");
    ++scene;
    for (unsigned i{}; i < 1000U; ++i) Send(actor.get(), abi::SceneChanged);
    Check(bcn::frame_tasks::jobs.size() == 1U, "1000 same-scene callbacks coalesce to one actor job");
    const auto evaluated = bcn::OutfitRefit::Get().evaluations;
    bcn::frame_tasks::Pump();
    Check(bcn::OutfitRefit::Get().evaluations == evaluated + 1 && bcn::frame_tasks::jobs.empty(),
        "scene event reevaluates once without a self scheduling loop");
    Send(actor.get(), abi::SceneChanged);
    Check(bcn::frame_tasks::jobs.empty(), "duplicate delivered scene ignored");

    // A hidden/displayed switch may first publish NotReady while attachments
    // change. Only the later Ready event may permit a fresh correction/clear.
    for (unsigned transition{}; transition < 128U; ++transition) {
        reply = Status::NotReady; ++revision; Send(actor.get());
        bcn::frame_tasks::Pump();
        Check(!ro::g_tracked.at(actor->id).planned && bcn::frame_tasks::jobs.empty(),
            "transient unavailable state waits for publication without polling");
        reply = Status::Ready; ++revision;
        outfit = transition % 3U == 0U ? std::vector{actual} :
            transition % 3U == 1U ? std::vector{registered} : std::vector<abi::Item>{};
        Send(actor.get()); bcn::frame_tasks::Pump();
        Check(ro::ValidateApply(actor.get()) && bcn::frame_tasks::jobs.empty(),
            "next Ready event immediately resumes actual/displayed/hidden correction");
        auto appliedPlan = ro::g_tracked.at(actor->id).planned;
        ++scene;
        Check(!ro::CanApply(appliedPlan, reader.Read(GameTaskQuery, actor->id)),
            "new task export cannot reuse a stale apply-time scene");
    }
    actor->preview = true;
    Check(!ro::ValidateApply(actor.get()), "body preview does not accept background outfit mutation");
    actor->preview = false;
    ++epoch;
    Send(nullptr, abi::EpochChanged);
    Check(bcn::frame_tasks::jobs.size() == 2U, "epoch reenrolls only tracked actors");
    bcn::frame_tasks::Pump();
    ro::Forget(other->id);
    ++revision; Send(other.get());
    Check(bcn::frame_tasks::jobs.empty(), "detached actor no longer receives work");
    reply = Status::InvalidActor; Send(actor.get());
    Check(ro::g_tracked.empty(), "deleted actor drops metadata");
    reply = Status::NotReady; epoch = 0U;
    (void)ro::Read(actor.get());
    for (unsigned i{}; i < 20U; ++i) bcn::frame_tasks::Pump();
    Check(bcn::frame_tasks::jobs.empty(), "pre-subscription warmup retries bounded");
    ro::Reset();
    Check(ro::g_tracked.empty(), "load reset releases consumer records");
    ro::g_query.store(nullptr);
    Check(ro::ValidateApply(actor.get()), "absent SFS leaves existing apply path unrestricted");
    std::cout << passed << " rendered outfit checks passed\n";
}
