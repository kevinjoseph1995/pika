// MIT License

// Copyright (c) 2023 Kevin Joseph

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef PIKA_SHARED_RING_BUFFER_HPP
#define PIKA_SHARED_RING_BUFFER_HPP

#include "channel_interface.hpp"
#include "error.hpp"
#include "synchronization_primitives.hpp"
// System includes
#include <__expected/unexpected.h>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <type_traits>

using namespace pika;

struct RingBufferBase {
    virtual ~RingBufferBase() = default;
    //*****************************************************************************************//
    [[nodiscard]] virtual auto Initialize(uint8_t* buffer, uint64_t element_size,
        uint64_t element_alignment, uint64_t number_of_elements) -> std::expected<void, PikaError>
        = 0;
    //*****************************************************************************************//
    [[nodiscard]] virtual auto PushFront(uint8_t const* const element, DurationUs timeout_duration)
        -> std::expected<void, PikaError>
        = 0;
    //*****************************************************************************************//
    [[nodiscard]] virtual auto GetFrontElementPtr(DurationUs timeout_duration)
        -> std::expected<uint8_t* const, PikaError>
        = 0;
    [[nodiscard]] virtual auto ReleaseFrontElementPtr(uint8_t const* const element)
        -> std::expected<void, PikaError>
        = 0;
    //*****************************************************************************************//
    [[nodiscard]] virtual auto PopBack(uint8_t* const element, DurationUs timeout_duration)
        -> std::expected<void, PikaError>
        = 0;
    //*****************************************************************************************//
    [[nodiscard]] virtual auto GetBackElementPtr(DurationUs timeout_duration)
        -> std::expected<uint8_t const* const, PikaError>
        = 0;
    [[nodiscard]] virtual auto ReleaseBackElementPtr(uint8_t const* const element)
        -> std::expected<void, PikaError>
        = 0;
    //*****************************************************************************************//
    [[nodiscard]] auto GetElementAlignment() const -> uint64_t { return m_element_alignment; }
    [[nodiscard]] auto GetElementSizeInBytes() const -> uint64_t { return m_element_size_in_bytes; }
    [[nodiscard]] auto GetQueueLength() -> uint64_t { return m_queue_length; }

protected:
    [[nodiscard]] auto getBufferSlot(uint64_t index) -> uint8_t*
    {
        PIKA_ASSERT(index < m_queue_length);
        return m_ring_buffer + (index * m_element_size_in_bytes);
    }
    uint8_t* m_ring_buffer = nullptr;
    uint64_t m_element_alignment = 0;
    uint64_t m_element_size_in_bytes = 0;
    uint64_t m_queue_length = 0;
};

template <typename T>
concept RingBufferType = std::derived_from<T, RingBufferBase>;

struct RingBufferLockProtected : public RingBufferBase {
public:
    [[nodiscard]] auto PushFront(uint8_t const* const element, DurationUs timeout_duration)
        -> std::expected<void, PikaError> override;
    [[nodiscard]] auto PopBack(uint8_t* const element, DurationUs timeout_duration)
        -> std::expected<void, PikaError> override;
    [[nodiscard]] auto GetFrontElementPtr(DurationUs timeout_duration)
        -> std::expected<uint8_t* const, PikaError> override;
    [[nodiscard]] auto ReleaseFrontElementPtr(uint8_t const* const element)
        -> std::expected<void, PikaError> override;
    [[nodiscard]] virtual auto GetBackElementPtr(DurationUs timeout_duration)
        -> std::expected<uint8_t const* const, PikaError> override;
    [[nodiscard]] virtual auto ReleaseBackElementPtr(uint8_t const* const element)
        -> std::expected<void, PikaError> override;

protected:
    [[nodiscard]] static auto initialize(RingBufferLockProtected& ring_buffer_object,
        uint8_t* buffer, uint64_t element_size, uint64_t element_alignment,
        uint64_t number_of_elements, bool is_inter_process) -> std::expected<void, PikaError>;

private:
    Mutex m_mutex {}; // Coarse grained lock protecting all accesses to the buffer
    ConditionVariable m_not_empty_condition_variable {};
    ConditionVariable m_not_full_condition_variable {};
    uint64_t m_write_index = 0;
    uint64_t m_read_index = 0;
    uint64_t m_count = 0;
};

struct RingBufferInterProcessLockProtected : public RingBufferLockProtected {
    [[nodiscard]] auto Initialize(uint8_t* buffer, uint64_t element_size,
        uint64_t element_alignment, uint64_t number_of_elements)
        -> std::expected<void, PikaError> override
    {
        return RingBufferLockProtected::initialize(
            *this, buffer, element_size, element_alignment, number_of_elements, true);
    }
};

struct RingBufferInterThreadLockProtected : public RingBufferLockProtected {
    [[nodiscard]] auto Initialize(uint8_t* buffer, uint64_t element_size,
        uint64_t element_alignment, uint64_t number_of_elements)
        -> std::expected<void, PikaError> override
    {
        return RingBufferLockProtected::initialize(
            *this, buffer, element_size, element_alignment, number_of_elements, false);
    }
};

struct RingBufferLockFree : public RingBufferBase {
    [[nodiscard]] auto Initialize(uint8_t* buffer, uint64_t element_size,
        uint64_t element_alignment, uint64_t number_of_elements)
        -> std::expected<void, PikaError> override;
    [[nodiscard]] auto PushFront(uint8_t const* const element, DurationUs timeout_duration)
        -> std::expected<void, PikaError> override;
    [[nodiscard]] auto PopBack(uint8_t* const element, DurationUs timeout_duration)
        -> std::expected<void, PikaError> override;
    [[nodiscard]] auto GetFrontElementPtr(DurationUs timeout_duration)
        -> std::expected<uint8_t* const, PikaError> override
    {
        static_cast<void>(timeout_duration);
        return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "Zero-copy API not supported" } };
    }
    [[nodiscard]] auto ReleaseFrontElementPtr(uint8_t const* const element)
        -> std::expected<void, PikaError> override
    {
        static_cast<void>(element);
        return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "Zero-copy API not supported" } };
    }
    [[nodiscard]] virtual auto GetBackElementPtr(DurationUs timeout_duration)
        -> std::expected<uint8_t const* const, PikaError> override
    {
        static_cast<void>(timeout_duration);
        return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "Zero-copy API not supported" } };
    }
    [[nodiscard]] virtual auto ReleaseBackElementPtr(uint8_t const* const element)
        -> std::expected<void, PikaError> override
    {

        static_cast<void>(element);
        return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "Zero-copy API not supported" } };
    }

private:
    [[nodiscard]] auto getBufferSlot_(uint64_t index) -> uint8_t*
    {
        PIKA_ASSERT(index < m_internal_queue_length);
        return m_ring_buffer + (index * m_element_size_in_bytes);
    }
    auto incrementByOne(uint64_t index) const -> uint64_t
    {
        PIKA_ASSERT(index <= m_internal_queue_length);
        return (index + 1) % (m_internal_queue_length);
    }
    std::atomic_uint64_t m_head = 0;
    std::atomic_uint64_t m_tail = 0;
    uint64_t m_internal_queue_length = 0;
};
#endif