#ifndef PIKA_SYNC_PRIM_HPP
#define PIKA_SYNC_PRIM_HPP

#include "error.hpp"

#include <cstdio>
#include <expected>
#include <fmt/core.h>
#include <pthread.h>

struct InterProcessMutex {
    InterProcessMutex() = default;
    // Make this restrictive, we don't want to worry about the semantics of the all the types of contructors
    InterProcessMutex(InterProcessMutex const&) = delete;
    InterProcessMutex(InterProcessMutex&&) = delete;

    [[nodiscard]] auto Initialize() -> std::expected<void, PikaError>;
    [[nodiscard]] auto Lock() -> std::expected<void, PikaError>;
    [[nodiscard]] auto Unlock() -> std::expected<void, PikaError>;

    ~InterProcessMutex();

private:
    bool m_initialized = false;
    friend struct LockedMutex;
    friend struct InterProcessConditionVariable;
    pthread_mutex_t m_pthread_mutex {};
};

struct LockedMutex {
    [[nodiscard]] static auto New(InterProcessMutex* mutex) -> std::expected<LockedMutex, PikaError>;
    ~LockedMutex();
    LockedMutex(LockedMutex const&) = delete;
    LockedMutex(LockedMutex&& other)
        : m_mutex(other.m_mutex)
    {
        m_mutex = other.m_mutex;
        other.m_mutex = nullptr;
    };
    [[nodiscard]] LockedMutex& operator=(LockedMutex&& other)
    {
        m_mutex = other.m_mutex;
        other.m_mutex = nullptr;
        return *this;
    }

private:
    friend struct InterProcessConditionVariable;
    LockedMutex(InterProcessMutex* mutex)
        : m_mutex(mutex)
    {
    }
    InterProcessMutex* m_mutex = nullptr;
};

struct InterProcessConditionVariable {
    [[nodiscard]] auto Initialize() -> std::expected<void, PikaError>;
    template<typename Predicate>
    void Wait(LockedMutex& locked_mutex, Predicate stop_waiting)
    {
        while (stop_waiting() == false) {
            auto status = pthread_cond_wait(&m_pthread_cond, &locked_mutex.m_mutex->m_pthread_mutex);
            if (status != 0) {
                fmt::println(stderr, "pthread_cond_wait failed with return code{}", status);
                break;
            }
        }
    }
    void Signal()
    {
        auto status = pthread_cond_signal(&m_pthread_cond);
        if (status != 0) {
            fmt::println(stderr, "pthread_cond_wait failed with return code{}", status);
        }
    }
    InterProcessConditionVariable() = default;
    InterProcessConditionVariable(InterProcessConditionVariable const&) = delete;
    InterProcessConditionVariable(InterProcessConditionVariable&&) = delete;
    InterProcessConditionVariable& operator=(InterProcessConditionVariable&&) = delete;
    ~InterProcessConditionVariable();

private:
    bool m_initialized = false;
    pthread_cond_t m_pthread_cond {};
};

#endif