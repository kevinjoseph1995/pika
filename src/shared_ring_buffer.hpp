#ifndef PIKA_SHARED_RING_BUFFER_HPP
#define PIKA_SHARED_RING_BUFFER_HPP

#include "error.hpp"
#include "fmt/core.h"
#include "shared_buffer.hpp"
#include "synchronization_primitives.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <fmt/ranges.h>
#include <memory>
#include <ranges>
#include <span>
#include <sys/types.h>
#include <type_traits>
#include <unistd.h>

template<typename ElementType>
struct SharedRingBuffer;

template<typename ElementType>
struct WriteSlot {
    auto GetElement() const -> ElementType& { return *m_element; }
    ~WriteSlot()
    {
        if (m_cv != nullptr) {
            m_locked_mutex.~LockedMutex();
            m_cv->Signal();
            m_cv = nullptr;
        }
    }
    WriteSlot(WriteSlot&& other)
        : m_element(other.m_element)
        , m_locked_mutex(std::move(other.m_locked_mutex))
        , m_cv(other.m_cv)
    {
        other.m_cv = nullptr;
    }
    WriteSlot& operator=(WriteSlot&& other)
    {
        m_element = other.m_element;
        m_locked_mutex = std::move(other.m_locked_mutex);
        m_cv = other.m_cv;
        other.m_cv = nullptr;
        return *this;
    }

private:
    WriteSlot(LockedMutex locked_mutex, InterProcessConditionVariable* cv, ElementType* element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
        , m_cv(cv)
    {
    }
    friend struct SharedRingBuffer<ElementType>;
    ElementType* m_element = nullptr;
    LockedMutex m_locked_mutex;
    InterProcessConditionVariable* m_cv = nullptr;
};

template<typename ElementType>
struct ReadSlot {
    auto GetElement() const -> ElementType const& { return *m_element; }
    ~ReadSlot()
    {
        if (m_cv != nullptr) {
            m_locked_mutex.~LockedMutex();
            m_cv->Signal();
            m_cv = nullptr;
        }
    }
    ReadSlot(ReadSlot&& other)
        : m_element(other.m_element)
        , m_locked_mutex(std::move(other.m_locked_mutex))
        , m_cv(other.m_cv)
    {
        other.m_cv = nullptr;
    }
    ReadSlot& operator=(ReadSlot&& other)
    {
        m_element = other.m_element;
        m_locked_mutex = std::move(other.m_locked_mutex);
        m_cv = other.m_cv;
        other.m_cv = nullptr;
        return *this;
    }

protected:
    ReadSlot(LockedMutex locked_mutex, InterProcessConditionVariable* cv, ElementType const* element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
        , m_cv(cv)
    {
    }
    friend struct SharedRingBuffer<ElementType>;
    ElementType const* m_element = nullptr;
    LockedMutex m_locked_mutex;
    InterProcessConditionVariable* m_cv = nullptr;
};

template<typename ElementType>
struct SharedRingBuffer {
    struct Header {
        static constexpr auto MAX_NUMBER_OF_ENDPOINTS = 2;
        InterProcessMutex mutex;
        InterProcessConditionVariable not_empty_condition_variable;
        InterProcessConditionVariable not_full_condition_variable;
        bool initialized = false;
        uint64_t max_number_of_elements = 0;
        uint64_t write_index = 0;
        uint64_t read_index = 0;
        uint64_t count = 0;
        std::array<pid_t, MAX_NUMBER_OF_ENDPOINTS> registered_endpoints {};
    };
    static_assert(std::is_trivial_v<ElementType>);
    static constexpr auto RING_BUFFER_START_OFFSET = []() {
        if constexpr (alignof(ElementType) < sizeof(Header)) {
            return ((sizeof(Header) / alignof(ElementType)) + 1) * alignof(ElementType);
        } else {
            return alignof(ElementType);
        }
    }();
    static_assert((RING_BUFFER_START_OFFSET % alignof(ElementType)) == 0);
    auto RegisterEndpoint() -> std::expected<void, PikaError>
    {
        auto process_id = getpid();
        if (not getHeader().initialized) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = "SharedRingBuffer::RegisterEndpoint Uninitialized" }
            };
        }
        for (size_t i = 0; i < Header::MAX_NUMBER_OF_ENDPOINTS; ++i) {
            if (getHeader().registered_endpoints[i] == pid_t {} || getHeader().registered_endpoints[i] == process_id) {
                getHeader().registered_endpoints[i] = process_id;
                return {};
            }
        }
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = "SharedRingBuffer::RegisterEndpoint trying to register too many endpoints" }
        };
    }
    [[nodiscard]] auto Initialize(uint64_t number_of_elements) -> std::expected<void, PikaError>
    {
        m_shared_buffer = std::make_unique<SharedBuffer>();
        auto const total_shared_buffer_size = RING_BUFFER_START_OFFSET + (number_of_elements * sizeof(ElementType));
        auto result = m_shared_buffer->Initialize(total_shared_buffer_size);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }

        auto header_location = new (getHeaderPtr()) Header();
        result = header_location->mutex.Initialize();
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        result = header_location->not_empty_condition_variable.Initialize();
        if (not result.has_value()) {
            result.error().error_message.append("| not_empty_condition_variable");
            return std::unexpected(result.error());
        }
        result = header_location->not_full_condition_variable.Initialize();
        if (not result.has_value()) {
            result.error().error_message.append("| not_full_condition_variable");
            return std::unexpected(result.error());
        }
        header_location->max_number_of_elements = number_of_elements;
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
        auto locked_mutex_result = LockedMutex::New(&getHeader().mutex);
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }

        getHeader().not_full_condition_variable.Wait(locked_mutex_result.value(),
            [this]() -> bool {
                return getHeader().count < getHeader().max_number_of_elements;
            });

        auto write_slot = WriteSlot<ElementType> {
            std::move((locked_mutex_result.value())),
            &(getHeader().not_empty_condition_variable),
            &(getBufferSlot(getHeader().write_index))
        };
        getHeader().write_index = (getHeader().write_index + 1) % getHeader().max_number_of_elements;
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
        auto locked_mutex_result = LockedMutex::New(&getHeader().mutex);
        if (not locked_mutex_result.has_value()) {
            return std::unexpected { locked_mutex_result.error() };
        }

        getHeader().not_empty_condition_variable.Wait(locked_mutex_result.value(),
            [&]() -> bool {
                return getHeader().count != 0;
            });

        auto read_slot = ReadSlot<ElementType> {
            std::move((locked_mutex_result.value())),
            &(getHeader().not_full_condition_variable),
            &(getBufferSlot(getHeader().read_index))
        };
        getHeader().read_index = (getHeader().read_index + 1) % getHeader().max_number_of_elements;
        --getHeader().count;
        return read_slot;
    }
    ~SharedRingBuffer()
    {
        if (getHeader().initialized) {
            int number_of_processes_registered = 0;
            for (size_t i = 0; i < Header::MAX_NUMBER_OF_ENDPOINTS; ++i) {
                if (getHeader().registered_endpoints[i] != pid_t {}) {
                    ++number_of_processes_registered;
                }
                if (getHeader().registered_endpoints[i] == getpid()) {
                    getHeader().registered_endpoints[i] = pid_t {};
                }
            }
            if (number_of_processes_registered == 1) {
                getHeader().~Header();
                m_shared_buffer.reset(nullptr);
            } else {
                m_shared_buffer->Leak();
            }
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
        return reinterpret_cast<Header*>(m_shared_buffer->GetBuffer());
    }
    [[nodiscard]] auto getBufferSlot(uint64_t index) -> ElementType&
    {
        PIKA_ASSERT(getHeader().initialized);
        return *reinterpret_cast<ElementType*>(m_shared_buffer->GetBuffer() + RING_BUFFER_START_OFFSET + index * sizeof(ElementType));
    }

    std::unique_ptr<SharedBuffer> m_shared_buffer;
};

#endif