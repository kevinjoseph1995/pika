
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

#ifndef PIKA_CHANNEL_INTERNAL_HPP
#define PIKA_CHANNEL_INTERNAL_HPP

#include "backing_storage.hpp"
#include "channel_header.hpp"
#include "channel_interface.hpp"
#include "fmt/core.h"
#include "ring_buffer.hpp"
#include "synchronization_primitives.hpp"
#include "utils.hpp"

#include <atomic>
#include <concepts>
#include <memory>
#include <thread>
#include <type_traits>

using namespace std::chrono_literals;

template <typename BackingStorageType, RingBufferType RingBuffer>
static auto PrepareHeader(pika::ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment, BackingStorageType& storage) -> std::expected<void, PikaError>
{
    auto const semaphore_name = std::string(channel_params.channel_name)
        + (channel_params.channel_type == pika::ChannelType::InterThread ? "_inter_thread"
                                                                         : "_inter_process");
    // Acquire exclusive access of the header(This will work across processes as well)
    auto result = Semaphore::New(semaphore_name, 1);
    if (!result.has_value()) {
        return std::unexpected { result.error() };
    }
    auto& sem = result.value();
    sem.Wait();
    Defer defer([&sem]() {
        sem.Post();
    }); // Release exclusive access of the header at the end of this block

    // We now have exclusive access to either create or re-open an already
    // existing shared memory segment

    auto header = reinterpret_cast<ChannelHeader<RingBuffer>*>(storage.GetBuffer());
    if (not header->registered.load()) {
        // This segment was not previously initialized by another producer/consumer
        header = new (header) ChannelHeader<RingBuffer> {};
        header->single_producer_single_consumer_mode
            = channel_params.single_producer_single_consumer_mode;
        auto result = header->ring_buffer.Initialize(
            storage.GetBuffer() + GetRingBufferSlotsOffset<RingBuffer>(element_alignment),
            element_size, element_alignment, channel_params.queue_size);
        if (not result.has_value()) {
            return std::unexpected { result.error() };
        }
        header->registered.store(true);
    } else {
        // This segment was previously initialized by another producer / consumer
        // Validate the header with the current parameters
        if (channel_params.queue_size != header->ring_buffer.GetQueueLength()) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
                .error_message = fmt::format("Existing ring buffer queue length: {}; Requested "
                                             "ring buffer queue length: {}",
                    header->ring_buffer.GetQueueLength(), channel_params.queue_size) } };
        }
        if (element_size != header->ring_buffer.GetElementSizeInBytes()) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
                .error_message
                = fmt::format("Existing ring buffer element size(in bytes): {}; Requested "
                              "element size(in bytes): {}",
                    header->ring_buffer.GetElementSizeInBytes(), element_size) } };
        }
        if (element_alignment != header->ring_buffer.GetElementAlignment()) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
                .error_message
                = fmt::format("Existing ring buffer element alignment: {}; Requested "
                              "element alignment: {}",
                    header->ring_buffer.GetElementAlignment(), element_alignment) } };
        }
        if (channel_params.single_producer_single_consumer_mode
            != header->single_producer_single_consumer_mode) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::RingBufferError,
                .error_message = fmt::format(
                    "Provided channel parameters has "
                    "single_producer_single_consumer_mode set to {}. However channel was "
                    "already established with single_producer_single_consumer_mode set to {}",
                    channel_params.single_producer_single_consumer_mode,
                    header->single_producer_single_consumer_mode) } };
        }
    }
    return {};
}

