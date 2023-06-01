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

#include "process_fork.hpp"
#include "error.hpp"
#include "fmt/core.h"

#include <sys/wait.h>

auto ChildProcessHandle::WaitForChildProcess() -> std::expected<void, PikaError>
{
    int status {};
    do {
        auto return_code = waitpid(m_child_process_id, &status, WUNTRACED | WCONTINUED);
        if (return_code == -1) {
            auto error_message = strerror(errno);
            errno = 0;
            return std::unexpected(PikaError {
                .error_message = fmt::format("waitpid failed with error{} ", error_message) });
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0) {
                return {};
            } else {
                return std::unexpected(PikaError {
                    .error_message = fmt::format(
                        "Child process exited with return code:{}", WEXITSTATUS(status)) });
            }
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    PIKA_ASSERT(false);
}