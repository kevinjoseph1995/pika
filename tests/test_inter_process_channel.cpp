#include "channel_interface.hpp"
#include "process_fork.hpp"
#include "test_utils.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

TEST(InterProcessChannel, BasicTest)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto result = pika::Channel::CreateConsumer<int>(params);
    ASSERT_TRUE(result.has_value()) << result.error().error_message;
}

TEST(InterProcessChannel, Connection)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto result1 = pika::Channel::CreateConsumer<int>(params);
    ASSERT_TRUE(result1.has_value()) << result1.error().error_message;

    auto result2 = pika::Channel::CreateProducer<int>(params);

    ASSERT_TRUE(result1->Connect().has_value());
    ASSERT_TRUE(result2->Connect().has_value());

    ASSERT_TRUE(result1->IsConnected());
    ASSERT_TRUE(result2->IsConnected());
}

TEST(InterProcessChannel, Disconnect1)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto child_process_handle = ChildProcessHandle::RunChildFunction([&]() -> ChildProcessState {
        auto producer = pika::Channel::CreateProducerOnHeap<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return ChildProcessState::FAIL;
        }
        auto connect_result = (*producer)->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return ChildProcessState::FAIL;
        }
        (*producer).reset(nullptr);
        return ChildProcessState::SUCCESS;
    });
    ASSERT_TRUE(child_process_handle.has_value()) << child_process_handle.error().error_message;
    // Setup consumer
    auto consumer = pika::Channel::CreateConsumer<int>(params);
    auto connect_result = consumer->Connect();
    ASSERT_TRUE(connect_result.has_value()) << connect_result.error().error_message;
    std::this_thread::sleep_for(1ms); // Give enough time to destroy the producer
    ASSERT_FALSE(consumer->IsConnected());
}

TEST(InterProcessChannel, Disconnect2)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto child_process_handle = ChildProcessHandle::RunChildFunction([&]() -> ChildProcessState {
        auto consumer = pika::Channel::CreateConsumerOnHeap<int>(params);
        if (not consumer.has_value()) {
            fmt::println(stderr, "{}", consumer.error().error_message);
            return ChildProcessState::FAIL;
        }
        auto connect_result = (*consumer)->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return ChildProcessState::FAIL;
        }
        (*consumer).reset(nullptr);
        return ChildProcessState::SUCCESS;
    });
    ASSERT_TRUE(child_process_handle.has_value()) << child_process_handle.error().error_message;
    // Setup consumer
    auto producer = pika::Channel::CreateProducer<int>(params);
    auto connect_result = producer->Connect();
    ASSERT_TRUE(connect_result.has_value()) << connect_result.error().error_message;
    std::this_thread::sleep_for(1ms); // Give enough time to destroy the consumer
    ASSERT_FALSE(producer->IsConnected());
}

TEST(InterProcessChannel, TxRx)
{
    auto const params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto const tx_data = GetRandomIntVector(100);
    auto child_process_handle = ChildProcessHandle::RunChildFunction([&]() -> ChildProcessState {
        auto producer = pika::Channel::CreateProducer<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return ChildProcessState::FAIL;
        }
        auto connect_result = producer->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return ChildProcessState::FAIL;
        }
        for (auto tx : tx_data) {
            auto send_result = producer->Send(static_cast<int>(tx));
            if (not send_result.has_value()) {
                fmt::println(stderr, "producer->Send Error: {}", send_result.error().error_message);
                return ChildProcessState::FAIL;
            }
        }
        return ChildProcessState::SUCCESS;
    });
    ASSERT_TRUE(child_process_handle.has_value()) << child_process_handle.error().error_message;
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
        ASSERT_TRUE(recv_packet == tx_data[index++]);
        if (index == tx_data.size() - 1) {
            break;
        }
        fmt::println("Rx cycle took:{} microseconds", watch.ElapsedDurationUs());
    }

    auto child_process_exit_status = child_process_handle->WaitForChildProcess();
    ASSERT_TRUE(child_process_exit_status.has_value())
        << child_process_handle.error().error_message;
}

TEST(InterProcessChannel, TxRxLockFree)
{
    auto const params = pika::ChannelParameters { .channel_name = "/test",
        .queue_size = 4,
        .channel_type = pika::ChannelType::InterProcess,
        .single_producer_single_consumer_mode = true };
    auto const tx_data = GetRandomIntVector(100);
    auto child_process_handle = ChildProcessHandle::RunChildFunction([&]() -> ChildProcessState {
        auto producer = pika::Channel::CreateProducer<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return ChildProcessState::FAIL;
        }
        auto connect_result = producer->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return ChildProcessState::FAIL;
        }
        for (auto tx : tx_data) {
            auto send_result = producer->Send(static_cast<int>(tx));
            if (not send_result.has_value()) {
                fmt::println(stderr, "producer->Send Error: {}", send_result.error().error_message);
                return ChildProcessState::FAIL;
            }
        }
        return ChildProcessState::SUCCESS;
    });
    ASSERT_TRUE(child_process_handle.has_value()) << child_process_handle.error().error_message;
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
        ASSERT_TRUE(recv_packet == tx_data[index++]);
        if (index == tx_data.size() - 1) {
            break;
        }
        fmt::println("Rx cycle took:{} microseconds", watch.ElapsedDurationUs());
    }

    auto child_process_exit_status = child_process_handle->WaitForChildProcess();
    ASSERT_TRUE(child_process_exit_status.has_value())
        << child_process_handle.error().error_message;
}

TEST(InterProcessChannel, TxRxWithTimeouts)
{
    auto const params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto const tx_data = GetRandomIntVector(100);
    auto child_process_handle = ChildProcessHandle::RunChildFunction([&]() -> ChildProcessState {
        auto producer = pika::Channel::CreateProducer<int>(params);
        if (not producer.has_value()) {
            fmt::println(stderr, "{}", producer.error().error_message);
            return ChildProcessState::FAIL;
        }
        auto connect_result = producer->Connect();
        if (not connect_result.has_value()) {
            fmt::println(stderr, "{}", connect_result.error().error_message);
            return ChildProcessState::FAIL;
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
                return ChildProcessState::FAIL;
            }
        }
        return ChildProcessState::SUCCESS;
    });
    ASSERT_TRUE(child_process_handle.has_value()) << child_process_handle.error().error_message;
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
        ASSERT_TRUE(recv_packet == tx_data[index++]);
        if (index == tx_data.size() - 1) {
            break;
        }
        fmt::println("Rx cycle took:{} microseconds", watch.ElapsedDurationUs());
    }

    auto child_process_exit_status = child_process_handle->WaitForChildProcess();
    ASSERT_TRUE(child_process_exit_status.has_value())
        << child_process_handle.error().error_message;
}