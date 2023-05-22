// #ifndef PIKA_SHARED_RING_BUFFER_LOCKFREE_HPP
// #define PIKA_SHARED_RING_BUFFER_LOCKFREE_HPP

// #include "error.hpp"
// #include "shared_buffer.hpp"
// #include "synchronization_primitives.hpp"

// #include <atomic>
// #include <cstdint>
// #include <fmt/core.h>
// #include <memory>

// template<typename ElementType>
// struct SharedRingBufferLockFree {
//     struct Header {
//         static constexpr auto MAX_NUMBER_OF_ENDPOINTS = 2;
//         std::atomic_uint64_t tail = 0;
//         std::atomic_uint64_t head = 0;
//     };
//     static constexpr auto RING_BUFFER_START_OFFSET = []() {
//         if constexpr (alignof(ElementType) < sizeof(Header)) {
//             return ((sizeof(Header) / alignof(ElementType)) + 1) * alignof(ElementType);
//         } else {
//             return alignof(ElementType);
//         }
//     }();
//     [[nodiscard]] auto Initialize(uint64_t number_of_elements) -> std::expected<void, PikaError>
//     {
//         m_capacity = number_of_elements + 1;

//         m_shared_buffer = std::make_unique<SharedBuffer>();

//         auto const total_shared_buffer_size = RING_BUFFER_START_OFFSET + (m_capacity * sizeof(ElementType));
//         auto result = m_shared_buffer->Initialize(total_shared_buffer_size);
//         if (not result.has_value()) {
//             return std::unexpected(result.error());
//         }

//         auto _ [[maybe_unused]] = new (getHeaderPtr()) Header();

//         m_initialized = true;
//         return {};
//     };
//     [[nodiscard]] auto Push(ElementType const& element) -> bool
//     {
//         auto const current_tail = getHeader().tail.load(std::memory_order_relaxed);
//         auto const next_tail = increment(current_tail);
//         if (next_tail != getHeader().head.load(std::memory_order_acquire)) {
//             getBufferSlot(current_tail) = element;
//             getHeader().tail.store(next_tail, std::memory_order_release);
//             return true;
//         }
//         return false;
//     }
//     [[nodiscard]] auto Pop(ElementType& element) -> bool
//     {
//         auto const current_head = getHeader().head.load(std::memory_order_relaxed);
//         if (current_head == getHeader().tail.load(std::memory_order_acquire)) {
//             return false;
//         }
//         element = getBufferSlot(current_head);
//         getHeader().head.store(increment(current_head), std::memory_order_release);
//         return true;
//     }

// private:
//     auto increment(uint64_t idx) const -> uint64_t
//     {
//         return (idx + 1) % m_capacity;
//     }
//     [[nodiscard]] auto wasEmpty() -> bool
//     {
//         return getHeader().head.load() == getHeader().tail.load();
//     }

//     [[nodiscard]] auto getHeader() -> Header&
//     {
//         PIKA_ASSERT(m_initialized);
//         return *getHeaderPtr();
//     }
//     [[nodiscard]] auto getHeaderPtr() -> Header*
//     {
//         return reinterpret_cast<Header*>(m_shared_buffer->GetBuffer());
//     }
//     [[nodiscard]] auto getBufferSlot(uint64_t index) -> ElementType&
//     {
//         PIKA_ASSERT(m_initialized);
//         return *reinterpret_cast<ElementType*>(m_shared_buffer->GetBuffer() + RING_BUFFER_START_OFFSET + index * sizeof(ElementType));
//     }
//     std::unique_ptr<SharedBuffer> m_shared_buffer;
//     uint64_t m_capacity = 0;
//     bool m_initialized = false;
// };

// #endif