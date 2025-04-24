// https://github.com/jgaeddert/liquid-dsp/blob/master/examples/autocorr_cccf_example.c

#include <complex>

/**
 * @brief Generate a sequence of samples, containing a pattern of the length `sequence_len`, 
 *        that is repeated 'seq_repition' times and surrounded by zeros. The pattern starts at 'seq_start'. The sequence is padded with noise. 
 * @param sequence_len Length of the sequence to be generated.
 * @param seq_repition Number of repetitions of the sequence.
 * @param seq_start Starting index for the sequence in the output array.
 * @param x Pointer to the output array where the generated sequence will be stored.
 * @param num_samples Total number of samples in the output array.
 */
void GenerateSequence(unsigned int sequence_len, 
                        unsigned int seq_repition, 
                        unsigned int seq_start, 
                        std::complex<float>* x, 
                        unsigned int num_samples);