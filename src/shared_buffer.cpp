#include "shared_buffer.hpp"

// Local includes
#include "error.hpp"
// System includes
#include <cstdio>
#include <errno.h>
#include <fmt/core.h>
#include <sys/mman.h>

SharedBuffer::SharedBuffer(SharedBuffer&& other)
{
    this->m_data = other.m_data;
    this->m_size = other.m_size;
    other.m_data = nullptr;
    other.m_size = 0;
}

SharedBuffer& SharedBuffer::operator=(SharedBuffer&& other)
{
    this->m_data = other.m_data;
    this->m_size = other.m_size;
    other.m_data = nullptr;
    other.m_size = 0;
    return *this;
}

auto SharedBuffer::Initialize(uint64_t size) -> std::expected<void, PikaError>
{
    if (m_data != nullptr) {
        return std::unexpected(PikaError { .error_message = "SharedBuffer::Initialize: Already initialized" });
    }
    void* shared_memory_data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (*reinterpret_cast<int32_t*>(shared_memory_data) == -1) {
        return std::unexpected(PikaError { .error_message = fmt::format("mmap error: {}", strerror(errno)) });
        errno = 0;
    }
    m_data = reinterpret_cast<uint8_t*>(shared_memory_data);
    m_size = size;
    return {};
}

SharedBuffer::~SharedBuffer()
{
    if (m_data != nullptr) {
        if (munmap(m_data, m_size) != 0) {
            fmt::println(stderr, "munmap failed with error:{}", strerror(errno));
            errno = 0;
        }
    }
}