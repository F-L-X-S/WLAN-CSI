#
# @file music-spectrum.py
# @author Felix Schuelke (flxscode@gmail.com)
# 
# @brief This file contains the implementation of the MUSIC-Algorithm for estimating the Direction-of-arrival (DoA)
# of a signal impinging on an antenna array. The Data is received via a ZeroMQ socket and is processed in real-time.
# The received Channel State Information (CSI) is formatted as a matrix with dimensions (size, n_boards, n_rows, n_antennas, subcarriers).
# The Implementation is based on the ESPARGOS demo project https://github.com/ESPARGOS/pyespargos
# @version 0.1
# @date 2025-05-20

import pathlib
import sys

sys.path.append(str(pathlib.Path(__file__).absolute().parents[2]))

import numpy as np

import PyQt6.QtWidgets
import PyQt6.QtCharts
import PyQt6.QtCore
import PyQt6.QtQml
import zmq
import struct

ANTENNAS_PER_ROW = 10

# ZMQ socket setup
context = zmq.Context()
socket = context.socket(zmq.PULL)
socket.connect("tcp://localhost:5555")
poller = zmq.Poller()
poller.register(socket, zmq.POLLIN)

# initalize received data
csi_all = None
print("Receiving Data...")

while True:
    socks = dict(poller.poll(timeout=3000))  # 3000 ms

    if socket in socks:
        msg = socket.recv()

        # Header: 2 x uint32 → 8 bytes
        num_channels, samples_per_channel = struct.unpack("II", msg[:8])

        # Data: complex values
        data = np.frombuffer(msg[8:], dtype=np.complex64)
        
        # Transform to  (num_channels, samples_per_channel) 
        reshaped = data.reshape((num_channels, samples_per_channel))

        # shape : (size, n_boards, n_rows, n_antennas, subcarriers)
        csi = reshaped[np.newaxis, np.newaxis, np.newaxis, :, :]

        # Stack on axis (size)
        if csi_all is None:
            csi_all = csi
        else:
            csi_all = np.concatenate((csi_all, csi), axis=0)
            
    else:
        if (csi_all is not None):
            print("Socket timeout - continuing with received data:\n")
            print(f"Received: {csi_all.shape=}")
            break
    
    


class EspargosDemoMusicSpectrum(PyQt6.QtWidgets.QApplication):
	def __init__(self, argv):
		super().__init__(argv)

		# Qt setup
		self.aboutToQuit.connect(self.onAboutToQuit)
		self.engine = PyQt6.QtQml.QQmlApplicationEngine()

		# Initialize MUSIC scanning angles, steering vectors, ...
		self.scanning_angles = np.linspace(-np.pi / 2, np.pi / 2, 180) 
		self.steering_vectors = np.exp(-1.0j * np.outer(np.pi * np.sin(self.scanning_angles), np.arange(ANTENNAS_PER_ROW))) #[-pi...pi] in 180 steps
		self.spatial_spectrum = None

	def exec(self):
		context = self.engine.rootContext()
		context.setContextProperty("backend", self)

		qmlFile = pathlib.Path(__file__).resolve().parent / "music-spectrum-ui.qml"
		self.engine.load(qmlFile.as_uri())
		if not self.engine.rootObjects():
			return -1

		return super().exec()

	@PyQt6.QtCore.pyqtSlot(PyQt6.QtCharts.QLineSeries, PyQt6.QtCharts.QValueAxis)
	def updateSpatialSpectrum(self, series, axis):

		# compute the covariance matrix
		R = np.einsum("dbris,dbrjs->ij", csi_all, np.conj(csi_all))
  
		# eigenvalue decomposition
		eig_val, eig_vec = np.linalg.eig(R)
  
		# sort eigenvalues and eigenvectors in decreasing order
		order = np.argsort(eig_val)[::-1]
  
		# ignore the eigenvector of the largest eigenvalue => noise subspace
		Qn = eig_vec[:,order][:,1:]
  
		# compute the spatial spectrum
		spatial_spectrum_linear = 1 / np.linalg.norm(np.einsum("ae,ra->er", np.conj(Qn), self.steering_vectors), axis = 0)
		spatial_spectrum_log = 20 * np.log10(spatial_spectrum_linear)

		axis.setMin(np.min(spatial_spectrum_log) - 1)
		axis.setMax(max(np.max(spatial_spectrum_log), axis.max()))

		data = [PyQt6.QtCore.QPointF(np.rad2deg(angle), power) for angle, power in zip(self.scanning_angles, spatial_spectrum_log)]
		series.replace(data)

	def onAboutToQuit(self):
		self.engine.deleteLater()

	@PyQt6.QtCore.pyqtProperty(list, constant=True)
	def scanningAngles(self):
		return self.scanning_angles.tolist()

app = EspargosDemoMusicSpectrum(sys.argv)
sys.exit(app.exec())