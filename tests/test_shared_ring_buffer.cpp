// Local includes
#include "error.hpp"
#include "process_fork.hpp"
#include "ring_buffer.hpp"
#include "shared_buffer.hpp"
// System includes
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

template<typename Clock = std::chrono::high_resolution_clock>
class StopWatch {
    typename Clock::time_point start_point;

public:
    StopWatch()
        : start_point(Clock::now())
    {
    }

    auto Reset() -> void
    {
        start_point = Clock::now();
    }

    template<typename Rep = typename Clock::duration::rep, typename Units = typename Clock::duration>
    auto ElapsedDuration() const -> Rep
    {
        auto counted_time = std::chrono::duration_cast<Units>(Clock::now() - start_point).count();
        return static_cast<Rep>(counted_time);
    }
};

TEST(SharedBuffer, Init)
{
    SharedBuffer shared_buffer;
    auto result = shared_buffer.Initialize("/test", 100);
    ASSERT_TRUE(result.has_value()) << result.error().error_message;
}

TEST(SharedBuffer, MultipleProcess)
{
    auto child_process_handle = ChildProcessHandle::RunChildFunction([]() -> ChildProcessState {
        SharedBuffer shared_buffer;
        auto result = shared_buffer.Initialize("/test", 100);
        if (!result.has_value()) {
            fmt::println("{}", result.error().error_message);
        }
        while (reinterpret_cast<std::atomic_int*>(shared_buffer.GetBuffer())->load() != 1) {
            std::this_thread::yield();
        }
        return ChildProcessState::SUCCESS;
    });

    SharedBuffer shared_buffer;
    auto result = shared_buffer.Initialize("/test", 100);
    ASSERT_TRUE(result.has_value()) << result.error().error_message;
    reinterpret_cast<std::atomic_int*>(shared_buffer.GetBuffer())->store(1);
    result = child_process_handle->WaitForChildProcess();
    ASSERT_TRUE(result.has_value()) << result.error().error_message;
}