template <typename BackingStorageType, RingBufferType RingBuffer>
[[nodiscard]] static auto CreateBackingStorage(pika::ChannelParameters const& channel_params,
    uint64_t element_size, uint64_t element_alignment)
    -> std::expected<BackingStorageType, PikaError>
{
    BackingStorageType backing_storage;
    auto shared_buffer_result = backing_storage.Initialize(channel_params.channel_name,
        GetBufferSize<RingBuffer>(channel_params.queue_size, element_size, element_alignment));
    if (!shared_buffer_result.has_value()) {
        return std::unexpected { shared_buffer_result.error() };
    }
    if (reinterpret_cast<std::uintptr_t>(backing_storage.GetBuffer())
            % alignof(ChannelHeader<RingBuffer>)
        != 0) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::RingBufferError,
            .error_message = "CreateSharedBuffer::Create buffer is not aligned" });
    }
    auto result = PrepareHeader<BackingStorageType, RingBuffer>(
        channel_params, element_size, element_alignment, backing_storage);
    if (!result.has_value()) {
        return std::unexpected { result.error() };
    }
    return backing_storage;
}
template <typename BackingStorageType, RingBufferType RingBuffer>
auto GetHeader(BackingStorageType& storage) -> ChannelHeader<RingBuffer>&
{
    PIKA_ASSERT(storage.GetSize() != 0 && storage.GetBuffer() != nullptr);
    return *reinterpret_cast<ChannelHeader<RingBuffer>*>(storage.GetBuffer());
}

template <typename BackingStorageType, RingBufferType RingBuffer>
struct ConsumerInternal : public pika::ConsumerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<ConsumerInternal<BackingStorageType, RingBuffer>>,
            PikaError>
    {
        auto backing_storage_result = CreateBackingStorage<BackingStorageType, RingBuffer>(
            channel_params, element_size, element_alignment);
        if (!backing_storage_result.has_value()) {
            return std::unexpected { backing_storage_result.error() };
        }
        auto& header = GetHeader<BackingStorageType, RingBuffer>(*backing_storage_result);
        header.consumer_count.fetch_add(1);
        return std::unique_ptr<ConsumerInternal<BackingStorageType, RingBuffer>>(
            new ConsumerInternal<BackingStorageType, RingBuffer>(
                std::move(*backing_storage_result)));
    }

    auto Connect() -> std::expected<void, PikaError> override
    {
        auto& header = GetHeader<BackingStorageType, RingBuffer>(m_storage);
        while (header.producer_count.load() == 0) {
            std::this_thread::yield();
        }
        return {};
    }
    auto Receive(uint8_t* const destination_buffer) -> std::expected<void, PikaError> override
    {
        auto result = GetHeader<BackingStorageType, RingBuffer>(m_storage).ring_buffer.Get(
            destination_buffer);
        if (not result.has_value()) {
            return std::unexpected { result.error() };
        }
        return {};
    }

    virtual ~ConsumerInternal()
    {
        auto& header = GetHeader<BackingStorageType, RingBuffer>(m_storage);
        header.consumer_count.fetch_sub(1);
    }

private:
    ConsumerInternal(BackingStorageType storage)
        : m_storage(std::move(storage))
    {
    }
    BackingStorageType m_storage;
};

template <typename BackingStorageType, RingBufferType RingBuffer>
struct ProducerInternal : public pika::ProducerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<ProducerInternal<BackingStorageType, RingBuffer>>,
            PikaError>
    {
        auto backing_storage_result = CreateBackingStorage<BackingStorageType, RingBuffer>(
            channel_params, element_size, element_alignment);
        if (!backing_storage_result.has_value()) {
            return std::unexpected { backing_storage_result.error() };
        }
        auto& header = GetHeader<BackingStorageType, RingBuffer>(*backing_storage_result);
        header.producer_count.fetch_add(1);
        return std::unique_ptr<ProducerInternal<BackingStorageType, RingBuffer>>(
            new ProducerInternal<BackingStorageType, RingBuffer>(
                std::move(*backing_storage_result)));
    }

    auto Connect() -> std::expected<void, PikaError> override
    {
        auto& header = GetHeader<BackingStorageType, RingBuffer>(m_storage);
        while (header.consumer_count.load() == 0) {
            std::this_thread::yield();
        }
        return {};
    }

    auto Send(uint8_t const* const source_buffer) -> std::expected<void, PikaError> override
    {
        auto result
            = GetHeader<BackingStorageType, RingBuffer>(m_storage).ring_buffer.Put(source_buffer);
        if (not result.has_value()) {
            return std::unexpected { result.error() };
        }
        return {};
    }

    virtual ~ProducerInternal()
    {
        auto& header = GetHeader<BackingStorageType, RingBuffer>(m_storage);
        header.producer_count.fetch_sub(1);
    }

private:
    ProducerInternal(BackingStorageType storage)
        : m_storage(std::move(storage))
    {
    }
    BackingStorageType m_storage;
};

#endif