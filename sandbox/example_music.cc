/**
 * @file example_music.cc
 * @author Felix Schuelke (flxscode@gmail.com)
 * 
 * @brief This example demonstrates the usage of multiple OFDM frame synchronizers within multisync 
 * for receiving multiple channels with different channel impairments like in an antenna array.
 * The CFR or CIR is exported via ZeroMq to be processed within the MUSIC algorithm in Python for estimating the DOA.
 * The usage of Liquid's DSP-modules is based on https://github.com/jgaeddert/liquid-dsp (Copyright (c) 2007 - 2016 Joseph Gaeddert).
 * 
 * @version 0.1
 * @date 2025-05-20
 * 
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
 #include <zmq_socket/zmq_socket.h>

// Definition of the transmission-settings 
#define NUM_SAMPLES 150000          // Total Number of samples to be generated 
#define SYMBOLS_PER_FRAME 3         // Number of data-ofdm-symbols transmitted per frame
#define FRAME_START 30              // Start position of the ofdm-frame in the sequence
#define NUM_CHANNELS 10             // Number of channels to be synchronized
#define CARRIER_FREQUENCY 9600.0f   // Frequency, the Signal is modulated to (2.4GHz/20MHz = 120, M+CP Samples per Symbol (64+16)* 120 -> 9600 Samples per Symbol in 2.4GHz domain)

// Definition of the channel impairments
#define SNR_DB 37.0f                // Signal-to-noise ratio [dB]
#define NOISE_FLOOR -92.0f          // Noise floor [dB]
#define CFO 0.00f                   // Carrier frequency offset [radians per sample]
#define PHASE_OFFSET 0.0            // Phase offset [radians]
#define DELAY 10.0f                 // Delay for the first channel [samples] 
#define DDELAY 0.4066f              // Differential Delay between receiving channels [samples] (sin(15°)*0.5*pi= 0.4066 Samples)

// Interface for zmq socket
#define EXPORT_INTERFACE 'tcp://localhost:5555' 

// Output file in MATLAB-format to store results
#define OUTFILE "./matlab/example_music.m" 


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

    std::complex<float> tx_base[frame_samples];     // complex baseband signal buffer
    std::complex<float> X[M];                 // channelized symbols
    unsigned int n=0;                         // Sample number in time domains

    // write first S0 symbol
    ofdmframegen_write_S0a(fg, &tx_base[n]);
    n += frame_len;

    // write second S0 symbol
    ofdmframegen_write_S0b(fg, &tx_base[n]);
    n += frame_len;

    // write S1 symbol
    ofdmframegen_write_S1( fg, &tx_base[n]);
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
        ofdmframegen_writesymbol(fg, X, &tx_base[n]);
        n += frame_len;
    }

    // ------------------- Upconversion ---------------------
    // Resample signal to match carrier
    unsigned int tx_len = (unsigned int)(frame_samples*ceil(CARRIER_FREQUENCY/M));
    std::complex<float> tx[tx_len];
    msresamp_crcf resamp_tx = msresamp_crcf_create(CARRIER_FREQUENCY/M,60.0);
    unsigned int ny;
    msresamp_crcf_execute(resamp_tx, tx_base, frame_samples, tx, &ny);
    msresamp_crcf_destroy(resamp_tx);

    // Mix Up 
    nco_crcf nco_tx = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_frequency(nco_tx, 2*M_PI*CARRIER_FREQUENCY);
    nco_crcf_mix_block_up(nco_tx, tx, tx, tx_len);
    nco_crcf_destroy(nco_tx);                               

    // ------------------- Channel impairments ---------------------
    // Create base channel object
    channel_cccf base_channel = channel_cccf_create();
    channel_cccf_add_carrier_offset(base_channel, CFO, PHASE_OFFSET);    // Add Carrier Frequency Offset and Phase Offset

    // Delay filter parameters
    unsigned int nmax       =   200;            // maximum delay
    unsigned int m          =   cp_len;         // filter semi-length
    unsigned int npfb       =   1000;           // fractional delay resolution

    // Apply channel to the generated signal
    std::vector<std::vector<std::complex<float>>> rx_base(NUM_CHANNELS);                 // Buffer to store the received signal for all channels
    unsigned int rx_base_len = ceil(NUM_SAMPLES*((float)M/CARRIER_FREQUENCY)); 

    for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
        std::complex<float> rx_channel[NUM_SAMPLES];                                // Buffer to store the received signal a single channel

        // Insert the transmitted sequence into the longer rx_channel at the specified start position 'TF_SYMBOL_START'   
        InsertSequence(rx_channel, tx, FRAME_START, tx_len);

        // Add Time delay 
        fdelay_crcf fd = fdelay_crcf_create(nmax, m, npfb);
        float delay = DELAY+((float)i*DDELAY);
        fdelay_crcf_set_delay(fd, delay);                                           // Set the delay for respective channel
        fdelay_crcf_execute_block(fd, tx, NUM_SAMPLES, rx_channel);                 // Delay the signal    
        fdelay_crcf_destroy(fd);

        // Add channel impairments
        channel_cccf channel = channel_cccf_copy(base_channel);                     // Copy the base channel
        channel_cccf_add_awgn(channel, NOISE_FLOOR, SNR_DB);                        // Set Noise for each channel
        channel_cccf_execute_block(channel, rx_channel, NUM_SAMPLES, rx_channel);   // Apply channel impairments to the signal          
        channel_cccf_destroy(channel);

        // ------------------- Downconversion ---------------------
        // Mix down respective channel to complex baseband
        nco_crcf nco_rx = nco_crcf_create(LIQUID_NCO);
        nco_crcf_set_frequency(nco_rx, 2*M_PI*CARRIER_FREQUENCY);
        nco_crcf_mix_block_down(nco_rx, rx_channel, rx_channel, NUM_SAMPLES);
        nco_crcf_destroy(nco_rx);       
        
        // Resample the received signal 
        std::complex<float> rx_channel_base[rx_base_len]; // Buffer to store the received signal a single channel
        msresamp_crcf resamp_rx = msresamp_crcf_create(M/CARRIER_FREQUENCY,60.0);
        msresamp_crcf_execute(resamp_rx, rx_channel, NUM_SAMPLES, rx_channel_base, &ny);
        msresamp_crcf_destroy(resamp_rx);

        // Copy the received signal to the buffer for the respective channel
        rx_base[i].assign(rx_channel_base, rx_channel_base + rx_base_len);         
    }

    channel_cccf_destroy(base_channel);


    // ----------------- Synchronization ----------------------
    // callback data
    struct callback_data cb_data[NUM_CHANNELS]; 
    void* userdata[NUM_CHANNELS];

    // Array of Pointers to CB-Data 
    for (unsigned int i = 0; i < NUM_CHANNELS; ++i)
        userdata[i] = &cb_data[i];

    // Create multi frame synchronizer
    MultiSync<ofdmframesync> ms(NUM_CHANNELS, {M, cp_len, taper_len, p}, callback, userdata);

    // initialize zmq socket for data export 
    ZmqSender sender("tcp://*:5555");

    // Channel frequency response (CFR) and channel impulse response (CIR)
    std::vector<std::vector<std::complex<float>>> cfr(NUM_CHANNELS);   // Multidimensional buffer to store the cfr for all channels
    std::vector<std::vector<std::complex<float>>> cir(NUM_CHANNELS);   // Multidimensional buffer to store the cir for all channels
    cfr.assign(NUM_CHANNELS, std::vector<std::complex<float>>(M, std::complex<float>(0.0f, 0.0f)));   // Initialize the buffer with zeros
    cir.assign(NUM_CHANNELS, std::vector<std::complex<float>>(M, std::complex<float>(0.0f, 0.0f)));   // Initialize the buffer with zeros

    // Samplewise synchronization of each channel
    std::vector<std::complex<float>> rx_sample(1);  // create vector of size 1 containing the current sample
    for (unsigned int i = 0; i < rx_base_len; ++i) {
        for (unsigned int j = 0; j < NUM_CHANNELS; ++j){
            // execute the respective synchronizer
            rx_sample[0]= rx_base[j][i];             
            ms.Execute(j, &rx_sample);

            // Store the CFR (only once)
            if (cb_data[j].buffer.size() && !cb_data[j].cfr.size()){
                ms.GetCfr(j, &cb_data[j].cfr, M);                               // Write cfr to callback data
                cfr[j].assign(cb_data[j].cfr.begin(), cb_data[j].cfr.end());    // Copy the CFR to the multidimensional buffer   
            };
        };

        // Synchronize NCOs of all channels to the average NCO frequency and phase
        ms.SynchronizeNcos();
    };

    // compute the CIR from the CFR via IFFT
    std::vector<std::complex<float>> cir_temp(M);
    for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
        // cfr_temp = cfr[ch]; // copy the cfr to a temporary buffer
        // fftplan q = fft_create_plan(M, cfr_temp.data(), cir_temp.data(), LIQUID_FFT_BACKWARD, 0);
        // // compute the CIR from the CFR via IFFT
        // fft_execute(q); // IFFT
        // for (unsigned int k=0; k<M; k++)
        //     cir_temp[i] /= (float) M;
        // cir[ch] = cir_temp;
        // fft_destroy_plan(q);

        // Manual comupation of the IFFT 
        cir_temp.assign(M, std::complex<float>(0.0f, 0.0f)); // Initialize the cir_temp with zeros
        for (unsigned int k = 0; k < M; ++k) {
            for (unsigned int i = 0; i < M; ++i) {
                    int n = (i > M/2) ? (float)i - (float)(M) : (float)i; // CFR is ordered by  1,..., (M/2-1),-(M/2),...,-1,0
                    if (p[n] == OFDMFRAME_SCTYPE_NULL) // ignore null subcarriers
                        continue;
                    cir_temp[k] += cfr[ch][(i)]*std::exp(std::complex<float>(0.0f, 2.0f * M_PI * float(n * k) / float(M)));
            };
            cir_temp[k] /= (float)M; 
        };
        cir[ch] = cir_temp;
    }

    // send the CIR to zmq socket  
    sender.send(cir);        

    // destroy objects
    ofdmframegen_destroy(fg);    

// ----------------- MATLAB output ----------------------
MatlabExport m_file(OUTFILE);

// Export CFRs to MATLAB file
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    m_file.Add(cfr[ch], "cfr_" + ch_suffix);
    m_file.Add(cir[ch], "cir_" + ch_suffix);
    m_file.Add(rx_base[ch], "x_" + ch_suffix);
}

// Add combined plot-commands to the MATLAB file
std::stringstream matlab_cmd;
// Signals 
matlab_cmd << "figure; hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(real(x_" << ch_suffix
               << "), 'DisplayName', 'Re (Ch " << ch_suffix << ")');";
}
matlab_cmd << "title('Time Domain'); xlabel('Sample Index'); ylabel('Amplitude');";
matlab_cmd << "legend show; grid on;";
matlab_cmd << std::endl;



matlab_cmd << "figure;";
// CFR Magnitude
matlab_cmd << "subplot(2,1,1); hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(abs(cfr_" << ch_suffix << "), 'DisplayName', 'RX-Channel " << ch_suffix << "');";
}
matlab_cmd << "title('Channel Frequency Response Gain'); legend; grid on;";
matlab_cmd << std::endl;

// CFR Phase
matlab_cmd << "subplot(2,1,2); hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(angle(cfr_" << ch_suffix << "), 'DisplayName', 'RX-Channel " << ch_suffix << "');";
}
matlab_cmd << "title('Channel Frequency Response Phase'); legend; grid on;";
matlab_cmd << std::endl;

// CIR Magnitude
matlab_cmd << "figure;";
matlab_cmd << "subplot(2,1,1); hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(abs(cir_" << ch_suffix << "), 'DisplayName', 'RX-Channel " << ch_suffix << "');";
}
matlab_cmd << "title('Channel Impulse Response Gain'); legend; grid on;";
matlab_cmd << std::endl;

// CIR Phase
matlab_cmd << "subplot(2,1,2); hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(angle(cir_" << ch_suffix << "), 'DisplayName', 'RX-Channel " << ch_suffix << "');";
}
matlab_cmd << "title('Channel Impulse Response Phase'); legend; grid on;";
matlab_cmd << std::endl;


// CFR in Complex 
matlab_cmd << "figure;";
matlab_cmd << "subplot(1,2,1); hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(real(cfr_" << ch_suffix << "), imag(cfr_" << ch_suffix
               << "), '.', 'DisplayName', 'RX-Channel " << ch_suffix << "');";
}
matlab_cmd << "title('CFR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
matlab_cmd << std::endl;

// CIR in Complex 
matlab_cmd << "subplot(1,2,2); hold on;";
for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
    std::string ch_suffix = std::to_string(ch);
    matlab_cmd << "plot(real(cir_" << ch_suffix << "), imag(cir_" << ch_suffix
               << "), '.', 'DisplayName', 'RX-Channel " << ch_suffix << "');";
}
matlab_cmd << "title('CIR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
matlab_cmd << std::endl;
// Add the complete command string to MATLAB export
m_file.Add(matlab_cmd.str());

    return 0;
}