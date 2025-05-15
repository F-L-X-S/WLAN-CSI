/**
 * @file example_ofdm_multisync.cc
 * @brief This example demonstrates the usage of multiple OFDM frame synchronizers ithin multisync 
 * for receiving multiple channels with different channel impairments like in an antenna array.
 * 
 */

 #include <iostream>
 #include <cmath>
 #include <complex>
 #include <cassert>
 #include <liquid.h>
 #include <signal_generator/signal_generator.h>
 #include <matlab_export/matlab_export.h>
 #include <multisync/multisync.h>

// Definition of the transmission-settings 
#define NUM_SAMPLES 1200            // Total Number of samples to be generated 
#define SYMBOLS_PER_FRAME 3         // Number of data-ofdm-symbols transmitted per frame
#define FRAME_START 30              // Start position of the ofdm-frame in the sequence
#define NUM_CHANNELS 4              // Number of channels to be synchronized
 

// Definition of the channel impairments
#define SNR_DB 37.0f                // Signal-to-noise ratio (dB)
#define NOISE_FLOOR -92.0f          // Noise floor (dB)
#define CFO 0.01f                   // Carrier frequency offset (radians per sample)
#define PHASE_OFFSET 0.4            // Phase offset (radians) 
#define DELAY 0.3f                  // Delay for the first channel (samples)
#define DDELAY 0.03f                // Differential Delay between receiving channels (samples)

// Output file in MATLAB-format to store results
#define OUTFILE "./matlab/example_ofdm_multisync.m" 


// custom data type to pass to callback function
struct callback_data {
    std::vector<std::complex<float>> buffer;        // Buffer to store detected symbols 
    std::vector<float> cfo;                         // carrier frequency offsets estimated per sample 
    std::vector<std::complex<float>> cfr;           // channel frequency response 
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
return 0;
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
    // create base channel object
    channel_cccf base_channel = channel_cccf_create();
    channel_cccf_add_carrier_offset(base_channel, CFO, PHASE_OFFSET);    // Add Carrier Frequency Offset and Phase Offset

    // create delay object 
    unsigned int nmax       = 200;  // maximum delay
    unsigned int m          =  12;  // filter semi-length
    unsigned int npfb       =  10;  // fractional delay resolution
    fdelay_crcf fd = fdelay_crcf_create(nmax, m, npfb);

    // Insert the interpolated training field into the longer sequence at the specified start position 'TF_SYMBOL_START' 
    std::complex<float> tx[NUM_SAMPLES];                    // Buffer to store the transmitted signal (before channel impariments)     
    InsertSequence(tx, y, FRAME_START, n);

    // apply channel to the generated signal
    std::vector<std::vector<std::complex<float>>> rx(NUM_CHANNELS);                 // Buffer to store the received signal for all channels
    for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
        std::complex<float> rx_channel[NUM_SAMPLES];                                // Buffer to store the received signal a single channel

        // Add Time delay 
        fdelay_crcf_set_delay(fd, DELAY+i*DDELAY);                                  // Set the delay for respective channel
        fdelay_crcf_execute_block(fd, tx, NUM_SAMPLES, rx_channel);                 // Delay the signal    

        // Add channel impairments
        channel_cccf channel = channel_cccf_copy(base_channel);                     // Copy the base channel
        channel_cccf_add_awgn(channel, NOISE_FLOOR, SNR_DB);                        // Set Noise for each channel
        channel_cccf_execute_block(channel, rx_channel, NUM_SAMPLES, rx_channel);   // Apply channel impairments to the signal          
        
        // Copy the received signal to the buffer for the respective channel
        rx[i].assign(rx_channel, rx_channel + NUM_SAMPLES);                         
    }
    
    // ----------------- Synchronization ----------------------
    // callback data
    struct callback_data cb_data[NUM_CHANNELS]; 
    void* userdata[NUM_CHANNELS];

    // Array of Pointers to CB-Data 
    for (unsigned int i = 0; i < NUM_CHANNELS; ++i)
        userdata[i] = &cb_data[i];

    // Create multi frame synchronizer
    MultiSync<ofdmframesync> ms(NUM_CHANNELS, {M, cp_len, taper_len, p}, callback, userdata);

    // Samplewise synchronization of each channel
    std::vector<std::complex<float>> rx_sample(1);  // create vector of size 1 containing the current sample
    for (unsigned int i = 0; i < NUM_SAMPLES; ++i) {
        for (unsigned int j = 0; j < NUM_CHANNELS; ++j){
            // get the current sample of the channel
            rx_sample[0]= rx[j][i];             

            // execute the respective synchronizer
            ms.Execute(j, &rx_sample);

            // Store the CFR of all channels (only once)
            if (cb_data[j].buffer.size() && !cb_data[j].cfr.size()){
                ms.GetCfr(j, &cb_data[j].cfr, M); 
            };
        };
        
        // Synchronize NCOs of all channels to the average NCO frequency and phase
        ms.SynchronizeNcos();
    };
    
    // destroy objects
    ofdmframegen_destroy(fg);
    fdelay_crcf_destroy(fd);

    // ----------------- MATLAB output ----------------------
    MatlabExport m_file(OUTFILE);
    
    // Export variables to MATLAB file
    for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
        std::string ch_suffix = std::to_string(ch);

        m_file.Add(rx[ch], "x_" + ch_suffix)
            .Add(cb_data[ch].buffer, "buffer_" + ch_suffix)
            .Add(cb_data[ch].cfr, "cfr_" + ch_suffix);
    }

    // Add plot-commands to the MATLAB file
    for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
        std::string ch_suffix = std::to_string(ch);

        m_file.Add(
            "figure; "
            "subplot(4,1,1); plot(real(x_" + ch_suffix + ")); hold on; plot(imag(x_" + ch_suffix + ")); "
            "title('Input-Signal Kanal " + ch_suffix + "'); legend('Real', 'Imag'); grid on;"

            "subplot(4,1,3); plot(abs(cfr_" + ch_suffix + ")); title('Channel Frequency Response Gain – Kanal " + ch_suffix + "'); grid on;"

            "subplot(4,1,4); plot(angle(cfr_" + ch_suffix + ")); title('Channel Frequency Response Phase – Kanal " + ch_suffix + "'); grid on;"
        );

        m_file.Add(
            "figure; "
            "plot(real(buffer_" + ch_suffix + "), imag(buffer_" + ch_suffix + "), '.', 'MarkerSize', 10); "
            "grid on; axis equal; xlabel('In-Phase'); ylabel('Quadrature'); "
            "title('Detected Symbols Kanal " + ch_suffix + "'); axis([-1 1 -1 1]);"
        );
    }

    return 0;
}