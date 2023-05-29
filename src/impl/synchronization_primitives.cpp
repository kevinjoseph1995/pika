#include "synchronization_primitives.hpp"
#include "error.hpp"
#include "fmt/core.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

auto Semaphore::New(std::string const& semaphore_name, int32_t initial_value) -> std::expected<Semaphore, PikaError>
{
    if (semaphore_name.at(0) != '/') {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = "Semaphore::Initialize: Semaphore name must begin with a \"/\"" });
    }
    auto sem_ptr = sem_open(semaphore_name.c_str(), O_CREAT, S_IRUSR | S_IWUSR, initial_value);
    if (sem_ptr == SEM_FAILED) {
        auto error_message = strerror(errno);
        errno = 0;
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedBufferError,
            .error_message = fmt::format("sem_open failed with error:{}",
                error_message) });
    }
    Semaphore sem;
    sem.m_sem = sem_ptr;
    sem.m_sem_name = semaphore_name;
    return sem;
}

Semaphore::~Semaphore()
{
    if (m_sem != nullptr) {
        auto ret = sem_close(m_sem);
        if (ret != 0) {
            auto error_message = strerror(errno);
            errno = 0;
            fmt::println(stderr, "Semaphore::~Semaphore sem_unlink({}) failed with error {}", m_sem_name, error_message);
            return;
        }
        m_sem_name.clear();
        m_sem = nullptr;
    }
}

Semaphore::Semaphore(Semaphore&& other)
{
    m_sem = other.m_sem;
    m_sem_name = std::move(other.m_sem_name);
    other.m_sem = nullptr;
}

auto Semaphore::Wait() -> void
{
    PIKA_ASSERT(m_sem != nullptr);
    if (sem_wait(m_sem) != 0) {
        auto error_message = strerror(errno);
        errno = 0;
        fmt::println(stderr, "Semaphore::Wait sem_wait failed with error {}", error_message);
    }
}

auto Semaphore::Post() -> void
{
    PIKA_ASSERT(m_sem != nullptr);
    if (sem_post(m_sem) != 0) {
        auto error_message = strerror(errno);
        errno = 0;
        fmt::println(stderr, "Semaphore::Post sem_post failed with error {}", error_message);
    }
}

auto Mutex::Initialize(bool intra_process) -> std::expected<void, PikaError>
{
    pthread_mutexattr_t mutex_attr {};
    auto return_code = pthread_mutexattr_init(&mutex_attr);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_mutexattr_init failed with error code:{}", return_code) }
        };
    }
    if (intra_process) {
        return_code = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        if (return_code != 0) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = fmt::format("pthread_mutexattr_setpshared failed with error code:{}", return_code) }
            };
        }
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

Mutex ::~Mutex()
{
    if (m_initialized) {
        auto return_code = pthread_mutex_destroy(&m_pthread_mutex);
        if (return_code != 0) {
            fmt::println(stderr, "pthread_mutex_destroy failed with return code{}", return_code);
        }
    }
}

auto Mutex::Lock() -> std::expected<void, PikaError>
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

auto Mutex::Unlock() -> std::expected<void, PikaError>
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

auto LockedMutex::New(Mutex* mutex) -> std::expected<LockedMutex, PikaError>
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

auto ConditionVariable::Initialize(bool intra_process) -> std::expected<void, PikaError>
{
    pthread_condattr_t cond_attr {};
    auto return_code = pthread_condattr_init(&cond_attr);
    if (return_code != 0) {
        return std::unexpected {
            PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                .error_message = fmt::format("pthread_condattr_init failed with error code:{}", return_code) }
        };
    }
    if (intra_process) {
        return_code = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        if (return_code != 0) {
            return std::unexpected {
                PikaError { .error_type = PikaErrorType::SyncPrimitiveError,
                    .error_message = fmt::format("pthread_condattr_setpshared failed with error code:{}", return_code) }
            };
        }
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

ConditionVariable ::~ConditionVariable()
{
    if (m_initialized) {
        auto return_code = pthread_cond_destroy(&m_pthread_cond);
        if (return_code != 0) {
            fmt::println(stderr, "pthread_cond_destroy failed with return code{}", return_code);
        }
        m_initialized = false;
    }
}