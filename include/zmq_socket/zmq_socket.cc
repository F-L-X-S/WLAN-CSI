#include "zmq_socket.h"
#include <cstring>

ZmqSender::ZmqSender(const std::string& endpoint)
    : context_(1), socket_(context_, zmq::socket_type::push)
{
    socket_.bind(endpoint);  // e.g. "tcp://*:5555"
}

void ZmqSender::send(const std::vector<std::complex<float>>& data)
{
    const uint32_t numChannels = 1;
    const uint32_t samplesPerChannel = data.size();

    // Header: [numChannels, samplesPerChannel] → 2 × uint32_t
    std::vector<uint8_t> buffer(sizeof(uint32_t) * 2 + numChannels * samplesPerChannel * sizeof(std::complex<float>));

    // Pointer-Offset
    uint8_t* ptr = buffer.data();

    // Write Header
    std::memcpy(ptr, &numChannels, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &samplesPerChannel, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // Write Data
    std::memcpy(ptr, data.data(), samplesPerChannel * sizeof(std::complex<float>));
    ptr += samplesPerChannel * sizeof(std::complex<float>);


    // Send message
    zmq::message_t message(buffer.size());
    std::memcpy(message.data(), buffer.data(), buffer.size());
    socket_.send(message, zmq::send_flags::none);
}


void ZmqSender::send(const std::vector<std::vector<std::complex<float>>>& data)
{
    const uint32_t numChannels = data.size();
    const uint32_t samplesPerChannel = data[0].size();

    // Header: [numChannels, samplesPerChannel] → 2 × uint32_t
    std::vector<uint8_t> buffer(sizeof(uint32_t) * 2 + numChannels * samplesPerChannel * sizeof(std::complex<float>));

    // Pointer-Offset
    uint8_t* ptr = buffer.data();

    // Write Header
    std::memcpy(ptr, &numChannels, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &samplesPerChannel, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // Write Data
    for (const auto& ch : data) {
        std::memcpy(ptr, ch.data(), ch.size() * sizeof(std::complex<float>));
        ptr += ch.size() * sizeof(std::complex<float>);
    }

    // Send message
    zmq::message_t message(buffer.size());
    std::memcpy(message.data(), buffer.data(), buffer.size());
    socket_.send(message, zmq::send_flags::none);
}