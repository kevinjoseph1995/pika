#ifndef PIKA_SYNC_PRIM_HPP
#define PIKA_SYNC_PRIM_HPP

#include "error.hpp"

#include <expected>
#include <fmt/core.h>
#include <pthread.h>

struct InterProcessMutex {
    [[nodiscard]] auto Initialize() -> std::expected<void, PikaError>;
    [[nodiscard]] auto Lock() -> std::expected<void, PikaError>;
    [[nodiscard]] auto Unlock() -> std::expected<void, PikaError>;
    InterProcessMutex() = default;
    InterProcessMutex(InterProcessMutex const&) = delete;
    InterProcessMutex(InterProcessMutex&&);
    InterProcessMutex& operator=(InterProcessMutex&&);
    ~InterProcessMutex();

private:
    friend struct LockedMutex;
    bool m_initialized = false;
    pthread_mutex_t m_pthread_mutex {};
};

struct LockedMutex {
    [[nodiscard]] static auto New(InterProcessMutex& mutex) -> std::expected<LockedMutex, PikaError>;
    ~LockedMutex();
    LockedMutex(LockedMutex const&) = delete;
    LockedMutex(LockedMutex&& other)
        : m_mutex(other.m_mutex)
    {
        this->m_initialized = true;
        other.m_initialized = false;
    };
    [[nodiscard]] LockedMutex& operator=(LockedMutex&& other)
    {
        m_mutex = std::move(other.m_mutex);
        this->m_initialized = true;
        other.m_initialized = false;
        return *this;
    }

private:
    LockedMutex(InterProcessMutex& mutex)
        : m_initialized(true)
        , m_mutex(mutex)
    {
    }

    bool m_initialized = false;
    InterProcessMutex& m_mutex;
};

#endif