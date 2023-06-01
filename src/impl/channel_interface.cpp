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
#include "channel_interface.hpp"

#include "backing_storage.hpp"
#include "channel_internal.hpp"
#include "error.hpp"

namespace pika {
auto Channel::__CreateConsumerImpl(ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment) -> std::expected<std::unique_ptr<ConsumerImpl>, PikaError>
{
    switch (channel_params.channel_type) {
    case ChannelType::InterProcess:
        return ConsumerInternal<InterProcessSharedBuffer>::Create(
            channel_params, element_size, element_alignment);
    case ChannelType::InterThread:
        return ConsumerInternal<InterThreadSharedBuffer>::Create(
            channel_params, element_size, element_alignment);
    }
}

auto Channel::__CreateProducerImpl(ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment) -> std::expected<std::unique_ptr<ProducerImpl>, PikaError>
{
    switch (channel_params.channel_type) {
    case ChannelType::InterProcess:
        return ProducerInternal<InterProcessSharedBuffer>::Create(
            channel_params, element_size, element_alignment);
    case ChannelType::InterThread:
        return ProducerInternal<InterThreadSharedBuffer>::Create(
            channel_params, element_size, element_alignment);
    }
}
} // namespace pika