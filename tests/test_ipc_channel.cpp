#include "channel_interface.hpp"
#include "fmt/core.h"
#include "process_fork.hpp"

#include <expected>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

TEST(IPCChannel, BasicTest)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto result = pika::Channel::CreateConsumer<int>(params);
    ASSERT_TRUE(result.has_value()) << result.error().error_message;
}

TEST(IPCChannel, Connection)
{
    auto params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
    };
    auto result1 = pika::Channel::CreateConsumer<int>(params);
    ASSERT_TRUE(result1.has_value()) << result1.error().error_message;

    auto result2 = pika::Channel::CreateProducer<int>(params);

    ASSERT_TRUE(result1->Connect().has_value());
    ASSERT_TRUE(result2->Connect().has_value());
}
