#
# @file zmq-socket.py
# @author Felix Schuelke (flxscode@gmail.com)
# 
# @brief This file contains the implementation of the zmq socket for receiving data from a multisync object.
# @date 2025-05-20

import zmq
import numpy as np
import struct

context = zmq.Context()
socket = context.socket(zmq.PULL)
socket.connect("tcp://localhost:5555")

while True:
    msg = socket.recv()

    # Header: 2 x uint32 → 8 bytes
    num_channels, samples_per_channel = struct.unpack("II", msg[:8])

    # Data: complex values
    data = np.frombuffer(msg[8:], dtype=np.complex64)

    # Transform to  (num_channels, samples_per_channel) 
    reshaped = data.reshape((num_channels, samples_per_channel))

    print(f"Received: {num_channels} \t Samples per Channel:{samples_per_channel}")
    print(reshaped)
