
#ifndef PIKA_CHANNEL_INTERNAL_HPP
#define PIKA_CHANNEL_INTERNAL_HPP

#include "backing_storage.hpp"
#include "channel_interface.hpp"
#include "ring_buffer.hpp"
#include "utils.hpp"

#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>

using namespace std::chrono_literals;

struct SharedBufferHeader {
    std::atomic<uint64_t> m_producer_count = 0;
    std::atomic<uint64_t> m_consumer_count = 0;
    RingBuffer ring_buffer;
};

[[nodiscard]] static constexpr auto GetRingBufferSlotsOffset(uint64_t element_alignment)
{
    if (element_alignment < sizeof(SharedBufferHeader)) {
        return ((sizeof(SharedBufferHeader) / element_alignment) + 1) * element_alignment;
    } else {
        return element_alignment;
    }
}

[[nodiscard]] static constexpr auto GetBufferSize(pika::ChannelParameters const& channel_parameters,
    uint64_t element_size, uint64_t element_alignment) -> uint64_t
{
    return GetRingBufferSlotsOffset(element_alignment)
        + (channel_parameters.queue_size * element_size);
}

template <typename BackingStorageType>
static auto PrepareHeader(pika::ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment, BackingStorageType& storage) -> std::expected<void, PikaError>
{
    auto const semaphore_name = std::string(channel_params.channel_name)
        + (channel_params.channel_type == pika::ChannelType::InterThread ? "_inter_thread"
                                                                         : "_inter_process");
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

    auto header = reinterpret_cast<SharedBufferHeader*>(storage.GetBuffer());
    if (header->m_consumer_count.load() == 0 && header->m_producer_count.load() == 0) {
        // This segment was not previously initialized by another producer/consumer
        header = new (header) SharedBufferHeader {};
        auto result = header->ring_buffer.Initialize(
            storage.GetBuffer() + GetRingBufferSlotsOffset(element_alignment), element_size,
            element_alignment, channel_params.queue_size,
            std::is_same<InterProcessSharedBuffer, BackingStorageType>::value);
        if (not result.has_value()) {
            return std::unexpected { result.error() };
        }
    } else {
        // This segment was previously initialized by another producer / consumer
        // Validate the header with the current parameters
        if (channel_params.queue_size != header->ring_buffer.GetQueueLength()) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::SharedRingBufferError,
                .error_message = fmt::format("Existing ring buffer queue length: {}; Requested "
                                             "ring buffer queue length: {}",
                    header->ring_buffer.GetQueueLength(), channel_params.queue_size) } };
        }
        if (element_size != header->ring_buffer.GetElementSizeInBytes()) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::SharedRingBufferError,
                .error_message
                = fmt::format("Existing ring buffer element size(in bytes): {}; Requested "
                              "element size(in bytes): {}",
                    header->ring_buffer.GetElementSizeInBytes(), element_size) } };
        }
        if (element_alignment != header->ring_buffer.GetElementAlignment()) {
            return std::unexpected { PikaError { .error_type = PikaErrorType::SharedRingBufferError,
                .error_message
                = fmt::format("Existing ring buffer element alignment: {}; Requested "
                              "element alignment: {}",
                    header->ring_buffer.GetElementAlignment(), element_alignment) } };
        }
    }
    return {};
}

