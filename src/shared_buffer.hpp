#ifndef PIKA_SHARED_BUFFER_HPP
#define PIKA_SHARED_BUFFER_HPP

#include "error.hpp"
#include "synchronization_primitives.hpp"

#include <cstdint>
#include <expected>
#include <span>

class SharedBuffer {
public:
    SharedBuffer() = default;
    SharedBuffer(SharedBuffer const&) = delete;
    SharedBuffer& operator=(SharedBuffer const&) = delete;
    SharedBuffer(SharedBuffer&&);
    SharedBuffer& operator=(SharedBuffer&&);
    ~SharedBuffer();

    [[nodiscard]] auto Initialize(uint64_t size) -> std::expected<void, PikaError>;
    [[nodiscard]] auto GetBuffer() const -> u_int8_t*
    {
        return m_data;
    }
    [[nodiscard]] auto GetSize() const -> uint64_t
    {
        return m_size;
    }
    auto Leak()
    {
        m_data = nullptr;
        m_size = 0;
    }

private:
    uint8_t* m_data = nullptr;
    uint64_t m_size = 0;
};

#endif