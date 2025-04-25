#include <iostream>
#include <cmath>
#include <complex>
#include <liquid/liquid.h>
#include <correlation/auto_corr.h>
#include <signal_generator/signal_generator.h>
#include <matlab_export/matlab_export.h>

#define NUM_SAMPLES 400
#define STF_SYMBOL_LENGTH 16
#define STF_SYMBOL_REPEAT 4
#define STF_SYMBOL_START 30

#define OUTFILE "matlab/example_qdsync_out.m"

// synchronization callback (return 0:continue, 1:reset)
int callback(std::complex<float>* _buf, unsigned int _buf_len, void* _context){
    std::vector<float> buffer;
    for (unsigned int i = 0; i < _buf_len; ++i) {
        buffer.push_back(_buf[i].real());  
        buffer.push_back(_buf[i].imag()); 
    }
    MatlabExport(buffer, "buffer", OUTFILE);
}

int main() {
    // ---------------------- Signal Generation ----------------------
    // Create sequence with 'NUM_SAMPLES' samples in total
    unsigned int num_samples = NUM_SAMPLES;
    std::complex<float> x[num_samples]; 

    // Generate a random pattern of complex numbers
    std::complex<float> pattern[STF_SYMBOL_LENGTH];
    for (unsigned int i = 0; i < STF_SYMBOL_LENGTH; i++) {
        float re = (rand() % 2 ? 1.0f : -1.0f) * M_SQRT1_2;
        float im = (rand() % 2 ? 1.0f : -1.0f) * M_SQRT1_2;
        pattern[i] = std::complex<float>(re, im);
    }

    // Generate the training field containing 'STF_SYMBOL_REPEAT' symbols, each of length 'STF_SYMBOL_LENGTH'
    unsigned int tf_len = STF_SYMBOL_LENGTH * STF_SYMBOL_REPEAT;
    std::complex<float> tf[tf_len]; 
    GenerateRepeatingSequence(pattern, STF_SYMBOL_LENGTH, STF_SYMBOL_REPEAT, tf);

    // Interpolate the Training-field 
    unsigned int k            =    2;   // samples/symbol
    unsigned int m            =    7;   // filter delay [symbols]

    firinterp_crcf interp = firinterp_crcf_create_prototype(LIQUID_FIRFILT_ARKAISER,k,m,0.3f,0);
    std::complex<float> tf_i[tf_len];   // Interpolated training field
    for (unsigned int i = 0; i < tf_len; ++i) {
        firinterp_crcf_execute(interp, tf[i], &tf_i[i*k]);
    }

    // Insert the interpolated training field into the longer sequence at the specified start position 'STF_SYMBOL_START'
    InsertSequence(x, tf_i, STF_SYMBOL_START, k*tf_len);

    // Add noise to the sequence
    float SNRdB = 20.0f; // signal-to-noise ratio (dB)
    AddNoise(x, num_samples, SNRdB);

    // ----------------- Synchronization ----------------------
    std::complex<float> rxx_results[num_samples];   // Autocorrelation results
    float tau_results[num_samples];                 // fractional timing offset results
    float dphi_results[num_samples];                // frequency offset estimate results
    float phi_results[num_samples];                 // phase offset estimate results

    qdsync_cccf sync = qdsync_cccf_create_linear(pattern, STF_SYMBOL_LENGTH, LIQUID_FIRFILT_ARKAISER, k, m, 0.3f, callback, NULL);

    for (unsigned int i = 0; i < num_samples; ++i) {
        // Process synchronization for each single sample 
        qdsync_cccf_execute(sync, &x[i], STF_SYMBOL_LENGTH);

        // Get the results
        rxx_results[i] = qdsync_cccf_get_rxy(sync);
        tau_results[i] = qdsync_cccf_get_tau(sync);
        dphi_results[i] = qdsync_cccf_get_dphi(sync);
        phi_results[i] = qdsync_cccf_get_phi(sync);
    }
    qdsync_cccf_destroy(sync);

    // ----------------- MATLAB-compatible output in terminal ----------------------
    MatlabExport(std::vector<std::complex<float>>(x, x + num_samples), "x", OUTFILE);
    MatlabExport(std::vector<std::complex<float>>(rxx_results, rxx_results + num_samples), "rxx", OUTFILE);
    MatlabExport(std::vector<float>(tau_results, tau_results + num_samples), "tau", OUTFILE);
    MatlabExport(std::vector<float>(dphi_results, dphi_results + num_samples), "dphi", OUTFILE);
    MatlabExport(std::vector<float>(phi_results, phi_results + num_samples), "phi", OUTFILE);
    
    return 0;
}