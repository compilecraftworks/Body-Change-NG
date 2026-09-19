#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace bcn::native_skin
{
    // TESModelTextureSwap::ClearDataComponent uses the x64 non-trivial array
    // cookie, not a plain calloc buffer. Matches RE::SimpleArray's heap contract.
    // Only adopt engine-created arrays or arrays created through this helper.
    template<class Entry, class Heap>
    class ModelArrayConstruction final
    {
    public:
        static_assert(alignof(Entry) <= alignof(std::size_t));
        static_assert(!std::is_trivially_destructible_v<Entry>);
        ModelArrayConstruction() = default;
        ModelArrayConstruction(const ModelArrayConstruction&) = delete;
        ModelArrayConstruction& operator=(const ModelArrayConstruction&) = delete;
        ~ModelArrayConstruction() { Destroy(data_); }

        bool Allocate(std::size_t capacity)
        {
            if (data_ || !capacity || capacity >
                (std::numeric_limits<std::size_t>::max() - sizeof(std::size_t)) / sizeof(Entry)) return false;
            auto* head = static_cast<std::size_t*>(Heap::Allocate(sizeof(std::size_t) + capacity * sizeof(Entry)));
            if (!head) return false;
            *head = 0; // Only fully constructed entries may be destroyed.
            data_ = reinterpret_cast<Entry*>(head + 1);
            capacity_ = capacity;
            return true;
        }
        bool Append(const Entry& entry)
        {
            if (!data_ || size() >= capacity_) return false;
            std::construct_at(data_ + size(), entry);
            ++Head(data_)[0];
            return true;
        }
        [[nodiscard]] Entry* data() const { return data_; }
        [[nodiscard]] std::size_t size() const { return data_ ? *Head(data_) : 0; }
        [[nodiscard]] Entry* release() { capacity_ = 0; return std::exchange(data_, nullptr); }

        static void Destroy(Entry* entries)
        {
            if (!entries) return;
            auto* head = Head(entries);
            std::destroy_n(entries, *head);
            Heap::Free(head);
        }
    private:
        static std::size_t* Head(Entry* entries) { return reinterpret_cast<std::size_t*>(entries) - 1; }
        Entry* data_{};
        std::size_t capacity_{};
    };
}
