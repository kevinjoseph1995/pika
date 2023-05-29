#ifndef PIKA_TEST_UTILS_HPP
#define PIKA_TEST_UTILS_HPP

#include <chrono>
#include <cstdint>

template <typename Clock = std::chrono::high_resolution_clock> class StopWatch {
    typename Clock::time_point start_point;

public:
    StopWatch()
        : start_point(Clock::now())
    {
    }

    auto Reset() -> void { start_point = Clock::now(); }

    template <typename Rep = typename Clock::duration::rep>
    auto ElapsedDurationUs() const -> int64_t
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_point)
            .count();
    }
};

#endif