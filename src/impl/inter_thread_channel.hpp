#ifndef PIKA_INTER_THREAD_CHANNEL_HPP
#define PIKA_INTER_THREAD_CHANNEL_HPP
#include "channel_interface.hpp"

#include "backing_storage.hpp"
#include "error.hpp"
#include "ring_buffer.hpp"

namespace inter_thread {
struct SharedBufferHeader {
    std::atomic<uint64_t> m_producer_count = 0;
    std::atomic<uint64_t> m_consumer_count = 0;
    RingBuffer ring_buffer;
};
}

struct InterThreadConsumer : public pika::ConsumerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<InterThreadConsumer>, PikaError>;
    auto Connect() -> std::expected<void, PikaError> override;
    auto Receive(uint8_t* const destination_buffer, uint64_t destination_buffer_size)
        -> std::expected<void, PikaError> override;
    ~InterThreadConsumer() { getHeader().m_consumer_count.fetch_sub(1); }

private:
    InterThreadConsumer(InterThreadSharedBuffer buffer)
        : m_buffer(std::move(buffer))
    {
    }
    auto getHeader() -> inter_thread::SharedBufferHeader&
    {
        PIKA_ASSERT(m_buffer.GetSize() != 0 && m_buffer.GetBuffer() != nullptr);
        return *reinterpret_cast<inter_thread::SharedBufferHeader*>(m_buffer.GetBuffer());
    }
    InterThreadSharedBuffer m_buffer;
};

struct InterThreadProducer : public pika::ProducerImpl {
    static auto Create(pika::ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment)
        -> std::expected<std::unique_ptr<InterThreadProducer>, PikaError>;
    auto Connect() -> std::expected<void, PikaError> override;
    auto Send(uint8_t const* const source_buffer, uint64_t size)
        -> std::expected<void, PikaError> override;
    ~InterThreadProducer() { getHeader().m_producer_count.fetch_sub(1); }

private:
    InterThreadProducer(InterThreadSharedBuffer buffer)
        : m_buffer(std::move(buffer))
    {
    }
    auto getHeader() -> inter_thread::SharedBufferHeader&
    {
        PIKA_ASSERT(m_buffer.GetSize() != 0 && m_buffer.GetBuffer() != nullptr);
        return *reinterpret_cast<inter_thread::SharedBufferHeader*>(m_buffer.GetBuffer());
    }
    InterThreadSharedBuffer m_buffer;
};

#endif