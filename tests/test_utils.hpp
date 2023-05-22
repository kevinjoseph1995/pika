#ifndef PIKA_ERROR_HPP
#define PIKA_ERROR_HPP

#include <chrono>

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

#endif