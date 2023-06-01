# Pika: A basic message passing library

An experimental self learning project.

Usage:
##### Producer side on process 1
```cpp
auto const params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
};
auto producer = pika::Channel::CreateProducer<int>(params);
producer->Connect();
producer->Send(44);
```

##### Consumer side on process 2
```cpp
auto const params = pika::ChannelParameters {
        .channel_name = "/test", .queue_size = 4, .channel_type = pika::ChannelType::InterProcess
};
auto consumer = pika::Channel::CreateConsumer<int>(params);
consumer->Connect();
int recv_packet {};
consumer->Receive(recv_packet);
assert(recv_packet == 44);
```

![alt text](https://github.com/kevinjoseph1995/pika/blob/main/pika.jpg?raw=true)
