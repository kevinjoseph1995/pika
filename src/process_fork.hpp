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

enum ChildProcessState {
    SUCCESS,
    FAIL
};

template<typename F>
concept ChildProcessFunction = requires(F f) {
    {
        f()
    } -> std::same_as<ChildProcessState>;
};

struct ChildProcessHandle {
    template<ChildProcessFunction F>
    static auto RunChildFunction(F child_process_function) -> std::expected<ChildProcessHandle, PikaError>
    {
        auto status = fork();
        if (status == 0) {
            // Child process
            auto result = child_process_function();
            _exit(result == ChildProcessState::SUCCESS ? 0 : 1);
        } else if (status == -1) {
            auto error_message = strerror(errno);
            errno = 0;
            return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
                .error_message = fmt::format("fork failed with error:{}",
                    error_message) });
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