// MIT License

// Copyright (c) 2023 Kevin Joseph

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include "backing_storage.hpp"

// Local includes
#include "error.hpp"
// System includes
#include <cstdint>
#include <errno.h>
#include <fcntl.h> /* For O_* constants */
#include <fmt/core.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>
#include <unordered_map>
#include <vector>

auto InterProcessSharedBuffer::Initialize(std::string const& identifier, uint64_t size)
    -> std::expected<void, PikaError>
{
    if (m_data != nullptr) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = "SharedBuffer::Initialize: Already initialized" });
    }
    if (identifier.at(0) != '/') {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = "SharedBuffer::Initialize: Shared memory "
                             "object must begin with a \"/\"" });
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
            .error_message
            = fmt::format("Shared memory object with identifier \"{}\" already exists;"
                          "however has size:{} whereas current request is for {} number of "
                          "bytes",
                identifier, stat.st_size, size) });
    }

    if (stat.st_size == 0) {
        ret_code = ftruncate(fd, static_cast<long>(size));
        if (ret_code != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
                .error_message = fmt::format("ftruncate failed with error:{}", error_message) });
        }
    }

    void* shared_memory_data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*reinterpret_cast<int32_t*>(shared_memory_data) == -1) {
        return std::unexpected(
            PikaError { .error_message = fmt::format("mmap error: {}", strerror(errno)) });
    }

    // Initialize all members
    m_fd = fd;
    m_identifier = identifier;
    m_size = size;
    m_data = static_cast<uint8_t*>(shared_memory_data);
    return {};
}

InterProcessSharedBuffer::~InterProcessSharedBuffer()
{
    if (m_data != nullptr) {
        auto result = munmap(m_data, m_size);
        if (result != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            fmt::println(stderr, "munmap failed with error:{}", m_identifier, error_message);
        }
        m_data = nullptr;
    }
    if (m_fd != -1) {
        auto result = shm_unlink(m_identifier.c_str());
        if (result != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            fmt::println(
                stderr, "shm_unlink({}) failed with error:{}", m_identifier, error_message);
        }
        m_fd = -1;
    }
    m_size = 0;
    m_identifier.resize(0);
}

InterProcessSharedBuffer::InterProcessSharedBuffer(InterProcessSharedBuffer&& other)
{
    m_identifier = other.m_identifier;
    m_fd = other.m_fd;
    m_data = other.m_data;
    m_size = other.m_size;
    other.m_identifier.clear();
    other.m_fd = -1;
    other.m_data = nullptr;
    other.m_size = 0;
}

void InterProcessSharedBuffer::operator=(InterProcessSharedBuffer&& other)
{
    m_identifier = other.m_identifier;
    m_fd = other.m_fd;
    m_data = other.m_data;
    m_size = other.m_size;
    other.m_identifier.clear();
    other.m_fd = -1;
    other.m_data = nullptr;
    other.m_size = 0;
}

auto InterThreadSharedBuffer::Initialize(std::string const& identifier, uint64_t size)
    -> std::expected<void, PikaError>
{
    struct __InternalBlock {
        std::mutex map_mutex;
        std::unordered_map<std::string, std::vector<uint8_t>> buffer_map;
    };

    static auto internal_map = __InternalBlock {};

    std::scoped_lock lk { internal_map.map_mutex };
    if (internal_map.buffer_map.count(identifier) != 0) {
        m_data = &internal_map.buffer_map.at(identifier);
        m_identifier = identifier;
    } else {
        auto [pair, buffer] = internal_map.buffer_map.insert(
            std::make_pair(identifier, std::vector<uint8_t>(static_cast<unsigned long>(size), 0)));
        m_identifier = pair->first;
        m_data = &pair->second;
    }
    return {};
}