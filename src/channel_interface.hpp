#ifndef PIKA_CHANNEL_INTERFACE_HPP
#define PIKA_CHANNEL_INTERFACE_HPP

#include "error.hpp"

#include <cstdint>
#include <expected>
#include <memory>

namespace pika {

struct ProducerImpl {
    virtual ~ProducerImpl() = default;
    virtual auto Connect() -> std::expected<void, PikaError> = 0;
    virtual auto Send(uint8_t const* const source_buffer, uint64_t size)
        -> std::expected<void, PikaError>
        = 0;
};

struct ConsumerImpl {
    virtual ~ConsumerImpl() = default;
    virtual auto Connect() -> std::expected<void, PikaError> = 0;
    virtual auto Receive(uint8_t* const destination_buffer, uint64_t destination_buffer_size)
        -> std::expected<void, PikaError>
        = 0;
};

template <typename DataT> struct Producer {
    auto Send(DataT const& packet) -> std::expected<void, PikaError>
    {
        return m_impl->Send(reinterpret_cast<uint8_t const*>(&packet), sizeof(packet));
    }

    auto Connect() -> std::expected<void, PikaError> { return m_impl->Connect(); }

private:
    friend struct Channel;
    Producer(std::unique_ptr<ProducerImpl> impl)
        : m_impl(std::move(impl))
    {
    }
    std::unique_ptr<ProducerImpl> m_impl;
};

template <typename DataT> struct Consumer {
    auto Receive(DataT& packet) -> std::expected<void, PikaError>
    {
        return m_impl->Receive(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    }

    auto Connect() -> std::expected<void, PikaError> { return m_impl->Connect(); }

private:
    friend struct Channel;
    Consumer(std::unique_ptr<ConsumerImpl> impl)
        : m_impl(std::move(impl))
    {
    }
    std::unique_ptr<ConsumerImpl> m_impl;
};

enum class ChannelType { InterProcess, InterThread };

struct ChannelParameters {
    std::string channel_name;
    uint64_t queue_size;
    ChannelType channel_type;
};

struct Channel {
    static auto __CreateProducerImpl(ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment) -> std::expected<std::unique_ptr<ProducerImpl>, PikaError>;
    static auto __CreateConsumerImpl(ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment) -> std::expected<std::unique_ptr<ConsumerImpl>, PikaError>;

    template <typename DataT>
    static auto CreateProducer(ChannelParameters const& channel_params)
        -> std::expected<Producer<DataT>, PikaError>
    {
        auto impl = __CreateProducerImpl(channel_params, sizeof(DataT), alignof(DataT));
        if (impl.has_value()) {
            return Producer<DataT> { std::move(*impl) };
        } else {
            return std::unexpected(impl.error());
        }
    }

    template <typename DataT>
    static auto CreateConsumer(ChannelParameters const& channel_params)
        -> std::expected<Consumer<DataT>, PikaError>
    {
        auto impl = __CreateConsumerImpl(channel_params, sizeof(DataT), alignof(DataT));
        if (impl.has_value()) {
            return Consumer<DataT> { std::move(*impl) };
        } else {
            return std::unexpected(impl.error());
        }
    }
    Channel() = delete;
};

} // namespace pika
#endif