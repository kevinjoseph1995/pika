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

#ifndef PIKA_PROCESS_FORK_HPP
#define PIKA_PROCESS_FORK_HPP

// Local includes
#include "error.hpp"
// System includes
#include <expected>
#include <fmt/core.h>
#include <functional>
#include <sys/types.h>
#include <unistd.h>

enum ChildProcessState { SUCCESS, FAIL };

template <typename F>
concept ChildProcessFunction = requires(F f) {
    {
        f()
    } -> std::same_as<ChildProcessState>;
};

struct ChildProcessHandle {
    template <ChildProcessFunction F>
    static auto RunChildFunction(F child_process_function)
        -> std::expected<ChildProcessHandle, PikaError>
    {
        auto status = fork();
        if (status == 0) {
            // Child process
            auto result = child_process_function();
            _exit(result == ChildProcessState::SUCCESS ? 0 : 1);
        } else if (status == -1) {
            auto error_message = strerror(errno);
            errno = 0;
            return std::unexpected(PikaError { .error_type = PikaErrorType::Unknown,
                .error_message = fmt::format("fork failed with error:{}", error_message) });
        }
        ChildProcessHandle child_process_handle;
        child_process_handle.m_child_process_id = static_cast<pid_t>(status);
        return child_process_handle;
    }

    auto WaitForChildProcess() -> std::expected<void, PikaError>;

private:
    pid_t m_child_process_id {};
};

#endif