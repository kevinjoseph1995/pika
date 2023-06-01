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