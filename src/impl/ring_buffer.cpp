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

auto RingBufferLockProtected::initialize(RingBufferLockProtected& ring_buffer_object,
    uint8_t* ring_buffer, uint64_t element_size, uint64_t element_alignment,
    uint64_t number_of_elements, bool is_inter_process) -> std::expected<void, PikaError>
{
    if (ring_buffer == nullptr) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedRingBufferError,
            .error_message = "SharedRingBuffer::Initialize buffer==nullptr" });
    }

    if (reinterpret_cast<std::uintptr_t>(ring_buffer) % element_alignment != 0) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedRingBufferError,
            .error_message = "SharedRingBuffer::Initialize buffer is not aligned" });
    }

    ring_buffer_object.m_ring_buffer = ring_buffer;
    ring_buffer_object.m_element_alignment = element_alignment;
    ring_buffer_object.m_element_size_in_bytes = element_size;
    ring_buffer_object.m_queue_length = number_of_elements;

    auto result = ring_buffer_object.m_header.mutex.Initialize(is_inter_process);
    if (not result.has_value()) {
        return std::unexpected(result.error());
    }
    result = ring_buffer_object.m_header.not_empty_condition_variable.Initialize(is_inter_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_empty_condition_variable");
        return std::unexpected(result.error());
    }
    result = ring_buffer_object.m_header.not_full_condition_variable.Initialize(is_inter_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_full_condition_variable");
        return std::unexpected(result.error());
    }

    return {};
}

[[nodiscard]] auto RingBufferLockProtected::GetWriteSlot() -> std::expected<WriteSlot, PikaError>
{
    auto locked_mutex_result = LockedMutex::New(&m_header.mutex);
    if (not locked_mutex_result.has_value()) {
        return std::unexpected { locked_mutex_result.error() };
    }

    m_header.not_full_condition_variable.Wait(
        locked_mutex_result.value(), [this]() -> bool { return m_header.count < m_queue_length; });

    auto write_slot = WriteSlot { std::move((locked_mutex_result.value())),
        &(m_header.not_empty_condition_variable), getBufferSlot(m_header.write_index) };
    m_header.write_index = (m_header.write_index + 1) % m_queue_length;
    ++m_header.count;
    return write_slot;
}

[[nodiscard]] auto RingBufferLockProtected::GetReadSlot() -> std::expected<ReadSlot, PikaError>
{
    auto locked_mutex_result = LockedMutex::New(&m_header.mutex);
    if (not locked_mutex_result.has_value()) {
        return std::unexpected { locked_mutex_result.error() };
    }

    m_header.not_empty_condition_variable.Wait(
        locked_mutex_result.value(), [&]() -> bool { return m_header.count != 0; });

    auto read_slot = ReadSlot { std::move((locked_mutex_result.value())),
        &(m_header.not_full_condition_variable), getBufferSlot(m_header.read_index) };
    m_header.read_index = (m_header.read_index + 1) % m_queue_length;
    --m_header.count;
    return read_slot;
}

WriteSlot ::~WriteSlot()
{
    if (m_cv != nullptr) {
        m_locked_mutex.~LockedMutex();
        m_cv->Signal();
        m_cv = nullptr;
    }
}

void WriteSlot::operator=(WriteSlot&& other)
{
    m_element = other.m_element;
    m_locked_mutex = std::move(other.m_locked_mutex);
    m_cv = other.m_cv;
    other.m_cv = nullptr;
}

WriteSlot::WriteSlot(WriteSlot&& other)
    : m_element(other.m_element)
    , m_locked_mutex(std::move(other.m_locked_mutex))
    , m_cv(other.m_cv)
{
    other.m_cv = nullptr;
    other.m_element = nullptr;
}

ReadSlot::~ReadSlot()
{
    if (m_cv != nullptr) {
        m_locked_mutex.~LockedMutex();
        m_cv->Signal();
        m_cv = nullptr;
    }
}
ReadSlot::ReadSlot(ReadSlot&& other)
    : m_element(other.m_element)
    , m_locked_mutex(std::move(other.m_locked_mutex))
    , m_cv(other.m_cv)
{
    other.m_cv = nullptr;
    other.m_element = nullptr;
}
void ReadSlot::operator=(ReadSlot&& other)
{
    m_element = other.m_element;
    m_locked_mutex = std::move(other.m_locked_mutex);
    m_cv = other.m_cv;
    other.m_cv = nullptr;
}