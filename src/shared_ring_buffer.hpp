#ifndef PIKA_SHARED_RING_BUFFER_HPP
#define PIKA_SHARED_RING_BUFFER_HPP

#include "error.hpp"
#include "shared_buffer.hpp"
#include "synchronization_primitives.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <type_traits>

template<typename ElementType>
struct WriteSlot {
    WriteSlot(LockedMutex locked_mutex, ElementType& element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
    {
    }
    auto GetElement() const -> ElementType& { return m_element; }

protected:
    ElementType& m_element;

private:
    LockedMutex m_locked_mutex;
};

template<typename ElementType>
struct ReadSlot {
    ReadSlot(LockedMutex locked_mutex, ElementType const& element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
    {
    }
    auto GetElement() const -> ElementType const& { return m_element; }

protected:
    ElementType const& m_element;

private:
    LockedMutex m_locked_mutex;
};

struct Header {
    InterProcessMutex mutex;
    bool initialized = false;
    uint64_t number_of_elements = 0;
    uint64_t write_index = 0;
    uint64_t read_index = 0;
    uint64_t count = 0;
};

template<typename ElementType>
struct SharedRingBuffer {
    static_assert(std::is_trivial_v<ElementType>);
    static constexpr auto RING_BUFFER_START_OFFSET = []() {
        if constexpr (alignof(ElementType) < sizeof(Header)) {
            return ((sizeof(Header) / alignof(ElementType)) + 1) * alignof(ElementType);
        } else {
            return alignof(ElementType);
        }
    }();
    static_assert((RING_BUFFER_START_OFFSET % alignof(ElementType)) == 0);

    [[nodiscard]] auto Initialize(uint64_t number_of_elements) -> std::expected<void, PikaError>
    {
        auto const total_shared_buffer_size = RING_BUFFER_START_OFFSET + (number_of_elements * sizeof(ElementType));
        auto result = m_shared_buffer.Initialize(total_shared_buffer_size);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }

        auto header_location = new (getHeaderPtr()) Header();
        result = header_location->mutex.Initialize();
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        header_location->number_of_elements = number_of_elements;
        header_location->initialized = true;
        return {};
    }
    [[nodiscard]] auto GetWriteSlot() -> std::expected<WriteSlot<ElementType>, PikaError>
    {
        if (not getHeader().initialized) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = "SharedRingBuffer::GetWriteSlot Uninitialized" }
            };
        }
        auto locked_mutex_result = LockedMutex::New(getHeader().mutex);
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }
        if (getHeader().count == getHeader().number_of_elements) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = "SharedRingBuffer::GetWriteSlot Ring buffer full" }
            };
        }
        auto write_slot = WriteSlot<ElementType> { std::move((locked_mutex_result.value())), getBufferSlot(getHeader().write_index) };
        getHeader().write_index = (getHeader().write_index + 1) % getHeader().number_of_elements;
        ++getHeader().count;
        return write_slot;
    }
    [[nodiscard]] auto GetReadSlot() -> std::expected<ReadSlot<ElementType>, PikaError>
    {
        if (not getHeader().initialized) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = "SharedRingBuffer::GetReadSlot Uninitialized" }
            };
        }
        auto locked_mutex_result = LockedMutex::New(getHeader().mutex);
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }
        if (getHeader().read_index == getHeader().write_index) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = "SharedRingBuffer::GetReadSlot Ring buffer empty" }
            };
        }
        auto read_slot = ReadSlot<ElementType> { std::move((locked_mutex_result.value())), getBufferSlot(getHeader().read_index) };
        getHeader().read_index = (getHeader().read_index + 1) % getHeader().number_of_elements;
        --getHeader().count;
        return read_slot;
    }
    ~SharedRingBuffer()
    {
        if (getHeader().initialized) {
            getHeader().~Header();
        }
    }

private:
    [[nodiscard]] auto getHeader() -> Header&
    {
        PIKA_ASSERT(getHeaderPtr()->initialized);
        return *getHeaderPtr();
    }
    [[nodiscard]] auto getHeaderPtr() -> Header*
    {
        return reinterpret_cast<Header*>(m_shared_buffer.GetBuffer());
    }
    [[nodiscard]] auto getBufferSlot(uint64_t index) -> ElementType&
    {
        PIKA_ASSERT(getHeader().initialized);
        return *reinterpret_cast<ElementType*>(m_shared_buffer.GetBuffer() + RING_BUFFER_START_OFFSET + index * sizeof(ElementType));
    }

    SharedBuffer m_shared_buffer;
};

#endif