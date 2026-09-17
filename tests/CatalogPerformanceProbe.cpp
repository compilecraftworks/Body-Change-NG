#include "BodyChangeNG/PresetCatalog.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>

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

int main()
{
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
