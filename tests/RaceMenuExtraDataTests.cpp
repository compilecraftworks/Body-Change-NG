#include "BodyChangeNG/RaceMenuExtraDataGuard.h"
#include "BodyChangeNG/PeImageFile.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
    namespace guard = bcn::racemenu_extra_data;
    std::size_t checks{};
    void Check(bool value, const char* message) {
        ++checks;
        if (!value) throw std::runtime_error(message);
    }
    struct Extra {
        std::array<std::uint8_t, 16> prefix{};
        std::uintptr_t name{};
    };
    static_assert(offsetof(Extra, name) == 0x10);
    int BrokenComparator(const void* left, const void* right) {
        const auto a = *static_cast<const Extra* const*>(left);
        const auto b = *static_cast<const Extra* const*>(right);
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a->name) -
            static_cast<std::uint32_t>(b->name));
    }
    const Extra* Find(const std::vector<const Extra*>& values, std::uintptr_t name) {
        // Same full-width lower/mid/high pointer comparisons as the pinned DLL.
        int low = 0, high = static_cast<int>(values.size()) - 1;
        while (low <= high) {
            const auto mid = (low + high) / 2;
            const auto* extra = values[mid];
            if (extra->name == name) return extra;
            if (extra->name < name) low = mid + 1;
            else high = mid - 1;
        }
        return nullptr;
    }
    void Ordering() {
        constexpr std::array<std::uintptr_t, 11> edges{
            0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000,
            0x1F9CBAABB68, 0x2008B099888, 0x7FFFFFFFFFFF,
            0xFFFFFFFFFFFFFFFE, 0xFFFFFFFFFFFFFFFF
        };
        for (auto a : edges) for (auto b : edges) {
            Check(guard::CompareNameAddresses(a,b) == (a>b)-(a<b), "full-width comparison mismatch");
            Extra ae{{},a}, be{{},b}; const Extra* ap = &ae; const Extra* bp = &be;
            Check(guard::CompareExtraData(&ap,&bp) == (a>b)-(a<b), "qsort ABI/layout mismatch");
        }
        Extra locked{{},0x2008B099888}, pristine{{},0x1F9CBAABB68};
        std::vector<const Extra*> values{&locked,&pristine};
        std::qsort(values.data(), values.size(), sizeof(values[0]), BrokenComparator);
        Check(Find(values,pristine.name)==nullptr, "captured old comparator failure not reproduced");
        std::qsort(values.data(), values.size(), sizeof(values[0]), guard::CompareExtraData);
        for (int pass=0;pass<1000;++pass)
            Check(Find(values,pristine.name)==&pristine, "existing pristine baseline missed on repeat");
        Check(values.size()==2, "unexpected extra baseline");
        std::mt19937_64 rng{0xBC20260928};
        std::vector<Extra> records(50000); values.clear();
        for (auto& record : records) { record.name = rng(); values.push_back(&record); }
        std::qsort(values.data(), values.size(), sizeof(values[0]), guard::CompareExtraData);
        Check(std::is_sorted(values.begin(), values.end(), [](auto a,auto b){return a->name<b->name;}),
            "qsort order does not match full-pointer lookup");
        for (const auto& record : records)
            Check(Find(values,record.name)==&record, "random-address lookup lost an entry");
    }
    std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
        std::ifstream input{path, std::ios::binary|std::ios::ate};
        Check(static_cast<bool>(input), "fixture DLL missing");
        const auto size=input.tellg();
        Check(size>0 && size<16*1024*1024, "fixture file size invalid");
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        input.seekg(0); input.read(reinterpret_cast<char*>(data.data()),size);
        Check(static_cast<bool>(input),"fixture read failed");
        return data;
    }
    struct PrivateImage {
        std::uint8_t* data{};
        std::size_t size{};
        explicit PrivateImage(std::span<const std::uint8_t> file) {
            const auto pe=bcn::code_image::File::Parse(file);
            Check(pe.has_value(),"fixture PE invalid");
            size=pe->size;
            data=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
            Check(data!=nullptr,"private test allocation failed");
            const auto nt=*bcn::code_image::File::Read<std::uint32_t>(file,0x3c);
            const auto header=*bcn::code_image::File::Read<std::uint32_t>(file,nt+24+60);
            std::memcpy(data,file.data(),header);
            const auto count=*bcn::code_image::File::Read<std::uint16_t>(file,nt+6);
            const auto optional=*bcn::code_image::File::Read<std::uint16_t>(file,nt+20);
            for (unsigned i=0;i<count;++i) {
                const auto at=nt+24+optional+i*40;
                const auto rva=*bcn::code_image::File::Read<std::uint32_t>(file,at+12);
                const auto bytes=*bcn::code_image::File::Read<std::uint32_t>(file,at+16);
                const auto raw=*bcn::code_image::File::Read<std::uint32_t>(file,at+20);
                std::memcpy(data+rva,file.data()+raw,bytes);
            }
        }
        ~PrivateImage() { if(data) VirtualFree(data,0,MEM_RELEASE); }
        PrivateImage(const PrivateImage&)=delete;
        PrivateImage& operator=(const PrivateImage&)=delete;
    };
    void ImageGate(const std::filesystem::path& target, const std::vector<std::filesystem::path>& excluded) {
        const auto file=Read(target);
        PrivateImage image{file};
        std::vector<std::uint8_t> before{image.data,image.data+image.size};
        const auto unchanged=[&] { Check(!std::memcmp(image.data,before.data(),image.size),"negative gate modified image"); };
        Check(!guard::InstallTestImage(image.data,file,false),"other runtime accepted"); unchanged();
        Check(!guard::InstallTestImage(nullptr,file,true),"null image accepted"); unchanged();
        Check(!guard::InstallTestImage(image.data,std::span{file}.first(20),true),"truncated file accepted"); unchanged();
        auto modified=file; modified.back()^=1;
        Check(!guard::InstallTestImage(image.data,modified,true),"unknown digest accepted"); unchanged();
        for (auto offset : {0x100U,0xF2390U,0xF23A0U,0x84BAU,0x5FC0U}) {
            image.data[offset]^=1;
            const auto mismatched=std::vector<std::uint8_t>{image.data,image.data+image.size};
            Check(!guard::InstallTestImage(image.data,file,true),"modified live code/header accepted");
            Check(!std::memcmp(image.data,mismatched.data(),image.size),"foreign code changed by gate");
            image.data[offset]^=1; unchanged();
        }
        for (const auto& path : excluded) {
            auto other=Read(path);
            Check(!guard::InstallTestImage(image.data,other,true),"official/other DLL accepted"); unchanged();
        }
        DWORD old{};
        Check(VirtualProtect(image.data+0xF2390,16,PAGE_EXECUTE_READ,&old)!=0,"test page protect failed");
        Check(guard::InstallTestImage(image.data,file,true),guard::Status());
        MEMORY_BASIC_INFORMATION info{};
        Check(VirtualQuery(image.data+0xF2390,&info,sizeof(info))!=0 && info.Protect==PAGE_EXECUTE_READ,
            "page protection not restored");
        for (std::size_t i=0;i<image.size;++i)
            if (i<0xF2390 || i>=0xF2390+14) Check(image.data[i]==before[i],"patch escaped comparator entry");
        const auto patched=std::vector<std::uint8_t>{image.data,image.data+image.size};
        Check(guard::InstallTestImage(image.data,file,true),"repeat install not idempotent");
        Check(!std::memcmp(image.data,patched.data(),image.size),"repeat install changed code");
        // Execute ONLY the new tail-jump into our compiled comparator. No DLL
        // loading, initialization, imports or original third-party code runs.
        using Compare=int(*)(const void*,const void*);
        auto compare=reinterpret_cast<Compare>(image.data+0xF2390);
        Extra a{{},0x2008B099888},b{{},0x1F9CBAABB68};
        const Extra* ap=&a; const Extra* bp=&b;
        Check(compare(&ap,&bp)==1 && compare(&bp,&ap)==-1 && compare(&ap,&ap)==0,"entry jump ABI failure");
        std::cout << "Exact-file private-image gate and patch passed; original DLL untouched.\n";
    }
}
int wmain(int argc,wchar_t** argv) {
    try {
        Ordering();
        if (argc>1) {
            std::vector<std::filesystem::path> excluded;
            for (int i=2;i<argc;++i) excluded.emplace_back(argv[i]);
            ImageGate(argv[1],excluded);
        } else std::cout << "Private-image test skipped: supply pinned UBE SE DLL and optional excluded DLLs.\n";
        std::cout << "RaceMenu ExtraData: " << checks << " checks passed.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
