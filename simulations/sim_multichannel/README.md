# Simulation of Multi-Channel CFR Estimation

The functionality of the implemented ```MultiSync``` class is validated through a [multi-channel simulation](sim_multichannel.cc), which builds upon the [single-channel simulation](../sim_singlechannel/README.md). 
The simulation performs the following steps:

- Generation of an OFDM frame containing QPSK data symbols in the complex baseband domain  
- Insertion of the OFDM frame’s time-domain sequence into a longer sequence to simulate transmission  
- Duplication of the signal sequence across multiple channels  
- Introduction of fractional delays and unique noise for each channel  
- Execution of frame synchronization on each channel using the ```MultiSync``` module  
- Extraction and export of the CFR
- Generation of MATLAB script to visualize the received time-domain signal and the CFR

Chosen simulation parameters:
| Parameter                              | Symbol           | Value  |
|--------------------------------------|------------------|--------|
| Number of subcarriers                | $M$            | 64     |
| Number of pilot subcarriers          | $M_{\text{pilot}}$ | 6      |
| Number of data subcarriers           | $M_{\text{data}}$  | 44     |
| Number of subcarriers used in the STF | $M_{S0}$        | 24     |
| Number of subcarriers used in the LTF | $M_{S1}$        | 50     |
| CP length                           | $cp$           | 16     |
| Noise Floor                        | $\sigma^2$     | -90 dB |
| Signal-to-Noise Ratio               | $SNR_{dB}$     | 40 dB  |

### Expected Subcarrier Gain
The expected gain on subcarrier $k$ in the CFR estimated after detection of the LTF, is described as follows. The total signal power 
 
 ```math
|X|^2 = 40dB-90dB = -50 dB = 1*10^{-5} \mathrm{V^2}
 ```
 
is distributed over the active subcarriers $M_{\mathrm{pilot}} + M_{\mathrm{data}}$. The resulting signal power on subcarrier $k$ is therefore
 
```math
|X_k|^2 = \frac{|X|^2}{M_{\mathrm{pilot}} + M_{\mathrm{data}}} = \frac{10^{-5}\,\mathrm{V^2}}{6+44} = 2.0 * 10^{-7}\,\mathrm{V^2}
```
 
and the corresponding signal amplitude is
 
```math
|X_k| = \sqrt{|X_k|^2} = 4.4721 * 10^{-4}\,\mathrm{V}
```
 
The CFR gain on subcarrier $k$, $|H_k|$, is defined as the ratio of the signal amplitude $|X_k|$ to the amplitude of the training symbol $|S_k|$. Because the FFT in Liquid-DSP is normalized over the subcarriers, an additional factor of $M$ must be applied to obtain the expected CFR gain as output by the synchronizer:
 
```math
|H_k| = \frac{|X_k|}{|S_k|} * M
        = (4.4721 * 10^{-4}) * 64 
        = 0.0286
```

### Expected Subcarrier Phase
Since the CFRs phase response is expected to be linear across the enabled frequency band, calculating the phase shift for two subcarriers is sufficient to determine the expected behavior. For the calculations in the complex baseband, a Sample-rate of $R_s=1 Hz$ is defined. 
#### Frequency Spacing $_\Delta f$
$R_s=1 Hz$ results in a subcarrier frequency spacing $_\Delta f$ in the complex baseband domain of
 
```math
_\Delta f = \frac{R_S}{M} = \frac{1}{64} \quad[Hz]
```
#### Time-Delay $\tau$ in $[seconds]$
For $R_S=1Hz$, a time-delay of $\Tau$ in $[samples]$ results in a delay $\tau$ in $[s]$ as
```math
\tau=\Tau*\frac{1}{R_s} \qquad\to\quad \tau\quad[seconds]=\Tau\quad[samples]
```
Considering a ```DELAY``` of $0.1$ samples and a ```DDELAY``` of $0.1$ samples applied to the fractional delay filter results in channel delays in multiples of $0.1$ samples. 

#### Subcarrier-Frequency $f_k$
Since no carrier modulation is performed, the total frequency $f_k$ at subcarrier $k$ is given by
```math
f_k = k*_\Delta f = \frac{k}{M}
```
#### Subcarrier CFR Phase $_\Delta\phi_k$
An impinging wave is received at phase $\phi=2\pi f \tau$ after $\tau$ seconds. The expected training-symbols phase sets the reference as $\phi=0$, the phase-shift $_\Delta f_k$ at subcarrier $k$ is then given as 
```math
_\Delta \phi_k=2\pi*f_k*\tau=2\pi*\frac{k}{M}*\Tau
```

#### Example calculation of the expected phase shift for  Channel 0 
At two representative subcarriers at $k = \pm 15$:  
```math
\Delta\phi_{-15} = 2\pi \cdot \frac{-15}{64}\cdot*(-0.1) = 0.1473\,\mathrm{rad}
```
 
```math
\Delta\phi_{+15} = 2\pi \cdot \frac{15}{64}\cdot*(-0.1) = -0.1473\,\mathrm{rad}
```

The corresponding expected phase shifts calculated for the remaining channels:
|                       | Channel 0 | Channel 1 | Channel 2 | Channel 3 |
|-----------------------|-----------|-----------|-----------|-----------|
| Delay $[Samples]$     | 0.1       | 0.2       | 0.3       | 0.4       |
| $\Delta\phi_{-15}\quad [rad]$      | 0.1473    | 0.2945    | 0.4418    | 0.5890    |
| $\Delta\phi_{15} \quad [rad]$         | -0.1473   | -0.2945   | -0.4418   | -0.5890   |

Consequently, the CFR for the shown simulation parameters is expected to show an equal subcarrier gain of $|H_k|=0.0286$ for all $M_{data}+M_{pilot}$ subcarriers in the transmission bandwidth and a linear phase shift that intersects $\Delta\phi_{-15}$ and $\Delta\phi_{15}$. 


It should be noted that the fractional delay filter ```fdelay_crcf``` causes phase-distortion for certain delay values, resulting in unexpected phase-offsets in the CFR. The delays are chosen to avoid this distortion.

### Generated baseband sequences with the specified channel impairments
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_multichannel/sim_multichannel_signal.svg" alt="sim_multichannel_signal.svg" style="width:90%;">

### Estimated CFR 
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_multichannel/sim_multichannel_cfr.svg" alt="sim_multichannel_cfr.svg" style="width:75%;">
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_multichannel/sim_multichannel_cfr_complex.svg" alt="sim_multichannel_cfr_complex.svg" style="width:75%;">