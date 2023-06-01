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

#include "error.hpp"
#ifdef ENABLE_BACKTRACE
// clang-format off
#include <backward.hpp>
// clang-format on
#endif
#include <exception>
#include <fmt/core.h>

void PrintAssertionMessage(
    char const* file, int line, char const* function_name, char const* message)
{
    if (message == nullptr) {
        fmt::print(stderr, "Assertion failed at {}:{} in {}", file, line, function_name);
    } else {
        fmt::print(stderr, "Assertion failed at {}:{} in FUNC:\"{}\" with MESSAGE:\"{}\"", file,
            line, function_name, message);
    }
}

auto PrintBackTrace() -> void
{
#ifdef ENABLE_BACKTRACE
    using namespace backward;
    StackTrace st;
    st.load_here(8);
    Printer p;
    p.print(st);
#endif
}