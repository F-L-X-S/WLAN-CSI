# Simulation of Single-Channel CFR Estimation

To validate the approach of exporting the CFR as the gain estimate from the ofdmframesync module, the simulation simulations/sim_singlechannel/sim_singlechannel.cc was developed. This simulation illustrates the effect of a time delay on the CFR of a synchronized OFDM frame, observed as a linear phase shift across the subcarriers. For further analysis and visualization, the generated complex baseband sequence, the CFR, the estimated CFO, and the detected data symbols are exported using a custom MATLAB code generator (defined in ./include/matlab_export/matlab_export.h), which outputs variables and commands to a specified .m file. The simulation performs the following steps: 

- Generation of an OFDM frame with QPSK data symbols in the complex baseband domain
- Embedding of the OFDM frame's time-domain sequence within a longer signal sequence to emulate transmission
- Simulation of noise effects using Liquid-DSP’s ```channel_cccf``` module
- Application of a fractional delay filter to model time-delay effects
- Frame synchronization with the modified ofdmframesync module
- Extraction and export of the CFR and the first OFDM symbol 
- Generation of MATLAB script to plot the received time-domain sequence, the CFR, the estimated CFO and the detected data symbols

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
 
#### Time-Delay $\tau$ in $[seconds]$
For $R_S=1Hz$, a time-delay of $\Tau$ in $[samples]$ results in a delay $\tau$ in $[s]$ as
```math
\tau=\Tau*\frac{1}{R_s} \qquad\to\quad \tau\quad[seconds]=\Tau\quad[samples]
```

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

#### Expected Subcarrier Phase Calculation
At two representative subcarriers at $k = \pm 15$:  
```math
\Delta\phi_{-15} = 2\pi \cdot \frac{-15}{64}\cdot*(-0.5) = 0.7363\,\mathrm{rad}
```
 
```math
\Delta\phi_{+15} = 2\pi \cdot \frac{15}{64}\cdot*(-0.5) = -0.7363\,\mathrm{rad}
```

The CFR for the shown simulation parameters is expected to show an equal subcarrier gain of $|H_k|=0.0286$ for all $M_{data}+M_{pilot}$ subcarriers in the transmission bandwidth and a linear phase shift that intersects $\Delta\phi_{-15}$ and $\Delta\phi_{15}$. 
 

### Generated baseband sequence with the specified channel impairments
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_singlechannel/sim_singlechannel_signal.svg" alt="sim_singlechannel_signal.svg" style="width:90%;">

### Estimated CFR 
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_singlechannel/sim_singlechannel_cfr.svg" alt="sim_singlechannel_cfr.svg" style="width:75%;">
<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_singlechannel/sim_singlechannel_cfr_complex.svg" alt="sim_singlechannel_cfr_complex.svg" style="width:75%;">

### Detected Datasymbols 
(only first OFDM symbol)

<img src="https://raw.githubusercontent.com/F-L-X-S/doa4rfc/refs/heads/main/docs/assets/sim_singlechannel/sim_singlechannel_detection.svg" alt="sim_singlechannel_detection.svg" style="width75%;">