template <typename BackingStorageType>
[[nodiscard]] static auto CreateBackingStorage(pika::ChannelParameters const& channel_params,
    uint64_t element_size, uint64_t element_alignment)
    -> std::expected<BackingStorageType, PikaError>
{
    BackingStorageType backing_storage;
    auto shared_buffer_result = backing_storage.Initialize(channel_params.channel_name,
        GetBufferSize(channel_params, element_size, element_alignment));
    if (!shared_buffer_result.has_value()) {
        return std::unexpected { shared_buffer_result.error() };
    }
    if (reinterpret_cast<std::uintptr_t>(backing_storage.GetBuffer()) % alignof(SharedBufferHeader)
        != 0) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedRingBufferError,
            .error_message = "CreateSharedBuffer::Create buffer is not aligned" });
    }
    auto result = PrepareHeader(channel_params, element_size, element_alignment, backing_storage);
    if (!result.has_value()) {
        return std::unexpected { result.error() };
    }
    return backing_storage;
}
template <typename BackingStorageType>
auto GetHeader(BackingStorageType& storage) -> SharedBufferHeader&
{
    PIKA_ASSERT(storage.GetSize() != 0 && storage.GetBuffer() != nullptr);
    return *reinterpret_cast<SharedBufferHeader*>(storage.GetBuffer());
}

template <typename BackingStorageType> struct ConsumerInternal : public pika::ConsumerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<ConsumerInternal<BackingStorageType>>, PikaError>
    {
        auto backing_storage_result = CreateBackingStorage<BackingStorageType>(
            channel_params, element_size, element_alignment);
        if (!backing_storage_result.has_value()) {
            return std::unexpected { backing_storage_result.error() };
        }
        GetHeader(*backing_storage_result).m_consumer_count.fetch_add(1);
        return std::unique_ptr<ConsumerInternal<BackingStorageType>>(
            new ConsumerInternal<BackingStorageType>(std::move(*backing_storage_result)));
    }

    auto Connect() -> std::expected<void, PikaError> override
    {
        // TODO: Optimize me
        while (GetHeader(m_storage).m_producer_count.load() == 0) {
            std::this_thread::sleep_for(1ms);
        }
        return {};
    }

    auto Receive(uint8_t* const destination_buffer, uint64_t destination_buffer_size)
        -> std::expected<void, PikaError> override
    {
        auto read_slot = GetHeader(m_storage).ring_buffer.GetReadSlot();
        if (not read_slot.has_value()) {
            return std::unexpected { read_slot.error() };
        }
        std::memcpy(destination_buffer, read_slot->GetElement(), destination_buffer_size);
        return {};
    }
    virtual ~ConsumerInternal() { GetHeader(m_storage).m_consumer_count.fetch_sub(1); }

private:
    ConsumerInternal(BackingStorageType storage)
        : m_storage(std::move(storage))
    {
    }
    BackingStorageType m_storage;
};

template <typename BackingStorageType> struct ProducerInternal : public pika::ProducerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<ProducerInternal<BackingStorageType>>, PikaError>
    {
        auto backing_storage_result = CreateBackingStorage<BackingStorageType>(
            channel_params, element_size, element_alignment);
        if (!backing_storage_result.has_value()) {
            return std::unexpected { backing_storage_result.error() };
        }
        GetHeader(*backing_storage_result).m_producer_count.fetch_add(1);
        return std::unique_ptr<ProducerInternal<BackingStorageType>>(
            new ProducerInternal<BackingStorageType>(std::move(*backing_storage_result)));
    }

    auto Connect() -> std::expected<void, PikaError> override
    {
        // TODO: Optimize me
        while (GetHeader(m_storage).m_consumer_count.load() == 0) {
            std::this_thread::sleep_for(1ms);
        }
        return {};
    }

    auto Send(uint8_t const* const source_buffer, uint64_t source_buffer_size)
        -> std::expected<void, PikaError> override
    {
        auto write_slot = GetHeader(m_storage).ring_buffer.GetWriteSlot();
        if (not write_slot.has_value()) {
            return std::unexpected { write_slot.error() };
        }
        std::memcpy(write_slot->GetElement(), source_buffer, source_buffer_size);
        return {};
    }

    virtual ~ProducerInternal() { GetHeader(m_storage).m_producer_count.fetch_sub(1); }

private:
    ProducerInternal(BackingStorageType storage)
        : m_storage(std::move(storage))
    {
    }
    BackingStorageType m_storage;
};

#endif