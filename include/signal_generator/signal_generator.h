// https://github.com/jgaeddert/liquid-dsp/blob/master/examples/autocorr_cccf_example.c

#include <complex>
#include <liquid/liquid.h>

/**
 * @brief Generate a sequence of samples, containing a repeating pattern of the length `symbol_len`, 
 *        that is repeated 'symbol_repetitions' times. 
 * @param symbol_len Length (Number of samples) of each symbol.
 * @param seq_repition Number of repetitions of the symbols in the sequence.
 * @param x Pointer to the output array where the generated sequence will be stored.
 * @param mod_type Modulation type for the symbols.
 */
void GenerateRepeatingSequence(unsigned int symbol_len, unsigned int symbol_repetitions, std::complex<float>* x, modulation_scheme mod_type);

/**
 * @brief // Insert the short sequence into the long sequence at the specified start position
 * 
 * @param long_sequence Pointer to longer sequence 
 * @param short_sequence Pointer to shorter sequence 
 * @param seq_start Startng-position in the longer sequence where the shorter sequence will be inserted
 * @param seq_len Length of the shorter sequence
 */
void InsertSequence(std::complex<float>* long_sequence, std::complex<float>* short_sequence, unsigned int seq_start, unsigned int seq_len);

/**
 * @brief Add noise to a sequence of complex samples.
 * 
 * @param sequence Pointer to the sequence of complex samples.
 * @param sequence_len Length of the sequence.
 * @param SNRdB Signal-to-noise ratio in decibels.
 */
void AddNoise(std::complex<float>* sequence, unsigned int sequence_len, float SNRdB);