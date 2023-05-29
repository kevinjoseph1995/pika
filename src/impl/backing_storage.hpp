#ifndef PIKA_BACKING_STORAGE_HPP
#define PIKA_BACKING_STORAGE_HPP

#include "error.hpp"

#include <cstdint>
#include <expected>
#include <vector>

class InterProcessSharedBuffer {
public:
    InterProcessSharedBuffer() = default;
    InterProcessSharedBuffer(InterProcessSharedBuffer const&) = delete;
    InterProcessSharedBuffer(InterProcessSharedBuffer&&);
    void operator=(InterProcessSharedBuffer&&);
    ~InterProcessSharedBuffer();
    [[nodiscard]] auto Initialize(std::string const& identifier, uint64_t size)
        -> std::expected<void, PikaError>;

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

class InterThreadSharedBuffer {
public:
    [[nodiscard]] auto Initialize(std::string const& identifier, uint64_t size)
        -> std::expected<void, PikaError>;

    [[nodiscard]] auto GetBuffer() const -> uint8_t*
    {
        PIKA_ASSERT(m_data != nullptr);
        return m_data->data();
    }

    [[nodiscard]] auto GetSize() const -> uint64_t
    {
        PIKA_ASSERT(m_data != nullptr);
        return m_data->size();
    }

private:
    std::string m_identifier;
    std::vector<uint8_t>* m_data
        = nullptr; // TODO: Maybe have a ref-counted heap buffer. For now this pointer points to
                   // some vector with static lifetime.
};

#endif