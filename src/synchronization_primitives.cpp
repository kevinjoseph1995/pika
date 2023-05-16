#include "synchronization_primitives.hpp"
#include "error.hpp"
#include "fmt/core.h"

#include <cstdio>
#include <pthread.h>
#include <unistd.h>

auto InterProcessMutex::Initialize() -> std::expected<void, PikaError>
{
    pthread_mutexattr_t mutex_attr {};
    auto return_code = pthread_mutexattr_init(&mutex_attr);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_mutexattr_init failed with error code:{}", return_code) }
        };
    }
    return_code = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_mutexattr_setpshared failed with error code:{}", return_code) }
        };
    }
    return_code = pthread_mutex_init(&m_pthread_mutex, &mutex_attr);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_mutex_init failed with error code:{}", return_code) }
        };
    }
    m_initialized = true;
    return {};
}

InterProcessMutex ::~InterProcessMutex()
{
    if (m_initialized) {
        auto return_code = pthread_mutex_destroy(&m_pthread_mutex);
        if (return_code != 0) {
            fmt::println(stderr, "pthread_mutex_destroy failed with return code{}", return_code);
        }
    }
}

auto InterProcessMutex::Lock() -> std::expected<void, PikaError>
{
    if (not m_initialized) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = "InterProcessMutex::Lock Uninitialized" }
        };
    }
    auto return_code = pthread_mutex_lock(&m_pthread_mutex);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_mutex_lock failed with return code:{}", return_code) }
        };
    }
    return {};
}

auto InterProcessMutex::Unlock() -> std::expected<void, PikaError>
{
    if (not m_initialized) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = "InterProcessMutex::UnLock Uninitialized" }
        };
    }
    auto return_code = pthread_mutex_unlock(&m_pthread_mutex);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_mutex_unlock failed with return code:{}", return_code) }
        };
    }
    return {};
}

auto LockedMutex::New(InterProcessMutex* mutex) -> std::expected<LockedMutex, PikaError>
{
    PIKA_ASSERT(mutex != nullptr);
    if (not mutex->m_initialized) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = "LockedMutex::New Uninitialized mutex" }
        };
    }

    auto lock_result = mutex->Lock();
    if (not lock_result.has_value()) {
        return std::unexpected(lock_result.error());
    }
    return LockedMutex { mutex };
}

LockedMutex::~LockedMutex()
{
    if (m_mutex) {
        auto result = m_mutex->Unlock();
        if (not result.has_value()) {
            fmt::println(stderr, "LockedMutex::~LockedMutex() Mutex unlock failed with error {}", result.error().error_message);
        }
        m_mutex = nullptr;
    }
}

auto InterProcessConditionVariable::Initialize() -> std::expected<void, PikaError>
{
    pthread_condattr_t cond_attr {};
    auto return_code = pthread_condattr_init(&cond_attr);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_condattr_init failed with error code:{}", return_code) }
        };
    }
    return_code = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_condattr_setpshared failed with error code:{}", return_code) }
        };
    }
    return_code = pthread_cond_init(&m_pthread_cond, &cond_attr);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_cond_init failed with error code:{}", return_code) }
        };
    }
    m_initialized = true;
    return {};
}

InterProcessConditionVariable ::~InterProcessConditionVariable()
{
    if (m_initialized) {
        auto return_code = pthread_cond_destroy(&m_pthread_cond);
        if (return_code != 0) {
            fmt::println(stderr, "pthread_cond_destroy failed with return code{}", return_code);
        }
        m_initialized = false;
    }
}