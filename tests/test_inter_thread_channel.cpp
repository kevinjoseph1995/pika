#include "channel_interface.hpp"
#include "error.hpp"
#include "test_utils.hpp"

#include <__expected/expected.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

TEST(InterThreadChannel, BasicTest)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterThread
    };
    auto result = pika::Channel::CreateConsumer<int>(params);
    ASSERT_TRUE(result.has_value()) << result.error().error_message;
}

TEST(InterThreadChannel, Connection)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterThread
    };
    auto result1 = pika::Channel::CreateConsumer<int>(params);
    ASSERT_TRUE(result1.has_value()) << result1.error().error_message;

    auto result2 = pika::Channel::CreateProducer<int>(params);

    ASSERT_TRUE(result1->Connect().has_value());
    ASSERT_TRUE(result2->Connect().has_value());
}

TEST(InterThreadChannel, TxRx)
{
    auto const params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterThread
    };
    auto const tx_data = GetRandomIntVector(100);
    auto thread = std::thread([&]() {
        auto producer = pika::Channel::CreateProducer<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return;
        }
        auto connect_result = producer->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return;
        }

        for (auto tx : tx_data) {
            auto send_result = producer->Send(static_cast<int>(tx));
            if (not send_result.has_value()) {
                fmt::println(stderr, "producer->Send Error: {}", send_result.error().error_message);
                return;
            }
        }
        return;
    });

    // Setup consumer
    auto consumer = pika::Channel::CreateConsumer<int>(params);
    auto connect_result = consumer->Connect();
    ASSERT_TRUE(connect_result.has_value()) << connect_result.error().error_message;

    size_t index = 0;
    while (true) {
        StopWatch watch;
        int recv_packet {};
        auto recv_result = consumer->Receive(recv_packet);
        ASSERT_TRUE(recv_result.has_value()) << recv_result.error().error_message;
        ASSERT_TRUE(recv_result.has_value()) << recv_result.error().error_message;
        ASSERT_TRUE(recv_packet == tx_data[index++]);
        if (index == tx_data.size() - 1) {
            break;
        }
        fmt::println("Rx cycle took:{} microseconds", watch.ElapsedDurationUs());
    }

    thread.join();
}

TEST(InterThreadChannel, TxRxLockFree)
{
    auto const params = pika::ChannelParameters { .channel_name = "/test",
        .queue_size = 1,
        .channel_type = pika::ChannelType::InterThread,
        .single_producer_single_consumer_mode = true };

    auto const tx_data = GetRandomIntVector(100);
    auto thread = std::thread([&]() {
        auto producer = pika::Channel::CreateProducer<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return;
        }
        auto connect_result = producer->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return;
        }

        for (auto tx : tx_data) {
            auto send_result = producer->Send(static_cast<int>(tx));
            if (not send_result.has_value()) {
                fmt::println(stderr, "producer->Send Error: {}", send_result.error().error_message);
                return;
            }
        }
        return;
    });

    // Setup consumer
    auto consumer = pika::Channel::CreateConsumer<int>(params);
    auto connect_result = consumer->Connect();
    ASSERT_TRUE(connect_result.has_value()) << connect_result.error().error_message;

    size_t index = 0;
    while (true) {
        StopWatch watch;
        int recv_packet {};
        auto recv_result = consumer->Receive(recv_packet);
        ASSERT_TRUE(recv_result.has_value()) << recv_result.error().error_message;
        ASSERT_TRUE(recv_result.has_value()) << recv_result.error().error_message;
        ASSERT_TRUE(recv_packet == tx_data[index++]) << "recv_packet=" << recv_packet;
        if (index == tx_data.size() - 1) {
            break;
        }
        fmt::println("Rx cycle took:{} microseconds", watch.ElapsedDurationUs());
    }

    thread.join();
}

TEST(InterThreadChannel, TxRxWithTimeouts)
{
    auto const params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterThread
    };
    auto const tx_data = GetRandomIntVector(100);
    auto thread = std::thread([&]() {
        auto producer = pika::Channel::CreateProducer<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return;
        }
        auto connect_result = producer->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return;
        }

        for (auto tx : tx_data) {
            auto send_result = std::expected<void, PikaError> {};
            while (true) {
                send_result = producer->Send(static_cast<int>(tx), 1000);
                if (send_result.has_value()
                    || send_result.error().error_type != PikaErrorType::Timeout) {
                    break;
                }
            }

            if (not send_result.has_value()) {
                fmt::println(stderr, "producer->Send Error: {}", send_result.error().error_message);
                return;
            }
        }
        return;
    });

    // Setup consumer
    auto consumer = pika::Channel::CreateConsumer<int>(params);
    auto connect_result = consumer->Connect();
    ASSERT_TRUE(connect_result.has_value()) << connect_result.error().error_message;

    size_t index = 0;
    while (true) {
        StopWatch watch;
        int recv_packet {};
        auto recv_result = std::expected<void, PikaError> {};
        while (true) {
            recv_result = consumer->Receive(recv_packet, 1000);
            if (recv_result.has_value()
                || recv_result.error().error_type != PikaErrorType::Timeout) {
                break;
            }
        }

        ASSERT_TRUE(recv_result.has_value()) << recv_result.error().error_message;
        ASSERT_TRUE(recv_result.has_value()) << recv_result.error().error_message;
        ASSERT_TRUE(recv_packet == tx_data[index++]);
        if (index == tx_data.size() - 1) {
            break;
        }
        fmt::println("Rx cycle took:{} microseconds", watch.ElapsedDurationUs());
    }

    thread.join();
}