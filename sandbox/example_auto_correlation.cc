#include <iostream>
#include <cmath>
#include <complex>
#include <liquid/liquid.h>
#include <correlation/auto_corr.h>
#include <signal_generator/signal_generator.h>

int main() {
    // Create sequence with 16 samples, repeated 10 times, starting after 30 Samples, padded with noise
    unsigned int num_samples = 200;
    std::complex<float> x[num_samples]; 
    GenerateSequence(16, 4, 30, x, num_samples);

    // Create an instance of AutoCorr with a delay of 16 samples
    AutoCorr auto_corr(0.9f, 3);
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