// OBody NG 4.4.3, GPL-3.0, Aietos / Sairion350 and contributors.
// Unmodified parser function bodies; only engine-independent test bindings.
// https://github.com/Aietos/OBody-NG/blob/4.4.3/src/PresetManager/PresetManager.cpp
namespace PresetManager {
SliderSet SliderSetFromNode(const pugi::xml_node& a_node, const BodyType a_body) {
        SliderSet ret;

        for (auto& node : a_node) {
            if (!stl::cmp(node.name(), "SetSlider")) continue;

            std::string_view name{node.attribute("name").value()};

            bool inverted{false};
            if (a_body == BodyType::UNP) {
                if (std::ranges::contains(DefaultSliders, name)) inverted = true;
            }

            float min{0}, max{0};
            const float val{node.attribute("value").as_float() / 100.0f};
            const auto size{node.attribute("size").value()};

            (stl::cmp(size, "big") ? max : min) = inverted ? 1.0f - val : val;

            AddSliderToSet(ret, Slider(name.data(), min, max), inverted);
        }

        return ret;
    }

void AddSliderToSet(SliderSet& a_sliderSet, Slider&& a_slider, [[maybe_unused]] bool a_inverted) {
        if (const auto it = a_sliderSet.find(a_slider.name); it != a_sliderSet.end()) {
            constexpr float val{};
            auto& current = it->second;
            if ((current.min == val) && (a_slider.min != val)) current.min = a_slider.min;
            if ((current.max == val) && (a_slider.max != val)) current.max = a_slider.max;
        } else {
            a_sliderSet[a_slider.name] = std::move(a_slider);
        }
    }

BodyType GetBodyType(const std::string_view a_body) {
        constexpr std::array unp{"unp"sv, "coco"sv, "bhunp"sv, "uunp"sv};
        return stl::contains(a_body, unp) ? BodyType::UNP : BodyType::CBBE;
    }
}
