#include "error.hpp"
#include "fmt/core.h"
#include "shared_ring_buffer.hpp"
#include "shared_ring_buffer_lockfree.hpp"
#include "gtest/gtest.h"
#include <chrono>
#include <cstdint>
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

TEST(SharedRingBuffer, WithLock)
{
    SharedRingBuffer<int64_t> shared_ring_buffer;
    EXPECT_TRUE(shared_ring_buffer.Initialize(1000).has_value());

    auto code = fork();
    static constexpr auto NUM_MESSAGES = 1000000000;

    if (code == 0) {
        shared_ring_buffer.RegisterEndpoint();
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            {
                auto write_slot = shared_ring_buffer.GetWriteSlot();
                if (write_slot.has_value()) {
                    write_slot->GetElement() = i;
                }
            }
        }
    } else {
        shared_ring_buffer.RegisterEndpoint();
        StopWatch stop_watch;
        while (true) {
            {
                auto read_slot = shared_ring_buffer.GetReadSlot();
                if (read_slot.has_value()) {
                    fmt::println("Popped: {}", read_slot->GetElement());
                    fmt::println("duration={}", stop_watch.ElapsedDuration<uint64_t, std::chrono::nanoseconds>());
                    stop_watch.Reset();
                }
            }
        }
    }
}

TEST(SharedRingBuffer, LockFree)
{
    SharedRingBufferLockFree<int64_t> ring_buffer;
    EXPECT_TRUE(ring_buffer.Initialize(1000).has_value());
    static constexpr auto NUM_MESSAGES = 1000000000;
    auto code = fork();
    if (code == 0) {
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            {
                while (!ring_buffer.Push(i)) {
                    fmt::println("Failed Push: {}", i);
                }
                fmt::println("Push: {}", i);
            }
        }
    } else {
        int volatile i = 1;
        StopWatch stop_watch;
        while (i) {
            {
                int64_t data {};
                if (ring_buffer.Pop(data)) {
                    fmt::println("Popped: {}", data);
                    fmt::println("duration={}", stop_watch.ElapsedDuration<uint64_t, std::chrono::nanoseconds>());
                    stop_watch.Reset();
                }
            }
        }
    }
}
