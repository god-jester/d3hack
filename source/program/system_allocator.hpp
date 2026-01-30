#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <streambuf>
#include <string_view>

#include "lib/diag/assert.hpp"
#include "lib/reloc/reloc.hpp"
#include "lib/util/sys/mem_layout.hpp"
#include "nn/mem.hpp"

namespace d3::system_allocator {
    inline auto LookupOffsetAddress(const char *name) -> uintptr_t {
        const auto *entry = exl::reloc::GetLookupTable().FindByName(name);
        EXL_ABORT_UNLESS(entry != nullptr, "Missing lookup entry: %s", name);
        const auto &module = exl::util::GetModuleInfo(entry->m_ModuleIndex);
        return module.m_Total.m_Start + entry->m_Offset;
    }

    inline auto GetSystemAllocator() -> nn::mem::StandardAllocator * {
        static nn::mem::StandardAllocator **s_allocator_ptr = nullptr;
        if (s_allocator_ptr == nullptr) {
            s_allocator_ptr = reinterpret_cast<nn::mem::StandardAllocator **>(
                LookupOffsetAddress("system_allocator_ptr")
            );
        }
        if (s_allocator_ptr == nullptr || *s_allocator_ptr == nullptr) {
            return nullptr;
        }
        return *s_allocator_ptr;
    }

    class Buffer {
       public:
        explicit Buffer(nn::mem::StandardAllocator *allocator) : allocator_(allocator) {}

        ~Buffer() {
            if (allocator_ != nullptr && data_ != nullptr) {
                allocator_->Free(data_);
            }
        }

        Buffer(const Buffer &)            = delete;
        Buffer &operator=(const Buffer &) = delete;

        auto ok() const -> bool { return ok_; }

        auto size() const -> size_t { return size_; }

        auto data() -> char * { return data_; }
        auto data() const -> const char * { return data_; }

        auto view() const -> std::string_view { return std::string_view(data_, size_); }

        auto Clear() -> void { size_ = 0; }

        auto Resize(size_t size) -> bool {
            if (!Reserve(size)) {
                ok_ = false;
                return false;
            }
            size_ = size;
            return true;
        }

        auto Append(const char *data, size_t size) -> bool {
            if (size == 0) {
                return true;
            }
            if (size > std::numeric_limits<size_t>::max() - size_) {
                ok_ = false;
                return false;
            }
            if (!Reserve(size_ + size)) {
                ok_ = false;
                return false;
            }
            std::memcpy(data_ + size_, data, size);
            size_ += size;
            return true;
        }

       private:
        auto Reserve(size_t size) -> bool {
            if (allocator_ == nullptr) {
                return false;
            }
            if (size <= capacity_) {
                return true;
            }

            const size_t grow_by = std::max<size_t>(capacity_ / 2, 1024);
            const size_t target  = std::max(size, capacity_ + grow_by);

            void *new_data = allocator_->Allocate(target);
            if (new_data == nullptr) {
                return false;
            }

            if (data_ != nullptr && size_ > 0) {
                std::memcpy(new_data, data_, size_);
                allocator_->Free(data_);
            }

            data_     = static_cast<char *>(new_data);
            capacity_ = target;
            return true;
        }

        nn::mem::StandardAllocator *allocator_ = nullptr;
        char                       *data_      = nullptr;
        size_t                      size_      = 0;
        size_t                      capacity_  = 0;
        bool                        ok_        = true;
    };

    class BufferStreamBuf final : public std::streambuf {
       public:
        explicit BufferStreamBuf(Buffer *buffer) : buffer_(buffer) {}

       protected:
        auto overflow(int ch) -> int override {
            if (buffer_ == nullptr || !buffer_->ok()) {
                return traits_type::eof();
            }
            if (traits_type::eq_int_type(ch, traits_type::eof())) {
                return traits_type::not_eof(ch);
            }
            const char c = static_cast<char>(ch);
            if (!buffer_->Append(&c, 1)) {
                return traits_type::eof();
            }
            return ch;
        }

        auto xsputn(const char *s, std::streamsize count) -> std::streamsize override {
            if (buffer_ == nullptr || !buffer_->ok()) {
                return 0;
            }
            if (count <= 0) {
                return 0;
            }
            if (!buffer_->Append(s, static_cast<size_t>(count))) {
                return 0;
            }
            return count;
        }

       private:
        Buffer *buffer_;
    };
}  // namespace d3::system_allocator
