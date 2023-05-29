#include "inter_process_channel.hpp"
#include "error.hpp"
#include "shared_buffer.hpp"
#include "utils.hpp"

#include <memory>
#include <thread>

using namespace std::chrono_literals;

static auto GetRingBufferSlotsOffset(uint64_t element_alignment)
{
    PIKA_ASSERT(element_alignment % 2 == 0);
    if (element_alignment < sizeof(SharedBufferHeader)) {
        return ((sizeof(SharedBufferHeader) / element_alignment) + 1) * element_alignment;
    } else {
        return element_alignment;
    }
}

[[nodiscard]] static auto GetBufferSize(pika::ChannelParameters const& channel_parameters,
    uint64_t element_size, uint64_t element_alignment) -> uint64_t
{
    return GetRingBufferSlotsOffset(element_alignment)
        + (channel_parameters.queue_size * element_size);
}

static auto CreateSharedBuffer(pika::ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment) -> std::expected<SharedBuffer, PikaError>
{
    SharedBuffer buffer;
    auto shared_buffer_result = buffer.Initialize(channel_params.channel_name,
        GetBufferSize(channel_params, element_size, element_alignment));
    if (!shared_buffer_result.has_value()) {
        return std::unexpected { shared_buffer_result.error() };
    }
    if (reinterpret_cast<std::uintptr_t>(buffer.GetBuffer()) % alignof(SharedBufferHeader) != 0) {
        return std::unexpected(PikaError { .error_type = PikaErrorType::SharedRingBufferError,
            .error_message = "CreateSharedBuffer::Create buffer is not aligned" });
    }
    auto header = reinterpret_cast<SharedBufferHeader*>(buffer.GetBuffer());
    if (header->m_consumer_count.load() == 0 && header->m_producer_count.load() == 0) {
        // This segment was not previously initialized by another producer/consumer
        header = new (header) SharedBufferHeader {};
        auto result = header->ring_buffer.Initialize(
            buffer.GetBuffer() + GetRingBufferSlotsOffset(element_alignment), element_size,
            element_alignment, channel_params.queue_size, true);
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
    return buffer;
}

InterProcessConsumer::~InterProcessConsumer() { getHeader().m_consumer_count.fetch_sub(1); }
InterProcessProducer::~InterProcessProducer() { getHeader().m_producer_count.fetch_sub(1); }

auto InterProcessConsumer::Create(pika::ChannelParameters const& channel_params,
    uint64_t element_size, uint64_t element_alignment)
    -> std::expected<std::unique_ptr<InterProcessConsumer>, PikaError>
{
    auto result = Semaphore::New(channel_params.channel_name, 1);
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
    auto shared_buffer_result = CreateSharedBuffer(channel_params, element_size, element_alignment);
    if (not shared_buffer_result.has_value()) {
        return std::unexpected { result.error() };
    }
    auto header = reinterpret_cast<SharedBufferHeader*>(shared_buffer_result->GetBuffer());
    header->m_consumer_count.fetch_add(1);
    return std::unique_ptr<InterProcessConsumer>(
        new InterProcessConsumer(std::move(shared_buffer_result.value())));
}

auto InterProcessProducer::Create(pika::ChannelParameters const& channel_params,
    uint64_t element_size, uint64_t element_alignment)
    -> std::expected<std::unique_ptr<InterProcessProducer>, PikaError>
{
    auto result = Semaphore::New(channel_params.channel_name, 1);
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
    auto shared_buffer_result = CreateSharedBuffer(channel_params, element_size, element_alignment);
    if (not shared_buffer_result.has_value()) {
        return std::unexpected { result.error() };
    }
    auto header = reinterpret_cast<SharedBufferHeader*>(shared_buffer_result->GetBuffer());
    header->m_producer_count.fetch_add(1);
    return std::unique_ptr<InterProcessProducer>(
        new InterProcessProducer(std::move(shared_buffer_result.value())));
}

auto InterProcessConsumer::Connect() -> std::expected<void, PikaError>
{
    // TODO: Optimize me
    while (getHeader().m_producer_count.load() == 0) {
        std::this_thread::sleep_for(1s);
    }
    return {};
}

auto InterProcessConsumer::Receive(uint8_t* const destination_buffer,
    uint64_t destination_buffer_size) -> std::expected<void, PikaError>
{
    auto read_slot = getHeader().ring_buffer.GetReadSlot();
    if (not read_slot.has_value()) {
        return std::unexpected { read_slot.error() };
    }
    std::memcpy(destination_buffer, read_slot->GetElement(), destination_buffer_size);
    return {};
}

auto InterProcessProducer::Connect() -> std::expected<void, PikaError>
{
    // TODO: Optimize me
    while (getHeader().m_consumer_count.load() == 0) {
        std::this_thread::sleep_for(1s);
    }
    return {};
}
auto InterProcessProducer::Send(uint8_t const* const source_buffer, uint64_t size)
    -> std::expected<void, PikaError>
{
    auto write_slot = getHeader().ring_buffer.GetWriteSlot();
    if (not write_slot.has_value()) {
        return std::unexpected { write_slot.error() };
    }
    std::memcpy(write_slot->GetElement(), source_buffer, size);
    return {};
}