#include "shared_buffer.hpp"

// Local includes
#include "error.hpp"
// System includes
#include <cstdint>
#include <errno.h>
#include <fcntl.h> /* For O_* constants */
#include <fmt/core.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>

auto SharedBuffer::Initialize(std::string const& identifier, uint64_t size) -> std::expected<void, PikaError>
{
    if (m_data != nullptr) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = "SharedBuffer::Initialize: Already initialized" });
    }
    if (identifier.at(0) != '/') {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = "SharedBuffer::Initialize: Shared memory object must begin with a \"/\"" });
    }
    auto fd = shm_open(identifier.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        auto error_message = strerror(errno);
        errno = 0;
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = fmt::format("shm_open error: {}", error_message) });
    }

    struct stat stat { };
    auto ret_code = fstat(fd, &stat);
    if (ret_code != 0) {
        auto error_message = strerror(errno);
        errno = 0;
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = fmt::format("fstat error: {}", error_message) });
    }

    if (stat.st_size != 0 && stat.st_size != static_cast<decltype(stat.st_size)>(size)) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = fmt::format("Shared memory object with identifier \"{}\" already exists;"
                                         "however has size:{} whereas current request is for {} number of bytes",
                identifier, stat.st_size, size) });
    }

    if (stat.st_size == 0) {
        ret_code = ftruncate(fd, static_cast<long>(size));
        if (ret_code != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
                .error_message = fmt::format("ftruncate failed with error:{}",
                    error_message) });
        }
    }

    void* shared_memory_data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*reinterpret_cast<int32_t*>(shared_memory_data) == -1) {
        return std::unexpected(PikaError { .error_message = fmt::format("mmap error: {}", strerror(errno)) });
    }

    // Initialize all members
    m_fd = fd;
    m_identifier = identifier;
    m_size = size;
    m_data = static_cast<uint8_t*>(shared_memory_data);
    return {};
}

SharedBuffer::~SharedBuffer()
{
    if (m_fd != -1) {
        auto result = shm_unlink(m_identifier.c_str());
        if (result != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            fmt::println(stderr, "shm_unlink({}) failed with error:{}", m_identifier, error_message);
        }
        m_fd = -1;
    }
    if (m_data != nullptr) {
        auto result = munmap(m_data, m_size);
        if (result != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            fmt::println(stderr, "munmap failed with error:{}", m_identifier, error_message);
        }
        m_data = nullptr;
    }
    m_size = 0;
    m_identifier.resize(0);
}

SharedBuffer::SharedBuffer(SharedBuffer&& other)
{
    m_identifier = other.m_identifier;
    m_fd = other.m_fd;
    m_data = other.m_data;
    m_size = other.m_size;
    other.m_identifier = "";
    other.m_fd = -1;
    other.m_data = nullptr;
    other.m_size = 0;
}