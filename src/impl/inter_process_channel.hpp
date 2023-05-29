
#ifndef PIKA_INTER_PROCESS_CHANNEL_HPP
#define PIKA_INTER_PROCESS_CHANNEL_HPP

#include "channel_interface.hpp"

#include "error.hpp"
#include "ring_buffer.hpp"
#include "shared_buffer.hpp"

struct SharedBufferHeader {
    std::atomic<uint64_t> m_producer_count = 0;
    std::atomic<uint64_t> m_consumer_count = 0;
    RingBuffer ring_buffer;
};

struct InterProcessConsumer : public pika::ConsumerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<InterProcessConsumer>, PikaError>;
    auto Connect() -> std::expected<void, PikaError> override;
    auto Receive(uint8_t* const destination_buffer, uint64_t destination_buffer_size)
        -> std::expected<void, PikaError> override;

private:
    InterProcessConsumer(SharedBuffer buffer)
        : m_buffer(std::move(buffer))
    {
    }
    auto getHeader() -> SharedBufferHeader&
    {
        PIKA_ASSERT(m_buffer.GetSize() != 0 && m_buffer.GetBuffer() != nullptr);
        return *reinterpret_cast<SharedBufferHeader*>(m_buffer.GetBuffer());
    }
    SharedBuffer m_buffer;
};

struct InterProcessProducer : public pika::ProducerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<InterProcessProducer>, PikaError>;
    auto Connect() -> std::expected<void, PikaError> override;
    auto Send(uint8_t const* const source_buffer, uint64_t size)
        -> std::expected<void, PikaError> override;

private:
    InterProcessProducer(SharedBuffer buffer)
        : m_buffer(std::move(buffer))
    {
    }
    auto getHeader() -> SharedBufferHeader&
    {
        PIKA_ASSERT(m_buffer.GetSize() != 0 && m_buffer.GetBuffer() != nullptr);
        return *reinterpret_cast<SharedBufferHeader*>(m_buffer.GetBuffer());
    }
    SharedBuffer m_buffer;
};

#endif