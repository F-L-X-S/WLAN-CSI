/**
 * @file example_ofdmframesync.cc
 * @brief This example demonstrates the usage of the OFDM frame synchronizer for a phase-shifted Frame.
 * 
 */

 #include <iostream>
 #include <cmath>
 #include <complex>
 #include <cassert>
 #include <liquid/liquid.h>
 #include <signal_generator/signal_generator.h>
 #include <matlab_export/matlab_export.h>

// Definition of the transmission-settings 
#define NUM_SAMPLES 1200            // Total Number of samples to be generated 
#define SYMBOLS_PER_FRAME 3         // Number of data-ofdm-symbols transmitted per frame
#define FRAME_START 30              // Start position of the ofdm-frame in the sequence
 

// Definition of the channel impairments
#define SNR_DB 37.0f                // Signal-to-noise ratio (dB)
#define NOISE_FLOOR -92.0f          // Noise floor (dB)
#define CFO 0.0f                    // Carrier frequency offset (radians per sample)
#define PHASE_OFFSET 0.4189f        // Phase offset (radians) (e.g. for 20MHz (wavelength 15m) -> phaseshift of (1/15)*2PI per meter => 0.4189rad/m)
 
// Output file in MATLAB-format to store results
#define OUTFILE "./matlab/example_ofdmframesync_out.m" 


// custom data type to pass to callback function
struct callback_data {
    std::vector<std::complex<float>> buffer;        // Buffer to store detected symbols 
    std::vector<std::complex<float>> phi_results;   // phase offset estimate results
};

// callback function
//  _X          : array of received subcarrier samples [size: _M x 1]
//  _p          : subcarrier allocation array [size: _M x 1]
//  _M          : number of subcarriers
//  _userdata   : user-defined data pointer
static int callback(std::complex<float>* _X, unsigned char * _p, unsigned int _M, void * _cb_data){
    // Add symbols from all subcarriers to buffer 
    for (unsigned int i = 0; i < _M; ++i) {
        // ignore 'null' and 'pilot' subcarriers
        if (_p[i] != OFDMFRAME_SCTYPE_DATA)
            continue;
        static_cast<callback_data*>(_cb_data)->buffer.push_back(_X[i]);  
    }
// Reset after returning the first symbol 
return 1;
}

// main function
int main(int argc, char*argv[])
{
    // set the random seed differently for each run
    srand(time(NULL));

    // ---------------------- Signal Generation ----------------------
    // options
    unsigned int M           = 64;      // number of subcarriers 
    unsigned int cp_len      = 16;      // cyclic prefix length (800ns for 20MHz => 16 Sample)
    unsigned int taper_len   = 4;       // window taper length 

    // derived values
    unsigned int frame_len   = M + cp_len;
    unsigned int frame_samples = (3+SYMBOLS_PER_FRAME)*frame_len; // S0a + S0b + S1 + data symbols

    // Check if the number of samples is sufficient to contain the frame
    assert(NUM_SAMPLES > frame_samples+FRAME_START); 

    // initialize subcarrier allocation
    unsigned char p[M];                       // subcarrier allocation array
    ofdmframe_init_default_sctype(M, p);

    // create subcarrier notch in upper half of band
    unsigned int n0 = (unsigned int) (0.13 * M);    // lower edge of notch
    unsigned int n1 = (unsigned int) (0.21 * M);    // upper edge of notch
    for (size_t i=n0; i<n1; i++)
        p[i] = OFDMFRAME_SCTYPE_NULL;

    // create frame generator
    ofdmframegen fg = ofdmframegen_create(M, cp_len, taper_len, p);

    std::complex<float> y[frame_samples];     // output time series
    std::complex<float> X[M];                 // channelized symbols
    unsigned int n=0;                         // Sample number in time domains

    // write first S0 symbol
    ofdmframegen_write_S0a(fg, &y[n]);
    n += frame_len;

    // write second S0 symbol
    ofdmframegen_write_S0b(fg, &y[n]);
    n += frame_len;

    // write S1 symbol
    ofdmframegen_write_S1( fg, &y[n]);
    n += frame_len;

    // modulate data subcarriers
    for (size_t i=0; i<SYMBOLS_PER_FRAME; i++) {
        // load different subcarriers with different data
        unsigned int j;
        for (j=0; j<M; j++) {
            // ignore 'null' and 'pilot' subcarriers
            if (p[j] != OFDMFRAME_SCTYPE_DATA)
                continue;
            // Radnom QPSK Symbols
            X[j] = std::complex<float>((rand() % 2 ? -0.707f : 0.707f), (rand() % 2 ? -0.707f : 0.707f));
        }

        // generate OFDM symbol in the time domain
        ofdmframegen_writesymbol(fg, X, &y[n]);
        n += frame_len;
    }

    // ------------------- Channel impairments ----------------------
    // create channel and add impairments
    channel_cccf channel = channel_cccf_create();
    channel_cccf_add_awgn(channel, NOISE_FLOOR, SNR_DB);            // Add Noise 
    channel_cccf_add_carrier_offset(channel, CFO, PHASE_OFFSET);    // Add Carrier Frequency Offset and Phase Offset

    // Insert the interpolated training field into the longer sequence at the specified start position 'TF_SYMBOL_START' 
    std::complex<float> tx[NUM_SAMPLES];                    // Buffer to store the transmitted signal (before channel impariments)     
    InsertSequence(tx, y, FRAME_START, n);

    // apply channel to the generated signal
    std::complex<float> rx[NUM_SAMPLES];                    // Buffer to store the received signal (after channel impairiments)                 
    channel_cccf_execute_block(channel, tx, NUM_SAMPLES, rx);

    // ----------------- Synchronization ----------------------
    struct callback_data cb_data; // callback data

    // create frame synchronizer
    ofdmframesync fs = ofdmframesync_create(M, cp_len, taper_len, p, callback, (void*)&cb_data);

    // Samplewise synchronization
    for (unsigned int i = 0; i < NUM_SAMPLES; ++i) {
        ofdmframesync_execute(fs,&rx[i], 1);
    }
    
    // destroy objects
    ofdmframegen_destroy(fg);
    ofdmframesync_destroy(fs);

    // ----------------- MATLAB-compatible output ----------------------
    MatlabExport(std::vector<std::complex<float>>(rx, rx + NUM_SAMPLES), "x", OUTFILE);
    MatlabExport(cb_data.buffer, "buffer", OUTFILE);
    MatlabExport(cb_data.phi_results, "phi", OUTFILE);
    
    return 0;
}