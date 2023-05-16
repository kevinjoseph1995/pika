#include "error.hpp"
#include "fmt/core.h"
#include "shared_ring_buffer.hpp"
#include "gtest/gtest.h"
#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

TEST(SharedRingBuffer, Construction)
{
    using namespace std::chrono_literals;
    SharedRingBuffer<std::array<int, 100000000>> shared_ring_buffer;
    EXPECT_TRUE(shared_ring_buffer.Initialize(1));

    auto code = fork();
    static constexpr auto NUM_MESSAGES = 1000;

    if (code == 0) {
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            {
                auto write_slot = shared_ring_buffer.GetWriteSlot();
                if (write_slot.has_value()) {
                    *write_slot->GetElement().end() = i;
                    i = i + 1;
                    fmt::println("Write:{}", i);
                }
            }
        }
        exit(0);
    } else {
        while (true) {
            {
                auto read_slot = shared_ring_buffer.GetReadSlot();
                if (read_slot.has_value()) {
                    fmt::println("Read:{}", *read_slot->GetElement().end());
                    if (*read_slot->GetElement().end() == NUM_MESSAGES - 1) {
                        break;
                    }
                }
            }
        }
    }
}
