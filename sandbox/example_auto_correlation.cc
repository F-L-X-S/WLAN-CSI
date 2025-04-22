#include <iostream>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <complex.h>
#include <liquid/liquid.h>
#include <correlation/auto_corr.h>

// Generate a sequence of samples, containing a pattern of the length `sequence_len`, 
// that is repeated 'seq_repition' times and followed by zeros. The sequence is padded with noise. 
// https://github.com/jgaeddert/liquid-dsp/blob/master/examples/autocorr_cccf_example.c
void GenerateSequence(unsigned int sequence_len, unsigned int seq_repition, std::complex<float>* x, unsigned int num_samples) {
    assert(num_samples >= sequence_len * seq_repition);
    float SNRdB=20.0f;                              // signal-to-noise ratio (dB)
    std::complex<float> sequence[sequence_len];    // short sequence

    // generate random training sequence using QPSK symbols
    modemcf mod = modemcf_create(LIQUID_MODEM_QPSK);
    for (unsigned int i=0; i<sequence_len; i++)
        modemcf_modulate(mod, rand()%4, &sequence[i]);
    modemcf_destroy(mod);

    // write training sequence 'seq_repition' times, followed by zeros
    unsigned int t=0;
    for (unsigned int i=0; i<seq_repition; i++) {
        // copy sequence
        memmove(&x[t], sequence, sequence_len*sizeof(float));
        t += sequence_len;
    }

    // pad end with zeros
    for (unsigned int i=t; i<num_samples; i++)
        x[i] = 0.0f;

    // add noise
    float nstd = powf(10.0f, -SNRdB/20.0f);
    for (unsigned int i=0; i<num_samples; i++)
        cawgn(&x[i],nstd);
};

int main() {
    // Create sequence with 16 samples, repeated 10 times, and padded with noise
    unsigned int num_samples = 200;
    std::complex<float> x[num_samples]; // Array to hold the generated samples
    GenerateSequence(16, 10, x, num_samples);
    std::cout << "Generated samples:" << std::endl;
    for (unsigned int i = 0; i < num_samples; ++i) {
        std::cout << x[i] << std::endl;
    }

    // Create an instance of AutoCorr with a delay of 16 samples
    AutoCorr auto_corr(0.9, 16);
    // Process the generated samples
    for (unsigned int i = 0; i < num_samples; ++i) {
        auto_corr.Push(x[i]);
        std::complex<float> rxx = auto_corr.GetRxx();
        std::cout << "Auto-correlation value at sample " << i << ": " << std::abs(rxx) << std::endl;
    }
    return 0;
}