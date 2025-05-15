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

private:
    zmq::context_t context_;
    zmq::socket_t socket_;
};

#endif // ZMQ_SOCKET_H