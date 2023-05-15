#include "error.hpp"
#include "shared_ring_buffer.hpp"
#include "gtest/gtest.h"
#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

TEST(SharedRingBuffer, Construction)
{
    SharedRingBuffer<int> shared_ring_buffer;
    EXPECT_TRUE(shared_ring_buffer.Initialize(10));

    auto code = fork();
    if (code == 0) {
        int volatile i = 1;
        while (i) {
            {
                auto write_slot = shared_ring_buffer.GetWriteSlot();
                if (write_slot.has_value()) {
                    write_slot->GetElement() = i;
                    i = i + 1;
                }
            }
        }
    } else {
        int volatile j = 1;
        while (j) {
            auto read_slot = shared_ring_buffer.GetReadSlot();
            if (read_slot.has_value()) {
                std::cout << read_slot->GetElement() << std::endl;
            }
        }
    }
}
