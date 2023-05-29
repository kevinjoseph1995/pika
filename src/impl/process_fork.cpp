#include "process_fork.hpp"
#include "error.hpp"
#include "fmt/core.h"

#include <__expected/unexpected.h>
#include <sys/wait.h>

auto ChildProcessHandle::WaitForChildProcess()
    -> std::expected<void, PikaError> {
  int status{};
  do {
    auto return_code =
        waitpid(m_child_process_id, &status, WUNTRACED | WCONTINUED);
    if (return_code == -1) {
      auto error_message = strerror(errno);
      errno = 0;
      return std::unexpected(
          PikaError{.error_message = fmt::format("waitpid failed with error{} ",
                                                 error_message)});
    }

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == 0) {
        return {};
      } else {
        return std::unexpected(PikaError{
            .error_message =
                fmt::format("Child process exited with return code:{}",
                            WEXITSTATUS(status))});
      }
    }
  } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  PIKA_ASSERT(false);
}