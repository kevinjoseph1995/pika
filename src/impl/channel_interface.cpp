#include "channel_interface.hpp"

#include "error.hpp"
#include "inter_process_channel.hpp"

namespace pika {
auto Channel::__CreateConsumerImpl(ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment) -> std::expected<std::unique_ptr<ConsumerImpl>, PikaError>
{
    switch (channel_params.channel_type) {
    case ChannelType::InterProcess:
        return InterProcessConsumer::Create(channel_params, element_size, element_alignment);
    case ChannelType::InterThread:
        TODO("ChannelType::InterThread");
        break;
    }
}

auto Channel::__CreateProducerImpl(ChannelParameters const& channel_params, uint64_t element_size,
    uint64_t element_alignment) -> std::expected<std::unique_ptr<ProducerImpl>, PikaError>
{
    switch (channel_params.channel_type) {
    case ChannelType::InterProcess:
        return InterProcessProducer::Create(channel_params, element_size, element_alignment);
    case ChannelType::InterThread:
        TODO("ChannelType::InterThread");
        break;
    }
}
} // namespace pika