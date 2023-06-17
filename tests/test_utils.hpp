#ifndef PIKA_TEST_UTILS_HPP
#define PIKA_TEST_UTILS_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>
#include <type_traits>

auto inline GetRandomIntVector(uint64_t size) -> std::vector<int>
{
    // First create an instance of an engine.
    std::random_device rnd_device;
    // Specify the engine and distribution.
    std::mt19937 mersenne_engine { rnd_device() }; // Generates random integers
    std::uniform_int_distribution<int> dist { 1, 52 };
    auto gen = [&dist, &mersenne_engine]() { return dist(mersenne_engine); };
    std::vector<int> vec(size);
    std::generate(begin(vec), end(vec), gen);
    return vec;
}

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