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
#include <csignal>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <matlab_export/matlab_export.h>
#include <multisync/multisync.h>
#include <multi_rx/multi_rx.h>
#include <zmq_socket/zmq_socket.h>

#define NUM_CHANNELS 2                              // Number of Channels (USRP-devices)   
#define OUTFILE "./matlab/example_usrp_music.m"     // Output file in MATLAB-format to store results
#define EXPORT_INTERFACE 'tcp://localhost:5555'     // Interface for zmq socket

using Sync_t = MultiSync<ofdmframesync>;

// Signal handler to stop by keaboard interrupt
std::atomic<bool> stop_signal_called(false);
void sig_int_handler(int) {
    stop_signal_called.store(true);
}

// custom data type to pass to callback function
struct callback_data {
    std::vector<std::complex<float>> buffer;        // Buffer to store detected symbols 
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
return 1;
}

// Main function 
int UHD_SAFE_MAIN(int argc, char *argv[]) {
    uhd::set_thread_priority_safe();
    std::signal(SIGINT, &sig_int_handler);

   // Receiver settings 
    unsigned long int ADC_RATE = 100e6;
    double rx_rate = 25e6f;
    // NOTE : the sample rate computation MUST be in double precision so
    //        that the UHD can compute its decimation rate properly
    unsigned int decim_rate = (unsigned int)(ADC_RATE / rx_rate);
    // ensure multiple of 2
    decim_rate = (decim_rate >> 1) << 1;
    // compute usrp sampling rate
    double usrp_rx_rate = ADC_RATE / (float)decim_rate;
    // compute the resampling rate (20MHz to 40 MHz = 0.5)
    float rx_resamp_rate = 0.5*rx_rate / usrp_rx_rate;

    //create USRP devices
    std::string device_args_0("addr=192.168.10.3");
    std::string device_args_1("addr=192.168.168.2");
    uhd::usrp::multi_usrp::sptr usrp_0 = uhd::usrp::multi_usrp::make(device_args_0);
    uhd::usrp::multi_usrp::sptr usrp_1 = uhd::usrp::multi_usrp::make(device_args_1);

    // Lock mboard clocks
    usrp_0->set_clock_source("internal", 0);  // internal clock source for device 0 
    usrp_1->set_time_source("mimo", 0);       // mimo clock source for device 1
    usrp_1->set_clock_source("mimo", 0);      // mimo clock source for device 1
    usrp_0->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 0 
    usrp_1->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 1

    std::cout << boost::format("Device 1 Clock-src: %s") % usrp_1->get_clock_source(0) << std::endl << std::endl;
    std::cout << boost::format("Device 1 Time-src: %s") % usrp_1->get_time_source(0) << std::endl << std::endl;
    
    //sleep a bit while the slave locks its time to the master
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    //always select the subdevice first, the channel mapping affects the other settings
    usrp_0->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 0 to use the A RX frontend (RX channel 0)
    usrp_1->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 1 to use the A RX frontend (RX channel 0)
 
    // set sample rate
    usrp_0->set_rx_rate(usrp_rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);
    usrp_1->set_rx_rate(usrp_rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);
    std::cout << boost::format("Required USRP RX Rate: %f Msps...") % (usrp_rx_rate / 1e6) << std::endl;
    std::cout << boost::format("Device 0 RX Rate: %f Msps...") % (usrp_0->get_rx_rate(0) / 1e6) << std::endl;
    std::cout << boost::format("Device 1 RX Rate: %f Msps...") % (usrp_1->get_rx_rate(0) / 1e6) << std::endl;
    std::cout << boost::format("Resampling-Rate: %f Msps...") % (rx_resamp_rate) << std::endl;

    // set freq
    uhd::tune_request_t tune_request(2220e6, 227e6);  // create a tune request with the desired frequency and local oscillator offset
    std::cout << boost::format("Tune Policy: %f") % (tune_request.rf_freq_policy) << std::endl;
    usrp_0->set_rx_freq(tune_request, 0);
    usrp_1->set_rx_freq(tune_request, 0);
    std::cout << boost::format("Device 0 RX Freq: %f MHz...") % (usrp_0->get_rx_freq(0) / 1e6) << std::endl;
    std::cout << boost::format("Device 1 RX Freq: %f MHz...") % (usrp_1->get_rx_freq(0) / 1e6) << std::endl;

    // set the rf gain
    usrp_0->set_rx_gain(30, 0);
    usrp_1->set_rx_gain(30, 0);
    std::cout << boost::format("Device 0 RX Gain: %f dB...") % usrp_0->get_rx_gain(0) << std::endl;
    std::cout << boost::format("Device 1 RX Gain: %f dB...") % usrp_1->get_rx_gain(0) << std::endl;

    // Print Bandwidth 
    std::cout << boost::format("Device 0 RX Bandwidth: %f MHz...") % (usrp_0->get_rx_bandwidth(0) / 1e6) << std::endl;
    std::cout << boost::format("Device 1 RX Bandwidth: %f MHz...") % (usrp_1->get_rx_bandwidth(0) / 1e6) << std::endl << std::endl;

    // set the antenna
    usrp_0->set_rx_antenna("RX2", 0);
    usrp_1->set_rx_antenna("RX2", 0);
    
    // Receive stream confguration
    uhd::stream_args_t stream_args("fc32");                                                 // convert internal sc16 to complex float 32
    stream_args.args["recv_buff_size"] = "100000000"; // 100MB Buffer
    uhd::rx_streamer::sptr rx_stream_0 = usrp_0->get_rx_stream(stream_args);                // create receive streams 
    uhd::rx_streamer::sptr rx_stream_1 = usrp_1->get_rx_stream(stream_args);                // cretae a receive stream 
    size_t max_samps = rx_stream_0->get_max_num_samps();  

    // Resamplers
    std::array<resamp_crcf, 2> resamplers = {
        resamp_crcf_create(rx_resamp_rate, 12, 0.45f, 60.0f, 32),
        resamp_crcf_create(rx_resamp_rate, 12, 0.45f, 60.0f, 32)
    };
     
    // Thread-safe queues 
    std::array<RxSamplesQueue_t, 2> rx_queues;
    CfrQueue_t cfr_queue;

    // callback data
    std::array<callback_data, NUM_CHANNELS> cb_data;
    void* userdata[NUM_CHANNELS];

    // Array of Pointers to CB-Data 
    for (unsigned int i = 0; i < NUM_CHANNELS; ++i)
        userdata[i] = &cb_data[i];
        
    // Create multi frame synchronizer
    unsigned int M           = 64;      // number of subcarriers 
    unsigned int cp_len      = 16;      // cyclic prefix length (800ns for 20MHz => 16 Sample)
    unsigned int taper_len   = 4;       // window taper length 
    unsigned char p[M];                 // subcarrier allocation array
    ofdmframe_init_default_sctype(M, p);
    Sync_t ms(NUM_CHANNELS, {M, cp_len, taper_len, p}, callback, userdata);

    // ZMQ socket for data export 
    ZmqSender sender("tcp://*:5555");

    // Matlab Export destination file
    MatlabExport m_file(OUTFILE);

    // Configure stream command
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.num_samps = max_samps;                                                   // number of samples to receive per frame
    stream_cmd.stream_now = false;  
    double seconds_in_future = 5.0;            
    stream_cmd.time_spec = usrp_0->get_time_now() + uhd::time_spec_t(seconds_in_future);  // set the time spec to start receiving in the future

    // Start USRPs streaming
    usrp_0->issue_stream_cmd(stream_cmd); 
    usrp_1->issue_stream_cmd(stream_cmd); 

    // Start receiving...
    std::thread t0(rx_worker<4096>, rx_stream_0, std::ref(rx_queues[0]), std::ref(stop_signal_called));
    std::thread t1(rx_worker<4096>, rx_stream_1, std::ref(rx_queues[1]), std::ref(stop_signal_called));
    std::thread t2(sync_worker<NUM_CHANNELS, Sync_t, callback_data>, 
        std::ref(resamplers), std::ref(ms), 
        std::ref(cb_data), std::ref(rx_queues),
        std::ref(cfr_queue), std::ref(stop_signal_called));
    std::thread t3(export_worker<NUM_CHANNELS>, std::ref(cfr_queue), double(1), std::ref(sender), std::ref(m_file), std::ref(stop_signal_called));

    // Let it run for a while...
    std::this_thread::sleep_for(std::chrono::seconds(500));

    // Signal stop
    usrp_0->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    usrp_1->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);

    rx_queues[0].cv.notify_all();
    rx_queues[1].cv.notify_all();
    cfr_queue.cv.notify_all();

    t0.join();
    t1.join();
    t2.join();
    t3.join();

    for (auto& r : resamplers) {
    resamp_crcf_destroy(r);
    }

    std::cout << "Stopped receiving...\n" << std::endl;

return EXIT_SUCCESS;
}