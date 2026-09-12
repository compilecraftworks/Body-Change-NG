#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace bcn::code_image
{
    // Bounds-checked, file-backed PE64 view. No loads, executable allocation,
    // address guessing or dereferencing of data supplied by the image.
    class File final {
    public:
        struct Section { std::uint32_t rva{}, length{}, raw{}, flags{}, memoryLength{}; };
        struct Function { std::uint32_t begin{}, end{}, unwind{}; };
        template<class T> static std::optional<T> Read(std::span<const std::uint8_t> bytes, std::size_t offset) {
            if (offset>bytes.size() || sizeof(T)>bytes.size()-offset) return std::nullopt;
            T result{};
            std::memcpy(&result,bytes.data()+offset,sizeof(T));
            return result;
        }
        static std::optional<File> Parse(std::span<const std::uint8_t> data) {
            if (Read<std::uint16_t>(data,0)!=0x5A4D) return std::nullopt;
            const auto nt=Read<std::uint32_t>(data,0x3C);
            if (!nt || *nt>data.size() || data.size()-*nt<264 || Read<std::uint32_t>(data,*nt)!=0x4550U ||
                Read<std::uint16_t>(data,*nt+4)!=0x8664 || Read<std::uint16_t>(data,*nt+24)!=0x20B) return std::nullopt;
            const auto count=*Read<std::uint16_t>(data,*nt+6), optional=*Read<std::uint16_t>(data,*nt+20);
            if (!count || count>96 || optional<240) return std::nullopt;
            File file{data};
            file.size=*Read<std::uint32_t>(data,*nt+24+56);
            if (!file.size || file.size>512*1024*1024) return std::nullopt;
            const auto table=static_cast<std::size_t>(*nt)+24+optional;
            for (std::size_t i{};i<count;++i) {
                const auto offset=table+i*40;
                if (offset>data.size() || data.size()-offset<40) return std::nullopt;
                Section section{*Read<std::uint32_t>(data,offset+12), *Read<std::uint32_t>(data,offset+16),
                    *Read<std::uint32_t>(data,offset+20), *Read<std::uint32_t>(data,offset+36),
                    *Read<std::uint32_t>(data,offset+8)};
                if (section.memoryLength<section.length) section.memoryLength=section.length;
                if (section.raw>data.size() || section.length>data.size()-section.raw ||
                    section.rva>file.size || section.memoryLength>file.size-section.rva) return std::nullopt;
                file.sections.push_back(section);
            }
            const auto exceptionRva=*Read<std::uint32_t>(data,*nt+24+112+24);
            const auto exceptionSize=*Read<std::uint32_t>(data,*nt+24+116+24);
            const auto exceptions=file.Bytes(exceptionRva,exceptionSize);
            if (!exceptionSize || exceptionSize%12 || exceptions.size()!=exceptionSize) return std::nullopt;
            for (std::size_t i{};i<exceptions.size();i+=12) {
                Function function{*Read<std::uint32_t>(exceptions,i), *Read<std::uint32_t>(exceptions,i+4),
                    *Read<std::uint32_t>(exceptions,i+8)};
                if (function.begin>=function.end || !file.HasFlags(function.begin,function.end-function.begin,0x20000000)) return std::nullopt;
                file.functions.push_back(function);
            }
            return file;
        }
        std::span<const std::uint8_t> Bytes(std::uint32_t rva,std::size_t count) const {
            for (const auto& section:sections) {
                if (rva>=section.rva && rva-section.rva<=section.length && count<=section.length-(rva-section.rva))
                    return data.subspan(section.raw+rva-section.rva,count);
            }
            return {};
        }
        bool HasFlags(std::uint32_t rva,std::size_t count,std::uint32_t flags) const {
            for (const auto& section:sections)
                if (rva>=section.rva && rva-section.rva<=section.memoryLength && count<=section.memoryLength-(rva-section.rva))
                    return (section.flags&flags)==flags;
            return false;
        }
        bool IsFunction(std::uint32_t rva) const {
            for (const auto& function:functions) if (function.begin==rva) return true;
            return false;
        }
        std::uint32_t size{};
        std::vector<Function> functions;
    private:
        explicit File(std::span<const std::uint8_t> bytes):data(bytes) {}
        std::span<const std::uint8_t> data;
        std::vector<Section> sections;
    };
}
