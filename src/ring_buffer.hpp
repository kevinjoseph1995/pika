#ifndef PIKA_SHARED_RING_BUFFER_HPP
#define PIKA_SHARED_RING_BUFFER_HPP

#include "error.hpp"
#include "shared_buffer.hpp"
#include "synchronization_primitives.hpp"
// System includes
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

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
    WriteSlot(LockedMutex locked_mutex, ConditionVariable* cv, ElementType* element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
        , m_cv(cv)
    {
    }
    friend struct SharedRingBuffer<ElementType>;
    ElementType* m_element = nullptr;
    LockedMutex m_locked_mutex;
    ConditionVariable* m_cv = nullptr;
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
    ReadSlot(LockedMutex locked_mutex, ConditionVariable* cv, ElementType const* element)
        : m_element(element)
        , m_locked_mutex(std::move(locked_mutex))
        , m_cv(cv)
    {
    }
    friend struct SharedRingBuffer<ElementType>;
    ElementType const* m_element = nullptr;
    LockedMutex m_locked_mutex;
    ConditionVariable* m_cv = nullptr;
};

template<typename ElementType>
struct RingBuffer {
private:
    struct Header {
        std::atomic_bool initialized = false;
        Mutex mutex; // Coarse grained lock protecting all accesses to the buffer
        ConditionVariable not_empty_condition_variable;
        ConditionVariable not_full_condition_variable;
        uint64_t max_number_of_elements = 0;
        uint64_t write_index = 0;
        uint64_t read_index = 0;
        uint64_t count = 0;
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

public:
    static constexpr auto GetRequiredBufferSize(uint64_t number_of_elements) -> size_t
    {
        return RING_BUFFER_START_OFFSET + (number_of_elements * sizeof(ElementType));
    }

    static constexpr auto GetAlignmentRequirement() -> size_t
    {
        return std::alignment_of<Header>();
    }

    [[nodiscard]] static auto IsInitialized(uint8_t* buffer) -> bool
    {
        return getHeaderPtr(buffer)->initialized.load();
    }

    [[nodiscard]] auto Initialize(uint8_t* buffer, uint64_t number_of_elements, bool intra_process) -> std::expected<void, PikaError>
    {
        if (buffer == nullptr) {
            return std::unexpected(PikaError {
                .error_type = PikaErrorType::SharedRingBufferError,
                .error_message = "SharedRingBuffer::Initialize buffer==nullptr" });
        }

        if (reinterpret_cast<std::uintptr_t>(buffer) % GetAlignmentRequirement() != 0) {
            return std::unexpected(PikaError {
                .error_type = PikaErrorType::SharedRingBufferError,
                .error_message = "SharedRingBuffer::Initialize buffer is not aligned" });
        }

        m_buffer = buffer;

        auto header_location = new (getHeaderPtr()) Header();
        auto result = header_location->mutex.Initialize(intra_process);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        result = header_location->not_empty_condition_variable.Initialize(intra_process);
        if (not result.has_value()) {
            result.error().error_message.append("| not_empty_condition_variable");
            return std::unexpected(result.error());
        }
        result = header_location->not_full_condition_variable.Initialize(intra_process);
        if (not result.has_value()) {
            result.error().error_message.append("| not_full_condition_variable");
            return std::unexpected(result.error());
        }
        header_location->max_number_of_elements = number_of_elements;
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

    ~RingBuffer()
    {
        if (m_buffer) {
            getHeader().~Header();
            m_buffer = nullptr;
        }
    }

private:
    [[nodiscard]] auto getHeader() -> Header&
    {
        return *getHeaderPtr();
    }
    [[nodiscard]] auto getHeaderPtr() -> Header*
    {
        PIKA_ASSERT(m_buffer != nullptr);
        return getHeaderPtr(m_buffer);
    }
    [[nodiscard]] static auto getHeaderPtr(uint8_t* ptr) -> Header*
    {
        PIKA_ASSERT(ptr != nullptr);
        return reinterpret_cast<Header*>(ptr);
    }
    [[nodiscard]] auto getBufferSlot(uint64_t index) -> ElementType&
    {
        PIKA_ASSERT(getHeader().initialized);
        return *reinterpret_cast<ElementType*>(m_buffer + RING_BUFFER_START_OFFSET + index * sizeof(ElementType));
    }

    uint8_t* m_buffer = nullptr;
};

#endif