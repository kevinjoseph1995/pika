#ifndef PIKA_SHARED_BUFFER_HPP
#define PIKA_SHARED_BUFFER_HPP

#include "error.hpp"

#include <expected>

class SharedBuffer {
public:
    SharedBuffer() = default;
    SharedBuffer(SharedBuffer const&) = delete;
    SharedBuffer(SharedBuffer&&);
    ~SharedBuffer();
    [[nodiscard]] auto Initialize(std::string const& identifier, uint64_t size) -> std::expected<void, PikaError>;

    [[nodiscard]] auto GetBuffer() const -> uint8_t*
    {
        PIKA_ASSERT(m_data != nullptr);
        return m_data;
    }

    [[nodiscard]] auto GetSize() const -> uint64_t
    {
        PIKA_ASSERT(m_data != nullptr);
        return m_size;
    }

private:
    std::string m_identifier;
    int32_t m_fd = -1;
    uint8_t* m_data = nullptr;
    uint64_t m_size = 0;
};

#endif