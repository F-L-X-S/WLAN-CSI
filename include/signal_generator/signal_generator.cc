#include "signal_generator.h"

#include <cmath>
#include <cstdlib>
#include <cassert>
#include <liquid/liquid.h>

/**
 * @brief Generate a sequence of samples, containing a pattern of the length `sequence_len`, 
 *        that is repeated 'seq_repition' times and surrounded by zeros. The pattern starts at 'seq_start'. The sequence is padded with noise. 
 * @param sequence_len Length of the sequence to be generated.
 * @param seq_repition Number of repetitions of the sequence.
 * @param seq_start Starting index for the sequence in the output array.
 * @param x Pointer to the output array where the generated sequence will be stored.
 * @param num_samples Total number of samples in the output array.
 */
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