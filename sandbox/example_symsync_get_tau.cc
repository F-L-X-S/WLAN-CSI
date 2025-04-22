#include <iostream>
#include <complex>
#include <vector>
#include <cmath>
#include <liquid/liquid.h>

int main() {
    // Symbol-synchronization Parameters
    // Filter-length = k*2m+1 (number of taps)
    unsigned int k     = 16;     // samples/symbol (oversampling) (used as resampling rate)
    unsigned int m     = 1;     // filter semi-length (in symbols)
    float        beta  = 0.5f;  // excess bandwidth for RRC filter
    unsigned int npfb  = 32;    // number of filters in polyphase bank (timing resolution)
    liquid_firfilt_type ftype = LIQUID_FIRFILT_ARKAISER;

    // ---------------------------------------------- Upsample example symbols
    unsigned int num_symbols = 100; // number of symbols to process
    unsigned int num_samples = k*num_symbols;
    float dt = -0.3f;           // fractional sample offset

    // generate random QPSK symbols
    std::vector<std::complex<float>> s(num_symbols);
    for(unsigned int i=0;i<num_symbols;i++) {
        float re = (rand()%2 ? +M_SQRT1_2 : -M_SQRT1_2);
        float im = (rand()%2 ? +M_SQRT1_2 : -M_SQRT1_2);
        s[i] = { re, im };
    }

    // design interpolating filter with 'dt' samples of delay
    firinterp_crcf interp = firinterp_crcf_create_prototype(ftype,k,m,beta,dt);

    // run interpolator to generate transmitted samples
    std::vector<std::complex<float>> tx_samples(num_samples);
    firinterp_crcf_execute_block(interp, s.data(), num_symbols, tx_samples.data());

    // destroy interpolator
    firinterp_crcf_destroy(interp);

    //-------------------------------------------------
    // create symbol synchronizer
    symsync_crcf sync = symsync_crcf_create_rnyquist(ftype, k, m, beta, npfb);
    
    // set bandwidth
    float bandwidth = 0.02f;    // loop filter bandwidth
    symsync_crcf_set_lf_bw(sync, bandwidth);

    // execute on entire block of samples
    std::vector<std::complex<float>> y(num_symbols+64);
    unsigned int ny=0;
    symsync_crcf_execute(sync, tx_samples.data(), num_samples, y.data(), &ny);

    // Print estimated offset (tau)
    float tau_est = symsync_crcf_get_tau(sync);
    printf("Initial offset:         %.4f\n", dt);
    printf("Estimated timing tau:   %.4f\n", tau_est);
    printf("Symbols recovered:      %u\n", ny);

    // Print recovered symbol(s)
    for (unsigned int i = 0; i < ny; i++) {
        std::cout << "y[" << i << "] = " << y[i] << std::endl;
    }

    // destroy synchronizer
    symsync_crcf_destroy(sync);

    return 0;
}