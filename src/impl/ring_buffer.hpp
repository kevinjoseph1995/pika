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

#include "error.hpp"
#include "synchronization_primitives.hpp"
// System includes
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

template <typename ElementType> struct SharedRingBuffer;

struct WriteSlot {
    auto GetElement() const -> uint8_t* { return m_element; }
    ~WriteSlot();
    WriteSlot(WriteSlot&& other);
    void operator=(WriteSlot&& other);

private:
    WriteSlot(LockedMutex locked_mutex, ConditionVariable* cv, uint8_t* element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
        , m_cv(cv)
    {
    }
    friend struct RingBuffer;
    uint8_t* m_element = nullptr;
    LockedMutex m_locked_mutex;
    ConditionVariable* m_cv = nullptr;
};

struct ReadSlot {
    auto GetElement() const -> uint8_t const* { return m_element; }
    ~ReadSlot();
    ReadSlot(ReadSlot&& other);
    void operator=(ReadSlot&& other);

protected:
    ReadSlot(LockedMutex locked_mutex, ConditionVariable* cv, uint8_t const* element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
        , m_cv(cv)
    {
    }
    friend struct RingBuffer;
    uint8_t const* m_element = nullptr;
    LockedMutex m_locked_mutex;
    ConditionVariable* m_cv = nullptr;
};

struct RingBuffer {
public:
    [[nodiscard]] auto Initialize(uint8_t* buffer, uint64_t element_size,
        uint64_t element_alignment, uint64_t number_of_elements, bool is_inter_process)
        -> std::expected<void, PikaError>;

    [[nodiscard]] auto GetWriteSlot() -> std::expected<WriteSlot, PikaError>;

    [[nodiscard]] auto GetReadSlot() -> std::expected<ReadSlot, PikaError>;

    [[nodiscard]] auto GetElementAlignment() const -> uint64_t { return m_element_alignment; }
    [[nodiscard]] auto GetElementSizeInBytes() const -> uint64_t { return m_element_size_in_bytes; }
    [[nodiscard]] auto GetElementAlignment() -> uint64_t { return m_element_alignment; }
    [[nodiscard]] auto GetQueueLength() -> uint64_t { return m_queue_length; }

private:
    [[nodiscard]] auto getBufferSlot(uint64_t index) -> uint8_t*
    {
        return m_ring_buffer + index * m_element_size_in_bytes;
    }

    struct Header {
        Mutex mutex {}; // Coarse grained lock protecting all accesses to the buffer
        ConditionVariable not_empty_condition_variable {};
        ConditionVariable not_full_condition_variable {};
        uint64_t write_index = 0;
        uint64_t read_index = 0;
        uint64_t count = 0;
    } m_header {};

    uint8_t* m_ring_buffer = nullptr;
    uint64_t m_element_alignment = 0;
    uint64_t m_element_size_in_bytes = 0;
    uint64_t m_queue_length = 0;
};

#endif