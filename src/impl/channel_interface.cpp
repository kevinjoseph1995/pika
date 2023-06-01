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