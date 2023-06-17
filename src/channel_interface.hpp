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

#ifndef PIKA_CHANNEL_INTERFACE_HPP
#define PIKA_CHANNEL_INTERFACE_HPP

#include "error.hpp"

#include <__expected/unexpected.h>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <type_traits>

namespace pika {

template <typename T>
concept ChannelPacketType = std::is_pod_v<T>;

using DurationUs = uint64_t;
static constexpr DurationUs INFINITE_TIMEOUT = std::numeric_limits<DurationUs>::max();

struct ProducerImpl {
    virtual ~ProducerImpl() = default;
    virtual auto Connect() -> std::expected<void, PikaError> = 0;
    virtual auto Send(uint8_t const* const source_buffer, DurationUs timeout_duration)
        -> std::expected<void, PikaError>
        = 0;
    virtual auto GetSendSlot(DurationUs timeout_duration)
        -> std::expected<uint8_t* const, PikaError>
        = 0;
    virtual auto ReleaseSendSlot(uint8_t* slot) -> std::expected<void, PikaError> = 0;
    virtual auto IsConnected() -> bool = 0;
};

struct ConsumerImpl {
    virtual ~ConsumerImpl() = default;
    virtual auto Connect() -> std::expected<void, PikaError> = 0;
    virtual auto Receive(uint8_t* const destination_buffer, DurationUs timeout_duration)
        -> std::expected<void, PikaError>
        = 0;
    virtual auto GetReceiveSlot(DurationUs timeout_duration)
        -> std::expected<uint8_t const* const, PikaError>
        = 0;
    virtual auto ReleaseReceiveSlot(uint8_t const* const slot) -> std::expected<void, PikaError>
        = 0;
    virtual auto IsConnected() -> bool = 0;
};

template <ChannelPacketType DataT> struct Producer {
    auto Send(DataT const& packet, DurationUs timeout_duration = INFINITE_TIMEOUT)
        -> std::expected<void, PikaError>
    {
        return m_impl->Send(reinterpret_cast<uint8_t const*>(&packet), timeout_duration);
    }
    auto GetSendSlot(DurationUs timeout_duration = INFINITE_TIMEOUT)
        -> std::expected<DataT*, PikaError>
    {
        auto result = m_impl->GetSendSlot(timeout_duration);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        return reinterpret_cast<DataT* const>(result.value());
    }
    auto ReleaseSendSlot(DataT* const slot) -> std::expected<void, PikaError>
    {
        return m_impl->ReleaseSendSlot(reinterpret_cast<uint8_t* const>(slot));
    }

    auto Connect() -> std::expected<void, PikaError> { return m_impl->Connect(); }
    auto IsConnected() -> bool { return m_impl->IsConnected(); }

private:
    friend struct Channel;
    Producer(std::unique_ptr<ProducerImpl> impl)
        : m_impl(std::move(impl))
    {
    }
    std::unique_ptr<ProducerImpl> m_impl;
};

template <ChannelPacketType DataT> struct Consumer {
    auto Receive(DataT& packet, DurationUs timeout_duration = INFINITE_TIMEOUT)
        -> std::expected<void, PikaError>
    {
        return m_impl->Receive(reinterpret_cast<uint8_t*>(&packet), timeout_duration);
    }

    auto GetReceiveSlot(DurationUs timeout_duration = INFINITE_TIMEOUT)
        -> std::expected<DataT const* const, PikaError>
    {
        auto result = m_impl->GetReceiveSlot(timeout_duration);
        if (not result.has_value()) {
            return std::unexpected(result.error());
        }
        return reinterpret_cast<DataT const*>(result.value());
    }

    auto ReleaseReceiveSlot(DataT const* const packet_pointer) -> std::expected<void, PikaError>
    {
        return m_impl->ReleaseReceiveSlot(reinterpret_cast<uint8_t const* const>(packet_pointer));
    }

    auto Connect() -> std::expected<void, PikaError> { return m_impl->Connect(); }
    auto IsConnected() -> bool { return m_impl->IsConnected(); }

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
    uint64_t queue_size {};
    ChannelType channel_type;
    bool single_producer_single_consumer_mode = false;
};

struct Channel {
    static auto __CreateProducerImpl(ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment) -> std::expected<std::unique_ptr<ProducerImpl>, PikaError>;
    static auto __CreateConsumerImpl(ChannelParameters const& channel_params, uint64_t element_size,
        uint64_t element_alignment) -> std::expected<std::unique_ptr<ConsumerImpl>, PikaError>;

    template <ChannelPacketType DataT>
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

    template <ChannelPacketType DataT>
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
    template <ChannelPacketType DataT>
    static auto CreateProducerOnHeap(ChannelParameters const& channel_params)
        -> std::expected<std::unique_ptr<Producer<DataT>>, PikaError>
    {
        auto impl = __CreateProducerImpl(channel_params, sizeof(DataT), alignof(DataT));
        if (impl.has_value()) {
            return std::unique_ptr<Producer<DataT>>(new Producer<DataT> { std::move(*impl) });
        } else {
            return std::unexpected(impl.error());
        }
    }

    template <ChannelPacketType DataT>
    static auto CreateConsumerOnHeap(ChannelParameters const& channel_params)
        -> std::expected<std::unique_ptr<Consumer<DataT>>, PikaError>
    {
        auto impl = __CreateConsumerImpl(channel_params, sizeof(DataT), alignof(DataT));
        if (impl.has_value()) {
            return std::unique_ptr<Consumer<DataT>>(new Consumer<DataT> { std::move(*impl) });
        } else {
            return std::unexpected(impl.error());
        }
    }
    Channel() = delete;
};

} // namespace pika
#endif