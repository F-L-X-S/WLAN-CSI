#include <iostream>
#include <cmath>
#include <complex>
#include <liquid/liquid.h>
#include <correlation/auto_corr.h>
#include <signal_generator/signal_generator.h>

#define NUM_SAMPLES 200
#define SYMBOL_LENGTH 16
#define SYMBOL_REPEATS 4
#define SYMBOL_START 30

int main() {

    // Create sequence with 'NUM_SAMPLES' samples in total
    unsigned int num_samples = NUM_SAMPLES;
    std::complex<float> x[num_samples]; 

    // Generate the training field containing 'SYMBOL_REPEATS' symbols, each of length 'SYMBOL_LENGTH'
    unsigned int tf_len = SYMBOL_LENGTH * SYMBOL_REPEATS;
    std::complex<float> tf[tf_len]; 
    GenerateRepeatingSequence(SYMBOL_LENGTH, SYMBOL_REPEATS, tf, LIQUID_MODEM_QPSK);

    // Insert the training field into the longer sequence at the specified start position 'SYMBOL_START'
    InsertSequence(x, tf, SYMBOL_START, tf_len);

    // Add noise to the sequence
    float SNRdB = 20.0f; // signal-to-noise ratio (dB)
    AddNoise(x, num_samples, SNRdB);

    // Create an instance of AutoCorr, use a delay of SYMBOL_LENGTH to detect the plateau
    AutoCorr auto_corr(0.9f, SYMBOL_LENGTH);
    // Buffer for autocorrealtion results
    std::complex<float> rxx_results[num_samples]; 

    // Process the generated samples
    for (unsigned int i = 0; i < num_samples; ++i) {
        auto_corr.Push(x[i]);
        std::complex<float> rxx = auto_corr.GetRxx();

        // Print results
        std::cout << "Rxx(" << i << ")\t Abs:" << std::abs(rxx) <<" Arg:"<< std::arg(rxx) 
        << "\tPlateau: "<< (auto_corr.PlateauDetected() ? "True":"False") << std::endl;

        // Store the result
        rxx_results[i] = rxx;
    }

    // MATLAB-compatible output in terminal
    // Input-Signal x
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

    // Output the Rxx values
    std::cout << "Rxx = [ ..." << std::endl;
    std::cout << std::fixed;  
    for (unsigned int i = 0; i < num_samples; ++i) {
        float re = rxx_results[i].real();
        float im = rxx_results[i].imag();
        std::cout << re << " + 1i*" << im;
        if (i < num_samples - 1)
            std::cout << ", ";
        if ((i + 1) % 5 == 0)
            std::cout << " ...\n";
    }
    std::cout << "];" << std::endl;

    return 0;
}