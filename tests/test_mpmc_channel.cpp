#include "channel.hpp"
#include "fmt/core.h"
#include "process_fork.hpp"

#include <expected>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

TEST(ChannelMPMC, BasicTest)
{
    static constexpr auto NUM_MESSAGES = 10000;
    auto const channel_params = ChannelParameters { .channel_name = "/test", .queue_size = 4 };

    auto child_process_handle = ChildProcessHandle::RunChildFunction([&]() -> ChildProcessState {
        auto producer = IntraProcessChannelMPMC<int>::GetProducer(channel_params);
        if (not producer.has_value()) {
            fmt::println(stderr, "Child process failed with error:{}", producer.error().error_message);
            return ChildProcessState::FAIL;
        }
        for (int count = 1; count <= NUM_MESSAGES; ++count) {
            producer->Send(count);
        }
        return ChildProcessState::SUCCESS;
    });

    auto consumer = IntraProcessChannelMPMC<int>::GetConsumer(channel_params);
    ASSERT_TRUE(consumer.has_value()) << consumer.error().error_message;
    while (true) {
        int data {};
        consumer->Recv(data);
        if (data == NUM_MESSAGES) {
            break;
        }
    }
    auto res = child_process_handle->WaitForChildProcess();
    ASSERT_TRUE(res.has_value()) << res.error().error_message;
}
