/**
 * @file example_qdsync.cc
 * @brief This example demonstrates the use of the Liquid qdsync algorithm for synchronization.
 * A trainingfield is generated, that contains a repeating pattern of complex numbers. 
 * The training field is then interpolated and inserted into a longer sequence where noise is added.
 * Regarding an ofdm-signal, the SAMPLES_PER_SYMBOL and the training-field are always referring to a single subcarrier.
 * 
 * The liquid qdsync algorithm uses a cross-correlation a specific pattern to detect the start of the frame.
 * We use the whole trainingfield containing repeating tf-symbols for the cross-correlation.
 * 
 */

#include <iostream>
#include <cmath>
#include <complex>
#include <cassert>
#include <liquid/liquid.h>
#include <correlation/auto_corr.h>
#include <signal_generator/signal_generator.h>
#include <matlab_export/matlab_export.h>

// Definition of the training-field for 20MHz (Pattern to be detected) (Note, that a TF-Symbol means a pattern of QPSK-Symbols)
#define TF_SYMBOL_LENGTH 1          // Number of Symbols a single training-field-symbol (pattern) contains (20MSPS)
#define TF_SYMBOL_REPEAT 10         // Number of times the training-field-symbol (pattern) is repeated
#define TF_SYMBOL_START 30          // Start position of the repeated training-field-symbols (pattern) in the sequence

// Definition of the receiver-settings 
#define SAMPLES_PER_SYMBOL 16       // samples/symbol (interpolation factor for the training-field, note that 'symbol' means a single qpsk symbol, not a tf-symbol mentioned above)
#define FILTER_DELAY 7              // filter delay [symbols] (number of symbols, the filter has to wait before taking the first median value)
#define NUM_SAMPLES 800             // Total Number of samples to be generated 

// Definition of the channel impairments
#define SNR_DB 37.0f                // Signal-to-noise ratio (dB)
#define NOISE_FLOOR -92.0f          // Noise floor (dB)
#define CFO 0.0f                    // Carrier frequency offset (radians per sample)
#define PHASE_OFFSET 1.2f           // Phase offset (radians)

// Output file in MATLAB-format to store results
#define OUTFILE "./matlab/example_qdsync_out.m" 


// synchronization callback (return 0:continue, 1:reset)
int callback(std::complex<float>* _buf, unsigned int _buf_len, void* buffer){
    for (unsigned int i = 0; i < _buf_len; ++i) {
        static_cast<std::vector<std::complex<float>>*>(buffer)->push_back(_buf[i]);  
    }
    return 1;
}

int main() {
    // ---------------------- Signal Generation ----------------------
    // Check if the number of samples is sufficient to contain the interpolated training field
    assert(NUM_SAMPLES > TF_SYMBOL_START + (TF_SYMBOL_LENGTH * TF_SYMBOL_REPEAT)*SAMPLES_PER_SYMBOL); 

    // Generate a pattern of sqrt(1/2)*(1+j) (example symbol transmitted in S-STF)
    std::complex<float> pattern[TF_SYMBOL_LENGTH];
    for (unsigned int i = 0; i < TF_SYMBOL_LENGTH; i++) {
        float re = 1 * M_SQRT1_2;
        float im = 1 * M_SQRT1_2;
        pattern[i] = std::complex<float>(re, im);
    }

    // Generate the training field containing 'TF_SYMBOL_REPEAT' symbols, each of length 'TF_SYMBOL_LENGTH'
    unsigned int tf_len = TF_SYMBOL_LENGTH * TF_SYMBOL_REPEAT;
    std::complex<float> tf[tf_len]; 
    GenerateRepeatingSequence(pattern, TF_SYMBOL_LENGTH, TF_SYMBOL_REPEAT, tf);

    // Interpolate the Training-field 
    firinterp_crcf interp = firinterp_crcf_create_prototype(LIQUID_FIRFILT_ARKAISER,SAMPLES_PER_SYMBOL,FILTER_DELAY,0.3f,0);
    unsigned int tf_i_len = tf_len*SAMPLES_PER_SYMBOL+2;                                  // Length of the interpolated training field
    std::complex<float> tf_i[tf_i_len];                                                 // Interpolated training field

    // Interpolate each tf-symbol by the factor 'SAMPLES_PER_SYMBOL' 
    for (unsigned int i = 0; i < tf_len; ++i) {
        firinterp_crcf_execute(interp, tf[i], &tf_i[i*SAMPLES_PER_SYMBOL]);
    }

    // ------------------- Channel impairments ----------------------
    // create channel and add impairments
    channel_cccf channel = channel_cccf_create();
    channel_cccf_add_awgn(channel, NOISE_FLOOR, SNR_DB);            // Add Noise 
    channel_cccf_add_carrier_offset(channel, CFO, PHASE_OFFSET);    // Add Carrier Frequency Offset and Phase Offset

    // Insert the interpolated training field into the longer sequence at the specified start position 'TF_SYMBOL_START' 
    std::complex<float> tx[NUM_SAMPLES];                    // Buffer to store the transmitted signal (before channel impariments)     
    InsertSequence(tx, tf_i, TF_SYMBOL_START, tf_i_len);

    // apply channel to the generated signal
    std::complex<float> rx[NUM_SAMPLES];                    // Buffer to store the received signal (after channel impairiments)                 
    channel_cccf_execute_block(channel, tx, NUM_SAMPLES, rx);

    // ----------------- Synchronization ----------------------
    std::vector<std::complex<float>> buffer;        // Buffer to store detected symbols 
    std::complex<float> rxy_results[NUM_SAMPLES];   // Crosscorrelation results
    float tau_results[NUM_SAMPLES];                 // fractional timing offset results
    float dphi_results[NUM_SAMPLES];                // frequency offset estimate results
    float phi_results[NUM_SAMPLES];                 // phase offset estimate results

    qdsync_cccf sync = qdsync_cccf_create_linear(tf, tf_len, LIQUID_FIRFILT_ARKAISER, SAMPLES_PER_SYMBOL, FILTER_DELAY, 0.3f, callback, &buffer);

    for (unsigned int i = 0; i < NUM_SAMPLES; ++i) {
        // Process synchronization by symbol 
        qdsync_cccf_execute(sync, &rx[i], SAMPLES_PER_SYMBOL);

        // Get the results
        rxy_results[i] = qdsync_cccf_get_rxy(sync);
        tau_results[i] = qdsync_cccf_get_tau(sync);
        dphi_results[i] = qdsync_cccf_get_dphi(sync);
        phi_results[i] = qdsync_cccf_get_phi(sync);
    }
    firinterp_crcf_destroy(interp);
    qdsync_cccf_destroy(sync);

    // ----------------- MATLAB-compatible output in terminal ----------------------
    MatlabExport(std::vector<std::complex<float>>(rx, rx + NUM_SAMPLES), "x", OUTFILE);
    MatlabExport(buffer, "buffer", OUTFILE);
    MatlabExport(std::vector<std::complex<float>>(rxy_results, rxy_results + NUM_SAMPLES), "rxy", OUTFILE);
    MatlabExport(std::vector<float>(tau_results, tau_results + NUM_SAMPLES), "tau", OUTFILE);
    MatlabExport(std::vector<float>(dphi_results, dphi_results + NUM_SAMPLES), "dphi", OUTFILE);
    MatlabExport(std::vector<float>(phi_results, phi_results + NUM_SAMPLES), "phi", OUTFILE);
    
    return 0;
}