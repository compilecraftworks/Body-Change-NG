#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include "BodyChangeNG/SkinRefreshTargets.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_set>

// Synthetic CPU-only diagnostic. Not an in-game frame-time measurement.
using Clock = std::chrono::steady_clock;
static volatile std::size_t observed{};
template<class F> double Median(F work)
{
    std::vector<double> samples;
    for (int i = 0; i < 55; ++i) {
        const auto start = Clock::now();
        work();
        const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (i >= 5) samples.push_back(ms);
    }
    std::ranges::sort(samples);
    return samples[samples.size() / 2];
}

static int AppearanceAudit()
{
    // Actual production value types and copy/count operations, but synthetic
    // catalogs. This does not measure disk, GPU, game hooks or a full UI frame.
    std::cout << "Synthetic appearance metadata CPU-only median ms\n";
    std::cout << "packs,skin_copy,skin_copy_and_count,futa_copy,tint_copy,shared_skin_cached_count\n";
    for (const int count : {25, 100, 500, 1000}) {
        std::vector<bcn::SkinProfile> skins;
        std::vector<bcn::FutanariSkinProfile> futa;
        std::vector<bcn::player_tint::Asset> tint;
        for (int i{}; i < count; ++i) {
            bcn::SkinProfile skin;
            skin.id = "example-appearance-pack-" + std::to_string(i);
            skin.name = skin.id;
            skin.source = "BodySkin/" + skin.id;
            auto add = [&](auto& layers, const std::string& part) {
                for (int channel{}; channel < 4; ++channel)
                    layers.push_back({static_cast<std::uint8_t>(channel),
                        "BodySkin/" + skin.id + "/textures/actors/character/" + part +
                        "_" + std::to_string(channel) + ".dds"});
            };
            add(skin.body, "body"); add(skin.hands, "hands"); add(skin.feet, "feet");
            add(skin.face, "face"); add(skin.vampireFace, "vampire");
            add(skin.elderBody, "elderbody"); add(skin.elderHands, "elderhands");
            add(skin.elderFace, "elderface"); add(skin.faceDetails, "detail");
            for (std::size_t race{}; race < skin.raceFace.size(); ++race)
                add(skin.raceFace[race], "race" + std::to_string(race));
            bcn::FutanariSkinProfile addon;
            addon.id = addon.name = skin.id;
            addon.source = "Futanari/" + skin.id;
            add(addon.layers, "addon");
            futa.push_back(std::move(addon));
            for (int layer{}; layer < 15; ++layer) {
                const auto path = "BodySkin/" + skin.id + "/textures/actors/character/tintmasks/" + std::to_string(layer) + ".dds";
                tint.push_back({skin.id + std::to_string(layer), skin.id, "Example tint layer", path,
                    static_cast<bcn::player_tint::Layer>(layer), bcn::player_tint::Sex::female, 0U, path});
            }
            skin.textureCount = 4U * (9U + skin.raceFace.size());
            skins.push_back(std::move(skin));
        }
        auto skinCopy = Median([&] { auto copy = skins; observed = copy.size(); });
        auto skinCount = Median([&] {
            auto copy = skins;
            std::size_t total{};
            for (const auto& skin : copy) {
                std::unordered_set<std::string> paths;
                const auto collect = [&](const auto& layers) {
                    for (const auto& layer : layers) paths.insert(layer.path);
                };
                collect(skin.body); collect(skin.cbbeGenitalAnal); collect(skin.unpGenitalAnal);
                collect(skin.hands); collect(skin.feet); collect(skin.face); collect(skin.vampireFace);
                collect(skin.elderBody); collect(skin.elderHands); collect(skin.elderFace);
                for (const auto& race : skin.raceFace) collect(race);
                collect(skin.faceDetails);
                total += paths.size();
            }
            observed = total;
        });
        auto futaCopy = Median([&] { auto copy = futa; observed = copy.size(); });
        auto tintCopy = Median([&] { auto copy = tint; observed = copy.size(); });
        const auto published = std::make_shared<const std::vector<bcn::SkinProfile>>(skins);
        auto sharedRead = Median([&] {
            const auto snapshot = published;
            std::size_t total{};
            for (const auto& skin : *snapshot) total += skin.textureCount;
            observed = total;
        });
        std::cout << count << std::fixed << std::setprecision(4) << ',' << skinCopy << ','
            << skinCount << ',' << futaCopy << ',' << tintCopy << ',' << sharedRead << '\n';
    }
    // Exercise production policies in the bulk-reset submission order:
    // QueueClear coalesces equal Default intent into one ActorBase generation.
    // This is not an engine/face-render integration test.
    std::cout << "Shared-base clear policy: refs,normal_faces,removal_faces,removal_refresh_requests\n";
    for (unsigned count : {1U, 2U, 32U}) {
        std::vector<unsigned> refs(count);
        for (unsigned i{}; i < count; ++i) refs[i] = i + 1;
        unsigned normal{}, removal{}, refreshes{};
        for (auto& ref : refs) {
            normal += bcn::native_skin::CanRunDefaultRestore(true, true);
            if (bcn::native_skin::CanRunDefaultRestore(true, true)) {
                ++removal;
                if (ref != 1U) continue; // shared body detaches only once
                bcn::native_skin::VisitRefreshTargets(&ref,
                    [&](auto visit) { for (auto& peer : refs) visit(&peer); },
                    [](auto*) { return true; }, [&](auto*) { ++refreshes; });
            }
        }
        std::cout << count << ',' << normal << ',' << removal << ',' << refreshes << '\n';
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--appearance-audit") return AppearanceAudit();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1920, 1080};
    unsigned char* pixels{};
    int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    std::cout << "Synthetic: 100 sliders/preset; 640x600 list; Release CPU median ms\n";
    std::cout << "slider_names,presets,deep_copy_ms,metadata_copy_ms,full_rows_ms,clipped_rows_ms,rows_full,rows_clipped\n";
    for (const bool longNames : {false, true}) {
      for (const int count : {100, 500, 1000, 4600}) {
        std::vector<bcn::BodyPreset> presets;
        for (int i = 0; i < count; ++i) {
            bcn::BodyPreset preset;
            preset.name = "Example BodySlide Preset " + std::to_string(i);
            preset.source = "Data/CalienteTools/BodySlide/SliderPresets/Example.xml";
            preset.family = "CBBE 3BA";
            preset.bodySet = "CBBE Body Amazing";
            for (int slider = 0; slider < 100; ++slider)
                preset.sliders.push_back({std::string(longNames ? "ExampleLongSliderName_" : "Slider") + std::to_string(slider), 0.25F, 0.75F});
            presets.push_back(std::move(preset));
        }
        const auto deep = Median([&] {
            auto copy = presets; // same deep-copy semantics as Snapshot().
            observed = copy.back().sliders.size();
        });
        const auto metadata = Median([&] {
            std::vector<bcn::BodyPreset> copy;
            copy.reserve(presets.size());
            for (const auto& p : presets) {
                bcn::BodyPreset item;
                item.name = p.name; item.source = p.source;
                item.family = p.family; item.bodySet = p.bodySet;
                copy.push_back(std::move(item));
            }
            observed = copy.size();
        });
        int rows{};
        const auto draw = [&](bool clipped) {
            rows = 0;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({680, 650});
            ImGui::Begin("Probe", nullptr, ImGuiWindowFlags_NoTitleBar);
            if (ImGui::BeginChild("Catalog", {640, 600}, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                const auto row = [&](int index) {
                    ++rows;
                    const auto& p = presets[index];
                    ImGui::PushID(index);
                    const auto cursor = ImGui::GetCursorScreenPos();
                    const auto rowWidth = ImGui::GetContentRegionAvail().x;
                    ImGui::InvisibleButton("item", {rowWidth - 46, 48});
                    const auto hovered = ImGui::IsItemHovered();
                    (void)ImGui::IsItemClicked();
                    auto* list = ImGui::GetWindowDrawList();
                    list->AddRectFilled(cursor, {cursor.x + rowWidth, cursor.y + 48}, hovered ? 0xFF555555 : 0xFF333333, 4);
                    list->AddText({cursor.x + 10, cursor.y + 7}, 0xFFFFFFFF, p.name.c_str());
                    const auto subtitle = p.family + std::string(" - Compatible");
                    list->AddText({cursor.x + 10, cursor.y + 27}, 0xFFBBBBBB, subtitle.c_str());
                    ImGui::SetCursorScreenPos({cursor.x + rowWidth - 46, cursor.y});
                    ImGui::InvisibleButton("favorite", {46, 48});
                    ImGui::SetCursorScreenPos({cursor.x, cursor.y + 53});
                    ImGui::Dummy({0, 0});
                    ImGui::PopID();
                };
                if (clipped) {
                    ImGuiListClipper clipper;
                    clipper.Begin(count, 53.0F);
                    while (clipper.Step())
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) row(i);
                } else {
                    for (int i = 0; i < count; ++i) row(i);
                }
            }
            ImGui::EndChild();
            ImGui::End();
            ImGui::Render();
        };
        const auto fullDraw = Median([&] { draw(false); });
        const auto fullRows = rows;
        const auto clippedDraw = Median([&] { draw(true); });
        std::cout << (longNames ? "long," : "short,") << count << std::fixed << std::setprecision(4) << ',' << deep << ',' << metadata
                  << ',' << fullDraw << ',' << clippedDraw << ',' << fullRows << ',' << rows << '\n';
      }
    }
    ImGui::DestroyContext();
}
