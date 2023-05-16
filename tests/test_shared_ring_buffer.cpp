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
    SharedRingBuffer<bool> shared_ring_buffer;
    EXPECT_TRUE(shared_ring_buffer.Initialize(1));

    auto code = fork();
    static constexpr auto NUM_MESSAGES = 5;

    if (code == 0) {
        shared_ring_buffer.RegisterEndpoint();
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            {
                auto write_slot = shared_ring_buffer.GetWriteSlot();
                if (write_slot.has_value() && i == (NUM_MESSAGES - 1)) {
                    write_slot->GetElement() = true;
                } else {
                    write_slot->GetElement() = false;
                }
            }
        }
    } else {
        shared_ring_buffer.RegisterEndpoint();
        while (true) {
            {
                auto read_slot = shared_ring_buffer.GetReadSlot();
                if (read_slot.has_value()) {
                    if (read_slot.value().GetElement() == true) {
                        break;
                    } else {
                        fmt::println("Read value");
                    }
                }
            }
        }
    }
}
