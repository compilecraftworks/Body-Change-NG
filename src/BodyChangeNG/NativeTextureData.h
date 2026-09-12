#pragma once

#include <iterator>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace bcn::native_skin
{
    using FormTextureSlots = std::array<std::size_t, 8>;

    // Only call on a newly-created, unattached PRIVATE form. The engine's
    // BSTextureSet view can reorder the TES record texture components. Ask
    // its real getter which component backs each shader slot instead of
    // assuming that the similarly-named SDK enums imply identical ordering.
    // This neither loads textures nor changes an original/attached form.
    template <class TextureForm, class ReadShaderPath>
    [[nodiscard]] std::optional<FormTextureSlots> DiscoverFormTextureSlots(
        TextureForm& form, ReadShaderPath&& readShaderPath)
    {
        if (std::size(form.textures) != 8U) return std::nullopt;
        constexpr std::array<const char*, 8> probes{
            "bcng_slot_probe_0.dds", "bcng_slot_probe_1.dds",
            "bcng_slot_probe_2.dds", "bcng_slot_probe_3.dds",
            "bcng_slot_probe_4.dds", "bcng_slot_probe_5.dds",
            "bcng_slot_probe_6.dds", "bcng_slot_probe_7.dds"
        };
        struct Restore final {
            TextureForm& form;
            std::array<std::string, 8> paths;
            ~Restore() {
                for (std::size_t i{}; i < paths.size(); ++i) form.textures[i].textureName = paths[i].c_str();
            }
        } restore{ form, {} };
        for (std::size_t i{}; i < probes.size(); ++i) restore.paths[i] = form.textures[i].textureName.c_str();
        for (std::size_t i{}; i < probes.size(); ++i) form.textures[i].textureName = probes[i];
        FormTextureSlots slots{};
        std::array<bool, 8> used{};
        for (std::size_t shader{}; shader < slots.size(); ++shader) {
            const auto path = readShaderPath(shader);
            std::size_t record{};
            while (record < probes.size() && path != probes[record]) ++record;
            if (record == probes.size() || used[record]) return std::nullopt;
            slots[shader] = record;
            used[record] = true;
        }
        return slots;
    }

    // BGSTextureSet's form data is TESTexture[8], not BSShaderTextureSet's
    // string array. SKSE PapyrusTextureSet::SetNthTexturePath writes this
    // member directly. Keep the associated resource identity coherent too.
    template <class TextureForm>
    [[nodiscard]] bool WriteFormTexturePath(TextureForm& form,
        const std::size_t index, const char* path)
    {
        if (index >= std::size(form.textures) || !path) return false;
        form.textures[index].textureName = path;
        form.textureFileIDs[index] = {};
        if (*path) form.textureFileIDs[index].GenerateFromPath(path);
        return std::string_view(form.textures[index].textureName.c_str()) == path;
    }
}
