# doa4rfc
#### Realtime Direction-of-Arrival Estimation for RF Communication Protocols

## Objective  
This project aims to provide a flexible software architecture, to implement and test DoA methods for various RF communication protocols.

## Main Features
- Synchronized processing of multiple USRP RX streams in separated threads
- Frame detection and synchronization based on [Liquid-DSP](https://liquidsdr.org)

- Forwarding of synchronously detected CFRs to the python implementation of the DoA algorithm via a [ZMQ](https://zeromq.org/languages/cplusplus/) socket
- DoA estimation by spectral MUSIC (multiple signal classification) in python 
- MATLAB export to plot CFR and constellation diagrams

## Channel Frequency Response and Channel State Information
A received signal can be described as the convolution of the transmitted signal $s(t)$ and the channel-impulse-response (CIR) $h(t)$:
<br>

$$r(t)=s(t)\circledast h(t)$$ 

<br>
Therefore the channel-frequency-response (transfer-function of the channel) is defined as  [^1]:
<br>

$$H(\omega)=\frac{R(\omega)}{S(\omega)}$$

<br>

### Fundamentals of subspace based DoA Estimation
...

### Channel State Information in OFDM-systems
Orthogonal frequency division multiplexing (OFDM) divides the allocated bandwidth into a number of subcarriers for transmitting data in parallel, but with a lower symbol-rate in each stream. Aim of this procedure is, to reduce the likelihood of symbol-interference within the seperated streams. The attenuation and phase of a single subcarrier represent a sample of the CFR at the center-frequency of the respective subcarrier:
<br>

$$H(f_k)=||H(f_k)||e^{\angle H(f_k)}$$

<br>All CFR-samples in total are handles as the channel-state-information  [^1]:
<br>

$$H=\{ H(f_k)|j\in [1, K]|, K\in \mathbb{N} \}$$

<br>

## Hardware Setup 
The software is tested using two USRP N210 with the WBXv3 daughterboard. Phase synchronization is achieved with the MIMO-cable. The USRPs are connected to the host by separate ethernet interfaces. For utilizing a different type of SDRs, the interfaces can be implemented in separated threads similar to `multi_rx.h`.  <br>
One USRP is used for transmitting and receiving the OFDM packages while the other USRP is used in RX-mode only. The MUSIC-spectrum visualizes the position of the TX-antenna. 
 <br>
Make sure, the receiving antennas are spaced by the half wavelength of the carrier frequency (e.g. 12cm for a carrier of 1.25GHz).

## Installation 
1. Clone the Repo to your local machine
2. Setup a virtual environment within the `./music/`directory <br>
   ```
   cd ./music
   python -m venv env
   ```
3. Install all python dependencies specified in `requirements.txt` <br>
   ```
    source env/bin/activate
    pip install -e . 
   ``` 
4. Use the CMake extension to configure the project 
5. Set `doa4rfc` as target for build and execution (or any example-file)
6. Go to the vscode "run and debug" menu and start the `Debug (Clang CMake Preset)` task to build and run the specified target 
<br><br>

Make sure, that all USRPs are connected via separate Ethernet interfaces, since the datarate can possibly cause overflows in the shared-Etehrnet mode. Check the USRP connection by running `uhd_find_devices`. 

## Main Dependencies
- [ZMQ](https://zeromq.org/languages/cplusplus/) for socket communication with the Python-implemented DoA Algorithm 
- [Liquid-DSP](https://liquidsdr.org) for frame-detection, generation and synchronization
- [UHD](https://files.ettus.com/manual/index.html) for USRP communication

## References
[^1]: Zheng Yang, Yi Zhang, Guoxuan Chi, Guidong Zhang, "Hands-on Wireless Sensing with Wi-Fi: A Tutorial" tns.thss.tsinghua.edu.cn, 2023, https://tns.thss.tsinghua.edu.cn/wst/docs/pre/
(accessed April 4, 2025)

[^2]: "IEEE Standard for Information technology--Telecommunications and information exchange between systems Local and metropolitan area networks--Specific requirements Part 11: Wireless LAN Medium Access Control (MAC) and Physical Layer (PHY) Specifications," in IEEE Std 802.11-2012 (Revision of IEEE Std 802.11-2007) , vol., no., pp.1-2793, 29 March 2012, doi: 10.1109/IEEESTD.2012.6178212.

[^3]: Lin, N., Yun, Z., Zhou, S., & Han, S. (2025). GR-WiFi: A GNU Radio based WiFi Platform with Single-User and Multi-User MIMO Capability. ArXiv, abs/2501.06176.