/**
 * @file example_usrp.cc
 * @author Felix Schuelke 
 * @brief Example Project based on https://kb.ettus.com/Getting_Started_with_UHD_and_C%2B%2B
 * @version 0.1
 * @date 2025-05-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>

#include <matlab_export/matlab_export.h>
#include <multisync/multisync.h>

#define OUTFILE "./matlab/example_usrp.m"  // Output file in MATLAB-format to store results


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


int UHD_SAFE_MAIN(int argc, char *argv[]) {
    uhd::set_thread_priority_safe();

    // USRP Settings 
    std::string device_args("addr=192.168.10.3");
    std::string subdev("A:0");
    std::string ant("RX2");
    std::string ref("internal");

    double rate(25e6);
    double freq(2447e6);
    double gain(20);
    double bw(40e6);

    //create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % device_args << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(device_args);

    // Lock mboard clocks
    std::cout << boost::format("Lock mboard clocks: %f") % ref << std::endl;
    usrp->set_clock_source(ref);
    
    //always select the subdevice first, the channel mapping affects the other settings
    std::cout << boost::format("subdev set to: %f") % subdev << std::endl;
    usrp->set_rx_subdev_spec(subdev);
    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

    // set sample rate
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate / 1e6) << std::endl;
    usrp->set_rx_rate(rate);
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate() / 1e6) << std::endl << std::endl;

    // set freq
    std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
    uhd::tune_request_t tune_request(freq);
    usrp->set_rx_freq(tune_request);
    std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq() / 1e6) << std::endl << std::endl;

    // set the rf gain
    std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
    usrp->set_rx_gain(gain);
    std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;

    // set the IF filter bandwidth
    std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw / 1e6) << std::endl;
    usrp->set_rx_bandwidth(bw);
    std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth() / 1e6) << std::endl << std::endl;

    // set the antenna
    std::cout << boost::format("Setting RX Antenna: %s") % ant << std::endl;
    usrp->set_rx_antenna(ant);
    std::cout << boost::format("Actual RX Antenna: %s") % usrp->get_rx_antenna() << std::endl << std::endl;

    // callback data
    callback_data cb_data;
    void* userdata[] = { &cb_data };
        
    // Create multi frame synchronizer
    unsigned int M           = 64;      // number of subcarriers 
    unsigned int cp_len      = 16;      // cyclic prefix length (800ns for 20MHz => 16 Sample)
    unsigned int taper_len   = 4;       // window taper length 
    unsigned char p[M];                 // subcarrier allocation array
    ofdmframe_init_default_sctype(M, p);
    MultiSync<ofdmframesync> ms(1, {M, cp_len, taper_len, p}, callback, userdata);

    // CFR Buffer 
    std::vector<std::complex<float>> cfr(M, std::complex<float>(0.0f, 0.0f));

    // Receive stream
    uhd::stream_args_t stream_args("fc32", "sc16");                             // convert internal sc16 to complex float
    uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);        // cretae a receive stream 
    size_t max_samps = rx_stream->get_max_num_samps();                         
    
    // RX Stream buffer 
    std::vector<std::complex<float>> buff(max_samps);
    uhd::rx_metadata_t md;

    // Resampler 40MHz to 20MHz
    float resamp_factor = 20e6 / bw;
    unsigned int num_written = 0; 
    resamp_crcf resampler = resamp_crcf_create(resamp_factor, 12, 0.45f, 60.0f, 32);

    // start receiving samples
    usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    std::cout << "Receiving..." << std::endl;
    int receiving = 1;

    while (receiving) {
        size_t num_rx_samps = rx_stream->recv(&buff.front(), buff.size(), md);
        std::vector<std::complex<float>> rx_sample(1);  // create vector of size 1 containing the current sample
        for (unsigned int i = 0; i < num_rx_samps; ++i) {
                // Downsampling to 20MHz 
                resamp_crcf_execute(resampler, buff[i], &rx_sample[0], &num_written);
                // execute the respective synchronizer        
                ms.Execute(0, &rx_sample);

                // Store the CFR 
                if (cb_data.buffer.size() && !cb_data.cfr.size()){
                    ms.GetCfr(0, &cb_data.cfr, M);                              // Write cfr to callback data
                    cfr.assign(cb_data.cfr.begin(), cb_data.cfr.end());         // Copy the CFR to the buffer  
                    std::cout << "Captured CFR!" << std::endl;
                    receiving = 0; // Stop receiving after the first CFR is detected
                };
                // Synchronize NCOs of all channels to the average NCO frequency and phase
                ms.SynchronizeNcos();
        };
    };

    usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    std::cout << "Stopped receiving..." << std::endl;

// ----------------- MATLAB output ----------------------
MatlabExport m_file(OUTFILE);

// Export CFR to MATLAB file
std::string ch_suffix = std::to_string(1);
m_file.Add(cfr, "cfr_" + ch_suffix);

// Add combined plot-commands to the MATLAB file
std::stringstream matlab_cmd;

matlab_cmd << "figure;";
// CFR Magnitude
matlab_cmd << "subplot(2,1,1); hold on;";
matlab_cmd << "plot(abs(cfr_" << ch_suffix << "), 'DisplayName', 'RX-Channel " << ch_suffix << "');";
matlab_cmd << "title('Channel Frequency Response Gain'); legend; grid on;";
matlab_cmd << std::endl;

// CFR Phase
matlab_cmd << "subplot(2,1,2); hold on;";
matlab_cmd << "plot(angle(cfr_" << ch_suffix << "), 'DisplayName', 'RX-Channel " << ch_suffix << "');";
matlab_cmd << "title('Channel Frequency Response Phase'); legend; grid on;";
matlab_cmd << std::endl;

// CFR in Complex 
matlab_cmd << "figure;";
matlab_cmd << "subplot(1,2,1); hold on;";
matlab_cmd << "plot(real(cfr_" << ch_suffix << "), imag(cfr_" << ch_suffix
            << "), '.', 'DisplayName', 'RX-Channel " << ch_suffix << "');";
matlab_cmd << "title('CFR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
matlab_cmd << std::endl;

// Add the complete command string to MATLAB export
m_file.Add(matlab_cmd.str());

    return EXIT_SUCCESS;
}