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

#ifndef PIKA_SYNC_PRIM_HPP
#define PIKA_SYNC_PRIM_HPP

#include "channel_interface.hpp"
#include "error.hpp"
#include "utils.hpp"

#include <bits/types/struct_timespec.h>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <fmt/core.h>
#include <pthread.h>
#include <semaphore.h>

struct Semaphore {
    [[nodiscard]] static auto New(std::string const& semaphore_name, int32_t initial_value)
        -> std::expected<Semaphore, PikaError>;
    auto Wait() -> void;
    auto Post() -> void;
    Semaphore(Semaphore const&) = delete;
    Semaphore(Semaphore&&);
    ~Semaphore();

private:
    Semaphore() = default;
    sem_t* m_sem = nullptr;
    std::string m_sem_name;
};

struct Mutex {
    Mutex() = default;
    // Make this restrictive, we don't want to worry about the semantics of the
    // all the types of contructors
    Mutex(Mutex const&) = delete;
    Mutex(Mutex&&) = delete;

    [[nodiscard]] auto Initialize(bool inter_process = false) -> std::expected<void, PikaError>;
    [[nodiscard]] auto Lock() -> std::expected<void, PikaError>;
    [[nodiscard]] auto LockTimed(DurationUs duration) -> std::expected<void, PikaError>;
    [[nodiscard]] auto Unlock() -> std::expected<void, PikaError>;

    ~Mutex();

private:
    bool m_initialized = false;
    friend struct LockedMutex;
    friend struct ConditionVariable;
    pthread_mutex_t m_pthread_mutex {};
};

struct LockedMutex {
    [[nodiscard]] static auto New(Mutex* mutex) -> std::expected<LockedMutex, PikaError>;
    [[nodiscard]] static auto New(Mutex* mutex, DurationUs duration)
        -> std::expected<LockedMutex, PikaError>;
    ~LockedMutex();
    LockedMutex(LockedMutex const&) = delete;
    LockedMutex(LockedMutex&& other)
        : m_mutex(other.m_mutex)
    {
        m_mutex = other.m_mutex;
        other.m_mutex = nullptr;
    };
    void operator=(LockedMutex&& other)
    {
        m_mutex = other.m_mutex;
        other.m_mutex = nullptr;
    }

private:
    friend struct ConditionVariable;
    LockedMutex(Mutex* mutex)
        : m_mutex(mutex)
    {
    }
    Mutex* m_mutex = nullptr;
};

struct ConditionVariable {
    [[nodiscard]] auto Initialize(bool inter_process = false) -> std::expected<void, PikaError>;
    template <typename Predicate> void Wait(LockedMutex& locked_mutex, Predicate stop_waiting)
    {
        while (stop_waiting() == false) {
            auto status
                = pthread_cond_wait(&m_pthread_cond, &locked_mutex.m_mutex->m_pthread_mutex);
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
    ConditionVariable() = default;
    ConditionVariable(ConditionVariable const&) = delete;
    ConditionVariable(ConditionVariable&&) = delete;
    ConditionVariable& operator=(ConditionVariable&&) = delete;
    ~ConditionVariable();

private:
    bool m_initialized = false;
    pthread_cond_t m_pthread_cond {};
};

#endif