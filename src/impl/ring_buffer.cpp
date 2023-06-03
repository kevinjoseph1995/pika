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

#include "ring_buffer.hpp"
#include "error.hpp"
#include <atomic>
#include <cstring>
#include <expected>
#include <thread>

using namespace std::chrono_literals;

auto RingBufferLockProtected::initialize(RingBufferLockProtected& ring_buffer_object,
    uint8_t* ring_buffer, uint64_t element_size, uint64_t element_alignment,
    uint64_t number_of_elements, bool is_inter_process) -> std::expected<void, PikaError>
{
    if (ring_buffer == nullptr) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "SharedRingBuffer::Initialize buffer==nullptr" });
    }

    if (reinterpret_cast<std::uintptr_t>(ring_buffer) % element_alignment != 0) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "SharedRingBuffer::Initialize buffer is not aligned" });
    }

    ring_buffer_object.m_ring_buffer = ring_buffer;
    ring_buffer_object.m_element_alignment = element_alignment;
    ring_buffer_object.m_element_size_in_bytes = element_size;
    ring_buffer_object.m_queue_length = number_of_elements;

    auto result = ring_buffer_object.mutex.Initialize(is_inter_process);
    if (not result.has_value()) {
        return std::unexpected(result.error());
    }
    result = ring_buffer_object.not_empty_condition_variable.Initialize(is_inter_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_empty_condition_variable");
        return std::unexpected(result.error());
    }
    result = ring_buffer_object.not_full_condition_variable.Initialize(is_inter_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_full_condition_variable");
        return std::unexpected(result.error());
    }

    return {};
}

[[nodiscard]] auto RingBufferLockProtected::Put(uint8_t const* const element)
    -> std::expected<void, PikaError>
{
    {
        auto locked_mutex_result = LockedMutex::New(&mutex);
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }

        not_full_condition_variable.Wait(
            locked_mutex_result.value(), [this]() -> bool { return count < m_queue_length; });
        std::memcpy(getBufferSlot(write_index), element, m_element_size_in_bytes);
        write_index = (write_index + 1) % m_queue_length;
        ++count;
    }
    not_empty_condition_variable.Signal();
    return {};
}

[[nodiscard]] auto RingBufferLockProtected::Get(uint8_t* const element)
    -> std::expected<void, PikaError>
{
    {
        auto locked_mutex_result = LockedMutex::New(&mutex);
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }
        not_empty_condition_variable.Wait(
            locked_mutex_result.value(), [&]() -> bool { return count != 0; });
        std::memcpy(element, getBufferSlot(read_index), m_element_size_in_bytes);
        read_index = (read_index + 1) % m_queue_length;
        --count;
    }
    not_full_condition_variable.Signal();
    return {};
}

auto RingBufferLockFree::Initialize(uint8_t* buffer, uint64_t element_size,
    uint64_t element_alignment, uint64_t number_of_elements) -> std::expected<void, PikaError>
{
    m_ring_buffer = buffer;
    m_element_size_in_bytes = element_size;
    m_element_alignment = element_alignment;
    m_queue_length = number_of_elements;
    m_internal_queue_length = m_queue_length + 1;
    m_head.store(0);
    m_tail.store(0);
    return {};
}

auto RingBufferLockFree::Put(uint8_t const* const element) -> std::expected<void, PikaError>
{
    auto const current_tail = m_tail.load(std::memory_order_relaxed);
    auto const next_tail = incrementByOne(current_tail);
    while (next_tail == m_head.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::memcpy(getBufferSlot_(current_tail), element, m_element_size_in_bytes);
    m_tail.store(next_tail, std::memory_order_release);
    return {};
}

auto RingBufferLockFree::Get(uint8_t* const element) -> std::expected<void, PikaError>
{
    auto const current_head = m_head.load(std::memory_order_relaxed);
    while (current_head == m_tail.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::memcpy(element, getBufferSlot_(current_head), m_element_size_in_bytes);
    m_head.store(incrementByOne(current_head), std::memory_order_release);
    return {};
}