#pragma once

// Standalone probe executable ONLY. Never include in the Skyrim DLL.
// Counts requested C++ allocation bytes, not CRT headers, GPU or engine heaps.
#ifndef BCNG_OFFLINE_MEMORY_PROBE
#error This allocator is restricted to the offline diagnostic executable.
#endif
#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <new>

namespace allocation_meter
{
    struct Snapshot { std::size_t blocks, bytes, allocations, peak; };
    inline std::atomic_size_t blocks{}, bytes{}, allocations{}, peak{};
    struct Header { void* allocation; std::size_t requested; };
    inline Snapshot Read() { return { blocks.load(), bytes.load(), allocations.load(), peak.load() }; }
    inline void ResetPeak() { peak.store(bytes.load()); }
    inline void* Allocate(std::size_t size, std::size_t alignment)
    {
        if (alignment < alignof(Header)) alignment = alignof(Header);
        const auto overhead = sizeof(Header) + alignment - 1;
        if (size > std::numeric_limits<std::size_t>::max() - overhead) throw std::bad_alloc{};
        auto* raw = std::malloc((size ? size : 1) + overhead);
        if (!raw) throw std::bad_alloc{};
        const auto address = (reinterpret_cast<std::uintptr_t>(raw) + sizeof(Header) + alignment - 1) & ~(alignment - 1);
        auto* head = reinterpret_cast<Header*>(address) - 1;
        head->allocation = raw; head->requested = size;
        ++blocks; ++allocations;
        const auto current = bytes.fetch_add(size) + size;
        auto previous = peak.load();
        while (previous < current && !peak.compare_exchange_weak(previous, current)) {}
        return reinterpret_cast<void*>(address);
    }
    inline void Free(void* value) noexcept
    {
        if (!value) return;
        auto* head = reinterpret_cast<Header*>(value) - 1;
        --blocks; bytes.fetch_sub(head->requested);
        std::free(head->allocation);
    }
}

void* operator new(std::size_t n) { return allocation_meter::Allocate(n, alignof(std::max_align_t)); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { allocation_meter::Free(p); }
void operator delete[](void* p) noexcept { allocation_meter::Free(p); }
void operator delete(void* p, std::size_t) noexcept { allocation_meter::Free(p); }
void operator delete[](void* p, std::size_t) noexcept { allocation_meter::Free(p); }
void* operator new(std::size_t n, std::align_val_t a) { return allocation_meter::Allocate(n, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { allocation_meter::Free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { allocation_meter::Free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { allocation_meter::Free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { allocation_meter::Free(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept { return ::operator new(n, t); }
void operator delete(void* p, const std::nothrow_t&) noexcept { allocation_meter::Free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { allocation_meter::Free(p); }
