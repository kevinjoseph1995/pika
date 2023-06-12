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
#ifndef PIKA_CHANNEL_HEADER_HPP
#define PIKA_CHANNEL_HEADER_HPP
#include "ring_buffer.hpp"
#include <atomic>

template <RingBufferType RingBuffer> struct ChannelHeader {
    std::atomic_bool registered = false;
    std::atomic_uint64_t producer_count = 0;
    std::atomic_uint64_t consumer_count = 0;
    bool single_producer_single_consumer_mode = false;
    RingBuffer ring_buffer;
};

template <RingBufferType RingBuffer>
[[nodiscard]] static constexpr auto GetRingBufferSlotsOffset(uint64_t element_alignment)
{
    PIKA_ASSERT(element_alignment % 2 == 0);
    if (element_alignment < sizeof(ChannelHeader<RingBuffer>)) {
        return ((sizeof(ChannelHeader<RingBuffer>) / element_alignment) + 1) * element_alignment;
    } else {
        return element_alignment;
    }
}

template <RingBufferType RingBuffer>
[[nodiscard]] constexpr auto GetBufferSize(
    uint64_t queue_size, uint64_t element_size, uint64_t element_alignment) -> uint64_t
{
    return GetRingBufferSlotsOffset<RingBuffer>(element_alignment) + (queue_size * element_size);
}

template <>
[[nodiscard]] constexpr auto GetBufferSize<RingBufferLockFree>(
    uint64_t queue_size, uint64_t element_size, uint64_t element_alignment) -> uint64_t
{
    return GetRingBufferSlotsOffset<RingBufferLockFree>(element_alignment)
        + ((queue_size + 1) * element_size);
}

#endif