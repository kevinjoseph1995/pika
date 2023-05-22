#include "ring_buffer.hpp"

auto RingBuffer::GetRingBufferOffset(uint64_t element_alignment) -> uint64_t
{
    if (element_alignment < sizeof(Header)) {
        return ((sizeof(Header) / element_alignment) + 1) * element_alignment;
    } else {
        return element_alignment;
    }
}

auto RingBuffer::GetRequiredBufferSize(uint64_t element_alignment, uint64_t element_size, uint64_t number_of_elements) -> uint64_t
{
    return GetRingBufferOffset(element_alignment) + (number_of_elements * element_size);
}

auto RingBuffer::GetAlignmentRequirement() -> uint64_t
{
    return std::alignment_of<Header>();
}

auto RingBuffer::Initialize(uint8_t* ring_buffer, uint64_t element_size, uint64_t element_alignment, uint64_t number_of_elements, bool is_intra_process) -> std::expected<void, PikaError>
{
    if (ring_buffer == nullptr) {
        return std::unexpected(PikaError {
            .error_type = PikaErrorType::SharedRingBufferError,
            .error_message = "SharedRingBuffer::Initialize buffer==nullptr" });
    }

    if (reinterpret_cast<std::uintptr_t>(ring_buffer) % GetAlignmentRequirement() != 0) {
        return std::unexpected(PikaError {
            .error_type = PikaErrorType::SharedRingBufferError,
            .error_message = "SharedRingBuffer::Initialize buffer is not aligned" });
    }

    m_ring_buffer = ring_buffer;
    m_element_alignment = element_alignment;
    m_element_size = element_size;
    m_max_number_of_elements = number_of_elements;

    auto result = m_header.mutex.Initialize(is_intra_process);
    if (not result.has_value()) {
        return std::unexpected(result.error());
    }
    result = m_header.not_empty_condition_variable.Initialize(is_intra_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_empty_condition_variable");
        return std::unexpected(result.error());
    }
    result = m_header.not_full_condition_variable.Initialize(is_intra_process);
    if (not result.has_value()) {
        result.error().error_message.append("| not_full_condition_variable");
        return std::unexpected(result.error());
    }

    return {};
}

[[nodiscard]] auto RingBuffer::GetWriteSlot() -> std::expected<WriteSlot, PikaError>
{
    auto locked_mutex_result = LockedMutex::New(&m_header.mutex);
    if (not locked_mutex_result.has_value()) {
        return std::unexpected { locked_mutex_result.error() };
    }

    m_header.not_full_condition_variable.Wait(locked_mutex_result.value(),
        [this]() -> bool {
            return m_header.count < m_max_number_of_elements;
        });

    auto write_slot = WriteSlot {
        std::move((locked_mutex_result.value())),
        &(m_header.not_empty_condition_variable),
        getBufferSlot(m_header.write_index)
    };
    m_header.write_index = (m_header.write_index + 1) % m_max_number_of_elements;
    ++m_header.count;
    return write_slot;
}

[[nodiscard]] auto RingBuffer::GetReadSlot() -> std::expected<ReadSlot, PikaError>
{
    auto locked_mutex_result = LockedMutex::New(&m_header.mutex);
    if (not locked_mutex_result.has_value()) {
        return std::unexpected { locked_mutex_result.error() };
    }

    m_header.not_empty_condition_variable.Wait(locked_mutex_result.value(),
        [&]() -> bool {
            return m_header.count != 0;
        });

    auto read_slot = ReadSlot {
        std::move((locked_mutex_result.value())),
        &(m_header.not_full_condition_variable),
        getBufferSlot(m_header.read_index)
    };
    m_header.read_index = (m_header.read_index + 1) % m_max_number_of_elements;
    --m_header.count;
    return read_slot;
}