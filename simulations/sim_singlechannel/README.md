# Simulation of CFR Estimation in Single-Channel Frame Detection

To validate the approach of exporting the CFR as the gain estimate from the ofdmframesync module, the simulation simulations/sim_singlechannel/sim_singlechannel.cc was developed. This simulation illustrates the effect of a time delay on the CFR of a synchronized OFDM frame, observed as a linear phase shift across the subcarriers. For further analysis and visualization, the generated complex baseband sequence, the CFR, the estimated CFO, and the detected data symbols are exported using a custom MATLAB code generator (defined in ./include/matlab\_export/matlab\_export.h), which outputs variables and commands to a specified .m file. The simulation performs the following steps: 

- Generation of an OFDM frame with QPSK data symbols in the complex baseband domain
- Embedding of the OFDM frame's time-domain sequence within a longer signal sequence to emulate transmission
- Simulation of noise effects using Liquid-DSP’s \texttt{channel\_cccf} module
- Application of a fractional delay filter to model time-delay effects
- Frame synchronization with the modified ofdmframesync module
- Extraction and export of the CFR and the first OFDM symbol 
- Generation of MATLAB script to plot the received time-domain sequence, the CFR, the estimated \acrshort{cfo} and the detected data symbols

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



The expected gain on subcarrier $k$ in the CFR estimated after detection of the LTF, is described as follows. The total signal power 
<br>
$
|X|^2 = 40dB-90dB = -50 dB = 1*10^{-5} \mathrm{V^2}
$
<br>
is distributed over the active subcarriers $M_{\mathrm{pilot}} + M_{\mathrm{data}}$. The resulting signal power on subcarrier $k$ is therefore
<br>
$
|X_k|^2 = \frac{|X|^2}{M_{\mathrm{pilot}} + M_{\mathrm{data}}} = \frac{10^{-5}\,\mathrm{V^2}}{6+44} = 2.0 * 10^{-7}\,\mathrm{V^2}
$
<br>
and the corresponding signal amplitude is
<br>
$
|X_k| = \sqrt{|X_k|^2} = 4.4721 * 10^{-4}\,\mathrm{V}
$
<br>
The CFR gain on subcarrier $k$, $|H_k|$, is defined as the ratio of the signal amplitude $|X_k|$ to the amplitude of the training symbol $|S_k|$. Because the FFT in Liquid-DSP is normalized over the subcarriers, an additional factor of $M$ must be applied to obtain the expected CFR gain as output by the synchronizer:
<br>
$
|H_k| = \frac{|X_k|}{|S_k|} * M
        = (4.4721 * 10^{-4}) * 64 
        = 0.0286
$
<br>
Since the CFRs phase response is expected to be linear across the enabled frequency band, calculating the phase shift for two subcarriers is sufficient to determine the expected behavior. For the calculations in the complex baseband, a normalized time unit $[T_n]$ equivalent to a single baseband sample time is defined. For $M$ subcarriers, $M+\text{cp}$ samples are transmitted in the time domain, resulting in a normalized sample rate of the baseband sequence of
<br>
$
R_S = \frac{1}{M+\text{cp}} = \frac{1}{64+16} = 0.0125 \quad[T_n^{-1}]
$
<br>
This results in a normalized subcarrier frequency spacing $\Delta f_n$ in the complex baseband domain of
<br>
$
\Delta f_n = \frac{R_S}{M} = \frac{0.0125}{64} = 1.9531 \times 10^{-4}  \quad[T_n^{-1}]
$
<br>
Considering a \texttt{DELAY} of $0.5$ samples applied to the fractional delay filter, the normalized time delay is
<br>
$
\tau_n = -0.5 / R_S = -0.5 * (M+\text{cp}) = -40 \quad[T_n]
$
<br>
For two representative subcarriers at $k = \pm 15$, the resulting phase can be calculated according to Eq. (\ref{eq:phaseshift}) as
<br>
$
\Delta\phi_{-15} = 2\pi \cdot (-15) \cdot 1.9531 \times 10^{-4} \cdot (-40) = 0.7363\,\mathrm{rad}
$
<br>
$
\Delta\phi_{15} = 2\pi \cdot 15 \cdot 1.9531 \times 10^{-4} \cdot (-40) = -0.7363\,\mathrm{rad}
$
<br>
The CFR for the simulation parameters defined in Table \ref{fig:sim_singlechannel_parameters} with a fractional delay of 0.5 samples is expected to show an equal subcarrier gain of $|H_k|=0.0286$ for all $M_{data}+M_{pilot}$ subcarriers in the transmission bandwidth and a linear phase shift that intersects $\Delta\phi_{-15}$ and $\Delta\phi_{15}$. 
<br>

### Generated baseband sequence with the specified channel impairments
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc" alt="sim_singlechannel_signal.svg" style="width:100%;">

### Estimated CFR 
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc" alt="sim_singlechannel_signal.svg" style="width:100%;">
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc" alt="sim_singlechannel_signal.svg" style="width:100%;">

### Detected Datasymbols 
(only first OFDM symbol)
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc" alt="sim_singlechannel_signal.svg" style="width:100%;">