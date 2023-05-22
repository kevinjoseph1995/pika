#ifndef PIKA_CHANNEL_HPP
#define PIKA_CHANNEL_HPP
// Local includes
#include "error.hpp"
#include "ring_buffer.hpp"
#include "shared_buffer.hpp"
#include "synchronization_primitives.hpp"
// System includes
#include <atomic>
#include <cstddef>
#include <memory>

template<typename DataT>
struct ChannelMPMC;

struct ChannelParameters {
    std::string channel_name;
    size_t queue_size = 0;
    bool intra_process = false;
};

template<typename DataT>
struct ConsumerMPMC {
};

template<typename DataT>
struct ProducerMPMC {
private:
    friend struct ChannelMPMC<DataT>; // Let the channel construct the producer
    ProducerMPMC() = default;
    SharedBuffer m_buffer;
    RingBuffer<DataT> m_ring_buffer;
};

template<typename DataT>
struct ChannelMPMC {
    [[nodiscard]] static auto GetProducer(ChannelParameters& channel_parameters) -> std::expected<ProducerMPMC<DataT>, PikaError>
    {
        auto result = Semaphore::New(channel_parameters.channel_name, 1);
        if (!result.has_value()) {
            return std::unexpected { result.error() };
        }
        auto& sem = result.value();
        sem.Wait();
        // We now have exclusive access
        SharedBuffer buffer;
        auto shared_buffer_result = buffer.Initialize(channel_parameters.channel_name, getBufferSize(channel_parameters));
        if (!shared_buffer_result.has_value()) {
            return std::unexpected { result.error() };
        }
        if (!reinterpret_cast<std::atomic_bool*>(buffer.GetBuffer())->load()) {
        }
    }

    [[nodiscard]] static auto GetConsumer(ChannelParameters& channel_parameters) -> std::expected<ConsumerMPMC<DataT>, PikaError>
    {
        auto result = Semaphore::New(channel_parameters.channel_name, 1);
        if (!result.has_value()) {
            return std::unexpected { result.error() };
        }
        auto& sem = result.value();
        sem.Wait();
        TODO("");
    }

private:
    [[nodiscard]] static auto getRingBufferHeaderOffset()
    {
        // | atomic_bool | .. | Ring_buffer_header | Ring_buffer elements... |
        if constexpr (RingBuffer<DataT>::GetAlignmentRequirement() < sizeof(std::atomic_bool)) {
            return ((sizeof(std::atomic_bool) / RingBuffer<DataT>::GetAlignmentRequirement()) + 1) * RingBuffer<DataT>::GetAlignmentRequirement();
        } else {
            return RingBuffer<DataT>::GetAlignmentRequirement();
        }
    }
    [[nodiscard]] static auto getBufferSize(ChannelParameters& channel_parameters) -> size_t
    {
        return getRingBufferHeaderOffset() + RingBuffer<DataT>::GetRequiredBufferSize(channel_parameters.queue_size);
    }
};

#endif