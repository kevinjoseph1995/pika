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
#ifndef PIKA_ERROR_HPP
#define PIKA_ERROR_HPP

#include <string>

auto PrintAssertionMessage(
    char const* file, int line, char const* function_name, char const* message = nullptr) -> void;
auto PrintBackTrace() -> void;

#define PIKA_ASSERT(expr, ...)                                                                     \
    do {                                                                                           \
        if ((!(expr))) [[unlikely]] {                                                              \
            PrintAssertionMessage(__FILE__, __LINE__, __func__ __VA_OPT__(, ) __VA_ARGS__);        \
            PrintBackTrace();                                                                      \
            __builtin_trap();                                                                      \
        }                                                                                          \
    } while (0)

#define TODO(message) PIKA_ASSERT(false, message)

enum class PikaErrorType {
    Unknown,
    SharedBufferError,
    SyncPrimitiveError,
    RingBufferError,
    ChannelError,
    Timeout
};

struct PikaError {
    PikaErrorType error_type = PikaErrorType::Unknown;
    std::string error_message;
};

#endif