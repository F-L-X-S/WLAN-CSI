/**
 * @file zmq_socket.cc
 * @author Felix Schuelke (flxscode@gmail.com)
 * 
 * @brief This file contains the definition of the ZmqSender class, which is used to send vector-formatted data via ZeroMQ sockets.
 * 
 * @version 0.1
 * @date 2025-05-20
 * 
 * 
 */

#include "zmq_socket.h"
#include <cstring>

ZmqSender::ZmqSender(const std::string& endpoint)
    : context_(1), socket_(context_, zmq::socket_type::push)
{
    socket_.bind(endpoint);  // e.g. "tcp://*:5555"
}

/**
 * @brief Send single CSI measurement of single channels
 * 
 * @param data 1-D vector [samplesPerChannel]
 */
void ZmqSender::send(const std::vector<std::complex<float>>& data)
{
    std::vector<std::vector<std::vector<std::complex<float>>>> wrappedData(1); 
    wrappedData[0].resize(1);  
    wrappedData[0][0] = data;  
    send(wrappedData);
}

/**
 * @brief Send single CSI measurement of multiple channels
 * 
 * @param data 2-D vector [numChannels, samplesPerChannel]
 */
void ZmqSender::send(const std::vector<std::vector<std::complex<float>>>& data)
{
    std::vector<std::vector<std::vector<std::complex<float>>>> wrappedData(1, data);
    send(wrappedData);
}

/**
 * @brief Send multiple CSI measurements of multiple channels
 * 
 * @param data 3-D vector [numMeasurements, numChannels, samplesPerChannel]
 */
void ZmqSender::send(const std::vector<std::vector<std::vector<std::complex<float>>>>& data)
{
    const uint32_t numMeasurements = data.size();
    const uint32_t numChannels = data[0].size();
    const uint32_t samplesPerChannel = data[0][0].size();

    // Header: [numMeasurements, numChannels, samplesPerChannel] → 3 × uint32_t
    std::vector<uint8_t> buffer(sizeof(uint32_t) * 3 + numMeasurements * numChannels * samplesPerChannel * sizeof(std::complex<float>));

    // Pointer-Offset
    uint8_t* ptr = buffer.data();

    // Write Header
    std::memcpy(ptr, &numMeasurements, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &numChannels, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &samplesPerChannel, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // Write Data
    for (const auto& measurement : data) {
        for (const auto& ch : measurement) {
            std::memcpy(ptr, ch.data(), ch.size() * sizeof(std::complex<float>));
            ptr += ch.size() * sizeof(std::complex<float>);
        }
    }

    // Send message
    zmq::message_t message(buffer.size());
    std::memcpy(message.data(), buffer.data(), buffer.size());
    socket_.send(message, zmq::send_flags::none);
}