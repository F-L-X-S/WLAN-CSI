/**
 * @file zmq_socket.h
 * @author Felix Schuelke (flxscode@gmail.com)
 * 
 * @brief This file contains the definition of the ZmqSender class, which is used to send vector-formatted data via ZeroMQ sockets.
 * 
 * @version 0.1
 * @date 2025-05-20
 * 
 * 
 */

#ifndef ZMQ_SOCKET_H
#define ZMQ_SOCKET_H

#include <zmq.hpp>
#include <vector>
#include <complex>

class ZmqSender {
public:
    ZmqSender(const std::string& endpoint);
    void send(const std::vector<std::complex<float>>& data);
    void send(const std::vector<std::vector<std::complex<float>>>& data);
    void send(const std::vector<std::vector<std::vector<std::complex<float>>>>& data);

private:
    zmq::context_t context_;
    zmq::socket_t socket_;
};

#endif // ZMQ_SOCKET_H