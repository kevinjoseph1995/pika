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
#include "channel_interface.hpp"
#include "error.hpp"
#include "synchronization_primitives.hpp"
#include <__expected/unexpected.h>
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

    auto result = ring_buffer_object.m_mutex.Initialize(is_inter_process);
    if (not result.has_value()) {
        return std::unexpected(result.error());
    }
    result = ring_buffer_object.m_not_empty_condition_variable.Initialize(is_inter_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_empty_condition_variable");
        return std::unexpected(result.error());
    }
    result = ring_buffer_object.m_not_full_condition_variable.Initialize(is_inter_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_full_condition_variable");
        return std::unexpected(result.error());
    }

    return {};
}

[[nodiscard]] auto RingBufferLockProtected::PushFront(
    uint8_t const* const element, DurationUs timeout_duration) -> std::expected<void, PikaError>
{
    {
        auto locked_mutex_result = [&]() {
            return timeout_duration == pika::INFINITE_TIMEOUT
                ? LockedMutex::New(&m_mutex)
                : LockedMutex::New(&m_mutex, timeout_duration);
        }();
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }

        m_not_full_condition_variable.Wait(
            locked_mutex_result.value(), [this]() -> bool { return m_count < m_queue_length; });
        std::memcpy(getBufferSlot(m_write_index), element, m_element_size_in_bytes);
        m_write_index = (m_write_index + 1) % m_queue_length;
        ++m_count;
    }
    m_not_empty_condition_variable.Signal();
    return {};
}

[[nodiscard]] auto RingBufferLockProtected::PopBack(
    uint8_t* const element, DurationUs timeout_duration) -> std::expected<void, PikaError>
{
    {
        auto locked_mutex_result = [&]() {
            return timeout_duration == pika::INFINITE_TIMEOUT
                ? LockedMutex::New(&m_mutex)
                : LockedMutex::New(&m_mutex, timeout_duration);
        }();
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }
        m_not_empty_condition_variable.Wait(
            locked_mutex_result.value(), [&]() -> bool { return m_count != 0; });
        std::memcpy(element, getBufferSlot(m_read_index), m_element_size_in_bytes);
        m_read_index = (m_read_index + 1) % m_queue_length;
        --m_count;
    }
    m_not_full_condition_variable.Signal();
    return {};
}

[[nodiscard]] auto RingBufferLockProtected::GetFrontElementPtr(DurationUs timeout_duration)
    -> std::expected<uint8_t* const, PikaError>
{
    if (timeout_duration == INFINITE_TIMEOUT) {
        auto lock_result = m_mutex.Lock();
        if (not lock_result.has_value()) {
            return std::unexpected { lock_result.error() };
        }
    } else {
        auto lock_result = m_mutex.LockTimed(timeout_duration);
        if (not lock_result.has_value()) {
            return std::unexpected { lock_result.error() };
        }
    }
    // Wait till we have a free slot to write to
    m_not_full_condition_variable.Wait(
        m_mutex, [this]() -> bool { return m_count < m_queue_length; });
    // We have exclusive access and have a free slot, return to caller to write into
    return getBufferSlot(m_write_index);
}

[[nodiscard]] auto RingBufferLockProtected::ReleaseFrontElementPtr(uint8_t const* const element)
    -> std::expected<void, PikaError>
{
    if (element != getBufferSlot(m_write_index)) {
        return std::unexpected { PikaError {
            .error_type = PikaErrorType::RingBufferError,
            .error_message
            = "Element pointer given to RingBufferLockProtected::ReleaseFrontElementPtr "
              "not the front pointer. Ensure that the pointer given to this function is "
              "the one obtained through RingBufferLockProtected::GetFrontElementPtr",
        } };
    }
    m_write_index = (m_write_index + 1) % m_queue_length;
    ++m_count;
    auto unlock_result = m_mutex.Unlock();
    if (not unlock_result.has_value()) {
        return std::unexpected { unlock_result.error() };
    }
    m_not_empty_condition_variable.Signal();
    return {};
}

[[nodiscard]] auto RingBufferLockProtected::GetBackElementPtr(DurationUs timeout_duration)
    -> std::expected<uint8_t const* const, PikaError>
{
    if (timeout_duration == INFINITE_TIMEOUT) {
        auto lock_result = m_mutex.Lock();
        if (not lock_result.has_value()) {
            return std::unexpected { lock_result.error() };
        }
    } else {
        auto lock_result = m_mutex.LockTimed(timeout_duration);
        if (not lock_result.has_value()) {
            return std::unexpected { lock_result.error() };
        }
    }
    // Wait till we have a slot to read from
    m_not_empty_condition_variable.Wait(m_mutex, [&]() -> bool { return m_count != 0; });
    return getBufferSlot(m_read_index);
}

[[nodiscard]] auto RingBufferLockProtected::ReleaseBackElementPtr(uint8_t const* const element)
    -> std::expected<void, PikaError>
{
    if (element != getBufferSlot(m_read_index)) {
        return std::unexpected { PikaError {
            .error_type = PikaErrorType::RingBufferError,
            .error_message
            = "Element pointer given to RingBufferLockProtected::ReleaseBackElementPtr "
              "not the front pointer. Ensure that the pointer given to this function is "
              "the one obtained through RingBufferLockProtected::GetBackElementPtr",
        } };
    }
    m_read_index = (m_read_index + 1) % m_queue_length;
    --m_count;
    auto unlock_result = m_mutex.Unlock();
    if (not unlock_result.has_value()) {
        return std::unexpected { unlock_result.error() };
    }
    m_not_full_condition_variable.Signal();
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

auto RingBufferLockFree::PushFront(uint8_t const* const element, DurationUs timeout_duration)
    -> std::expected<void, PikaError>
{
    auto const current_tail = m_tail.load(std::memory_order_relaxed);
    auto const next_tail = incrementByOne(current_tail);
    if (timeout_duration == pika::INFINITE_TIMEOUT) {
        while (next_tail == m_head.load(std::memory_order_acquire)) {
            // Busy wait; TODO: Detemine best strategy here
        }
    } else {
        Timer timer;
        while (next_tail == m_head.load(std::memory_order_acquire)
            && timer.GetElapsedDuration() < timeout_duration) {
            // Busy wait; TODO: Detemine best strategy here
        }
    }
    std::memcpy(getBufferSlot_(current_tail), element, m_element_size_in_bytes);
    m_tail.store(next_tail, std::memory_order_release);
    return {};
}

auto RingBufferLockFree::PopBack(uint8_t* const element, DurationUs timeout_duration)
    -> std::expected<void, PikaError>
{
    auto const current_head = m_head.load(std::memory_order_relaxed);
    if (timeout_duration == pika::INFINITE_TIMEOUT) {
        while (current_head == m_tail.load(std::memory_order_acquire)) {
            // Busy wait; TODO: Detemine best strategy here
        }

    } else {
        Timer timer;
        while (current_head == m_tail.load(std::memory_order_acquire)
            && timer.GetElapsedDuration() < timeout_duration) {
            // Busy wait; TODO: Detemine best strategy here
        }
    }
    std::memcpy(element, getBufferSlot_(current_head), m_element_size_in_bytes);
    m_head.store(incrementByOne(current_head), std::memory_order_release);
    return {};
}