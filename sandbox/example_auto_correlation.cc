#include <iostream>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <complex.h>
#include <liquid/liquid.h>
#include <correlation/auto_corr.h>

// Generate a sequence of samples, containing a pattern of the length `sequence_len`, 
// that is repeated 'seq_repition' times and surrounded by zeros. The pattern starts at 'seq_start'. The sequence is padded with noise. 
// https://github.com/jgaeddert/liquid-dsp/blob/master/examples/autocorr_cccf_example.c
void GenerateSequence(unsigned int sequence_len, unsigned int seq_repition, unsigned int seq_start, std::complex<float>* x, unsigned int num_samples) {
    assert(num_samples >= sequence_len * seq_repition);
    float SNRdB=20.0f;                             // signal-to-noise ratio (dB)
    std::complex<float> sequence[sequence_len];    // short sequence

    // generate random training sequence using QPSK symbols
    modemcf mod = modemcf_create(LIQUID_MODEM_QPSK);
    for (unsigned int i=0; i<sequence_len; i++)
        modemcf_modulate(mod, rand()%4, &sequence[i]);
    modemcf_destroy(mod);

    // write training sequence 'seq_repition' times in the middle of the array
    unsigned int t=seq_start;
    for (unsigned int i=0; i<seq_repition; i++) {
        // copy sequence
        memmove(&x[t], sequence, sequence_len*sizeof(std::complex<float>));
        t += sequence_len;
    }

    // add noise
    float nstd = powf(10.0f, -SNRdB/20.0f);
    for (unsigned int i=0; i<num_samples; i++)
        cawgn(&x[i],nstd);
};

int main() {
    // Create sequence with 16 samples, repeated 10 times, starting after 30 Samples, padded with noise
    unsigned int num_samples = 200;
    std::complex<float> x[num_samples]; 
    GenerateSequence(16, 4, 30, x, num_samples);

    // MATLAB-compatible output in terminal
    std::cout << "x = [ ..." << std::endl;
    std::cout << std::fixed;  
    for (unsigned int i = 0; i < num_samples; ++i) {
        float re = x[i].real();
        float im = x[i].imag();
        std::cout << re << " + 1i*" << im;
        if (i < num_samples - 1)
            std::cout << ", ";
        if ((i + 1) % 5 == 0)
            std::cout << " ...\n";
    }
    std::cout << "];" << std::endl;

    // Create an instance of AutoCorr with a delay of 16 samples
    AutoCorr auto_corr(0.9f, 4);
    // Process the generated samples
    for (unsigned int i = 0; i < num_samples; ++i) {
        auto_corr.Push(x[i]);
        std::complex<float> rxx = auto_corr.GetRxx();
        std::cout << "Rxx(" << i << ")\t Abs:" << std::abs(rxx) <<" Arg:"<< std::arg(rxx) 
        << "\tPlateau: "<< (auto_corr.PlateauDetected() ? "True":"False") << std::endl;
    }
    return 0;
}