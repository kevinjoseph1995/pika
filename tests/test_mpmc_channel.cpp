#include "channel_interface.hpp"
#include "fmt/core.h"
#include "process_fork.hpp"

#include <expected>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

TEST(IPCChannel, BasicTest) {
  auto params =
      pika::ChannelParameters{.channel_name = "/test",
                              .queue_size = 4,
                              .channel_type = pika::ChannelType::InterProcess};
  auto result = pika::Channel::CreateConsumer<int>(params);
  ASSERT_TRUE(result.has_value()) << result.error().error_message;
}
