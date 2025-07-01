/**
 * @file example_usrp_music.cc
 * @author Felix Schuelke 
 * @brief USRP integartion based on Ettus example project https://kb.ettus.com/Getting_Started_with_UHD_and_C%2B%2B
 * @version 0.1
 * @date 2025-07-01
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
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>

#include <matlab_export/matlab_export.h>
#include <multisync/multisync.h>
#include <zmq_socket/zmq_socket.h>

#define NUM_FRAMES 10                               // Number of frames to receive before stopping    
#define OUTFILE "./matlab/example_usrp_music.m"     // Output file in MATLAB-format to store results
#define EXPORT_INTERFACE 'tcp://localhost:5555'     // Interface for zmq socket


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

    //create a multi usrp device
    uhd::device_addr_t dev_addr;
    dev_addr["addr0"] = "192.168.10.3";
    dev_addr["addr1"] = "192.168.10.4";
    std::cout << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(dev_addr);

    // Lock mboard clocks
    usrp->set_clock_source("internal", 0);  // internal clock source for device 0 
    usrp->set_time_source("mimo", 1);       // mimo clock source for device 1
    usrp->set_clock_source("mimo", 1);      // mimo clock source for device 1
    usrp->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 0 
    usrp->set_time_now(uhd::time_spec_t(0.0), 1);   // set time for device 1
    
    //sleep a bit while the slave locks its time to the master
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    //always select the subdevice first, the channel mapping affects the other settings
    usrp->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 0 to use the A RX frontend (RX channel 0)
    usrp->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 1);  //set the device 1 to use the A RX frontend (RX channel 0)
 
    // set sample rate
    usrp->set_rx_rate(25e6, uhd::usrp::multi_usrp::ALL_MBOARDS);
    std::cout << boost::format("Device 0 RX Rate: %f Msps...") % (usrp->get_rx_rate(0) / 1e6) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Rate: %f Msps...") % (usrp->get_rx_rate(1) / 1e6) << std::endl << std::endl;

    // set freq
    uhd::tune_request_t tune_request(2220e6, 227e6);  // create a tune request with the desired frequency and local oscillator offset
    std::cout << boost::format("Tune Policy: %f") % (tune_request.rf_freq_policy) << std::endl;
    usrp->set_rx_freq(tune_request, 0);
    usrp->set_rx_freq(tune_request, 1);
    std::cout << boost::format("Device 0 RX Freq: %f MHz...") % (usrp->get_rx_freq(0) / 1e6) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Freq: %f MHz...") % (usrp->get_rx_freq(1) / 1e6) << std::endl << std::endl;

    // set the rf gain
    usrp->set_rx_gain(20, 0);
    usrp->set_rx_gain(20, 1);
    std::cout << boost::format("Device 0 RX Gain: %f dB...") % usrp->get_rx_gain(0) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Gain: %f dB...") % usrp->get_rx_gain(1) << std::endl << std::endl;

    // set the IF filter bandwidth
    usrp->set_rx_bandwidth(40e6, 0);
    usrp->set_rx_bandwidth(40e6, 1);
    std::cout << boost::format("Device 0 RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth(0) / 1e6) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth(1) / 1e6) << std::endl << std::endl;

    // set the antenna
    usrp->set_rx_antenna("RX2", 0);
    usrp->set_rx_antenna("RX2", 1);

    // callback data
    struct callback_data cb_data[NUM_FRAMES]; 
    void* userdata[NUM_FRAMES];

    // Array of Pointers to CB-Data 
    for (unsigned int i = 0; i < NUM_FRAMES; ++i)
        userdata[i] = &cb_data[i];
        
    // Create multi frame synchronizer
    unsigned int M           = 64;      // number of subcarriers 
    unsigned int cp_len      = 16;      // cyclic prefix length (800ns for 20MHz => 16 Sample)
    unsigned int taper_len   = 4;       // window taper length 
    unsigned char p[M];                 // subcarrier allocation array
    ofdmframe_init_default_sctype(M, p);
    MultiSync<ofdmframesync> ms(NUM_FRAMES, {M, cp_len, taper_len, p}, callback, userdata);

    // CFR Buffer 
    std::vector<std::vector<std::complex<float>>> cfr(NUM_FRAMES);
    cfr.assign(NUM_FRAMES, std::vector<std::complex<float>>(M, std::complex<float>(0.0f, 0.0f)));

    // Resampler 40MHz to 20MHz 
    float resamp_factor = 20e6 / 40e6; // signal-bandwidth/usrp-bandwidth  
    unsigned int num_written = 0; 
    resamp_crcf resamplers[2];
    for (int i = 0; i < 2; i++) {
        resamplers[i] = resamp_crcf_create(resamp_factor, 12, 0.45f, 60.0f, 32);
    }
     
    // Receive stream confguration
    uhd::stream_args_t stream_args("fc32", "sc16");                                     // convert internal sc16 to complex float 32
    stream_args.channels = {0,1};                                                       // Set the channels to receive from (0 and 1)
    uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);                // cretae a receive stream 
    size_t max_samps = rx_stream->get_max_num_samps();  

    // Allocate RX Stream buffer  
    std::vector<std::vector<std::complex<float> > > buffs(
        usrp->get_rx_num_channels(), std::vector<std::complex<float> >(max_samps)
    );
    std::vector<std::complex<float> *> buff_ptrs;   //vector of pointers to point to each of the channel buffers
    for (size_t i = 0; i < buffs.size(); i++) buff_ptrs.push_back(&buffs[i].front());

    // Stream command
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = max_samps;                                                   // number of samples to receive per frame
    stream_cmd.stream_now = false;   

    // Start receiving samples 
    uhd::rx_metadata_t md;                      // Metadata Buffer for recv()
    unsigned int num_frames = 0;                // received frames counter  
    double seconds_in_future = 0.05;            // delay between receive cycles in seconds  
    double timeout = seconds_in_future+0.02;     //timeout (delay before receive + padding)

    // ZMQ socket for data export 
    ZmqSender sender("tcp://*:5555");

    while (num_frames<NUM_FRAMES) {
        // Issue stream command to start receiving
        stream_cmd.time_spec = usrp->get_time_now() + uhd::time_spec_t(seconds_in_future);  // set the time spec to start receiving in the future
        usrp->issue_stream_cmd(stream_cmd);

        //receive a single packet from USRPs
        size_t num_rx_samps = rx_stream->recv(
            buff_ptrs, max_samps, md, timeout
        );

        std::cout << boost::format("Received %d samples...") % num_rx_samps << std::endl;

        //handle the error code
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            std::cout << "Timeout while receiving samples..." << std::endl;
            break;
        }
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
            throw std::runtime_error(str(boost::format(
                "Receiver error %s"
            ) % md.strerror()));
        }

        // Detect Packets 
        std::vector<std::complex<float>> rx_sample(1); 
        for (unsigned int i = 0; i < num_rx_samps; ++i) {
                // Process Channel 0
                resamp_crcf_execute(resamplers[0], buff_ptrs[0][i], &rx_sample[0], &num_written);   // Downsampling to 20MHz 
                ms.Execute(0, &rx_sample);                                                          // Execute Synchronizer 

                if (cb_data[0].buffer.size() && cb_data[0].cfr.empty()){                            // Store the CFR 
                    ms.GetCfr(0, &cb_data[0].cfr, M);                                               // Write cfr to callback data
                    cfr[0].assign(cb_data[0].cfr.begin(), cb_data[0].cfr.end());                    // Copy the CFR to the buffer  
                    std::cout << "Captured CFR for channel 0!\n" << std::endl;                                        
                };

                // Process Channel 1
                resamp_crcf_execute(resamplers[1], buff_ptrs[1][i], &rx_sample[0], &num_written);   // Downsampling to 20MHz 
                ms.Execute(1, &rx_sample);                                                          // Execute Synchronizer                 

                if (cb_data[1].buffer.size() && cb_data[1].cfr.empty()){                            // Store the CFR 
                    ms.GetCfr(1, &cb_data[1].cfr, M);                                               // Write cfr to callback data
                    cfr[1].assign(cb_data[1].cfr.begin(), cb_data[1].cfr.end());                    // Copy the CFR to the buffer  
                    std::cout << "Captured CFR for channel 1!\n" << std::endl;
                                                               
                };

                // Synchronize NCOs of all channels to the average NCO frequency and phase
                ms.SynchronizeNcos();

                // Export CFR to ZMQ socket
                if (!cb_data[0].cfr.empty() && !cb_data[1].cfr.empty()){
                    sender.send(cfr); 
                    cfr[0].clear();  // Clear the CFR buffer for channel 0
                    cfr[1].clear();  // Clear the CFR buffer for channel 1
                    std::cout << "Exported CFRs to ZMQ!\n" << std::endl;
                    num_frames++;
                }

                // Stop receiving if we have enough frames
                if (num_frames >= NUM_FRAMES) break;
        };
    };

    usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    std::cout << "Stopped receiving...\n" << std::endl;

// ----------------- MATLAB output ----------------------
MatlabExport m_file(OUTFILE);

// Export CFRs to MATLAB file
for (unsigned int i = 0; i < NUM_FRAMES; ++i) {
    std::string suffix = std::to_string(i);
    m_file.Add(cfr[i], "cfr_" + suffix);
}

// Add combined plot-commands to the MATLAB file
std::stringstream matlab_cmd;

matlab_cmd << "figure;";
// CFR Magnitude
matlab_cmd << "subplot(2,1,1); hold on;";
for (unsigned int i = 0; i < NUM_FRAMES; ++i) {
    std::string suffix = std::to_string(i);
    matlab_cmd << "plot(abs(cfr_" << suffix << "), 'DisplayName', 'RX-Channel " << suffix << "');";
}
matlab_cmd << "title('Channel Frequency Response Gain'); legend; grid on;";
matlab_cmd << std::endl;

// CFR Phase
matlab_cmd << "subplot(2,1,2); hold on;";
for (unsigned int i = 0; i < NUM_FRAMES; ++i) {
    std::string suffix = std::to_string(i);
    matlab_cmd << "plot(angle(cfr_" << suffix << "), 'DisplayName', 'RX-Channel " << suffix << "');";
}
matlab_cmd << "title('Channel Frequency Response Phase'); legend; grid on;";
matlab_cmd << std::endl;

// CFR in Complex 
matlab_cmd << "figure;";
for (unsigned int i = 0; i < NUM_FRAMES; ++i) {
    std::string suffix = std::to_string(i);
    matlab_cmd << "plot(real(cfr_" << suffix << "), imag(cfr_" << suffix
               << "), '.', 'DisplayName', 'RX-Channel " << suffix << "');";
}
matlab_cmd << "title('CFR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
matlab_cmd << std::endl;

// Add the complete command string to MATLAB export
m_file.Add(matlab_cmd.str());

    return EXIT_SUCCESS;
}