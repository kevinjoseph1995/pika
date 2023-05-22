#ifndef PIKA_CHANNEL_HPP
#define PIKA_CHANNEL_HPP
// Local includes
#include "error.hpp"
#include "ring_buffer.hpp"
#include "shared_buffer.hpp"
#include "synchronization_primitives.hpp"
// System includes
#include <__expected/unexpected.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

struct ChannelParameters {
    std::string channel_name;
    uint64_t queue_size = 0;
};

template<typename DataT>
struct ConsumerMPMC {
    auto Recv(DataT& data) -> std::expected<void, PikaError>
    {
        auto ptr = m_ring_buffer->GetReadSlot();
        if (!ptr.has_value()) {
            return std::unexpected { ptr.error() };
        }
        auto const& read_data = reinterpret_cast<DataT const&>(*ptr->GetElement());
        data = read_data;
        return {};
    }

private:
    template<typename>
    friend struct IntraProcessChannelMPMC;
    ConsumerMPMC() = default;
    SharedBuffer m_buffer;
    RingBuffer* m_ring_buffer;
};

template<typename DataT>
struct ProducerMPMC {
    auto Send(DataT const& data) -> std::expected<void, PikaError>
    {
        auto ptr = m_ring_buffer->GetWriteSlot();
        if (not ptr.has_value()) {
            return std::unexpected { ptr.error() };
        }
        auto& write_slot = reinterpret_cast<DataT&>(*ptr->GetElement());
        write_slot = data;
        return {};
    }

private:
    template<typename>
    friend struct IntraProcessChannelMPMC;
    ProducerMPMC() = default;
    SharedBuffer m_buffer;
    RingBuffer* m_ring_buffer = nullptr;
};

template<typename DataT>
struct IntraProcessChannelMPMC {
private:
    struct ChannelHeader {
        std::atomic<uint64_t> m_endpoint_count = 0;
        RingBuffer ring_buffer;
        DataT data;
    };

public:
    [[nodiscard]] static auto GetProducer(ChannelParameters const& channel_parameters) -> std::expected<ProducerMPMC<DataT>, PikaError>
    {
        auto result = commonInit(channel_parameters);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        ProducerMPMC<DataT> producer;
        producer.m_buffer = std::move(result->first);
        producer.m_ring_buffer = result->second;
        return producer;
    }

    [[nodiscard]] static auto GetConsumer(ChannelParameters const& channel_parameters) -> std::expected<ConsumerMPMC<DataT>, PikaError>
    {
        auto result = commonInit(channel_parameters);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        ConsumerMPMC<DataT> consumer;
        consumer.m_buffer = std::move(result->first);
        consumer.m_ring_buffer = result->second;
        return consumer;
    }

private:
    [[nodiscard]] static auto commonInit(ChannelParameters const& channel_parameters) -> std::expected<std::pair<SharedBuffer, RingBuffer*>, PikaError>
    {
        auto result = Semaphore::New(channel_parameters.channel_name, 1);
        if (!result.has_value()) {
            return std::unexpected { result.error() };
        }
        auto& sem = result.value();
        struct _Helper {
            _Helper(Semaphore& sem)
                : m_sem(sem)
            {
                m_sem.Wait();
            }
            ~_Helper()
            {
                m_sem.Post();
            }
            Semaphore& m_sem;
        } _ { sem };

        // We now have exclusive access
        SharedBuffer buffer;
        auto shared_buffer_result = buffer.Initialize(channel_parameters.channel_name, getBufferSize(channel_parameters));
        if (!shared_buffer_result.has_value()) {
            return std::unexpected { result.error() };
        }
        if (reinterpret_cast<std::uintptr_t>(buffer.GetBuffer()) % alignof(ChannelHeader) != 0) {
            return std::unexpected(PikaError {
                .error_type = PikaErrorType::SharedRingBufferError,
                .error_message = "IntraProcessChannelMPMC::commonInit buffer is not aligned" });
        }
        auto channel_head_ptr = reinterpret_cast<ChannelHeader*>(buffer.GetBuffer());
        RingBuffer* ring_buffer_ptr = nullptr;
        if (channel_head_ptr->m_endpoint_count.load() > 0) {
            ring_buffer_ptr = &channel_head_ptr->ring_buffer;
        } else {
            auto ring_buffer_location_in_shared_buffer = &channel_head_ptr->ring_buffer;
            ring_buffer_ptr = new (ring_buffer_location_in_shared_buffer) RingBuffer(); // Construct inplace
            auto result = ring_buffer_ptr->Initialize(reinterpret_cast<uint8_t*>(&channel_head_ptr->data), sizeof(DataT), alignof(DataT), channel_parameters.queue_size, true);
            if (!result.has_value()) {
                return std::unexpected { result.error() };
            }
        }
        channel_head_ptr->m_endpoint_count.fetch_add(1);
        return std::make_pair(std::move(buffer), ring_buffer_ptr);
    }
    [[nodiscard]] static auto getBufferSize(ChannelParameters const& channel_parameters) -> uint64_t
    {
        return sizeof(ChannelHeader) + sizeof(DataT) * (channel_parameters.queue_size - 1); // The -1 here is because the channel header has one DataT
    }
};

#endif