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

#define NUM_CHANNELS 2                                          // Number of Channels (USRP-devices)   
#define OUTFILE_CFR "./matlab/example_usrp_music/cfr.m"         // Output file in MATLAB-format to store results
#define OUTFILE_CBDATA "./matlab/example_usrp_music/cbdata.m"   // Output file in MATLAB-format to store results
#define EXPORT_INTERFACE 'tcp://localhost:5555'                 // Interface for zmq socket

// Use OFDM-frame synchronizer for multi-channel synchronization
using Sync_t = MultiSync<ofdmframesync>;

// Signal handler to stop by keaboard interrupt
std::atomic<bool> stop_signal_called(false);
void sig_int_handler(int) {
    stop_signal_called=true;
}

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
        static_cast<CallbackData_t*>(_cb_data)->buffer.push_back(_X[i]);  
    }
return 1;
}

// Main function 
int UHD_SAFE_MAIN(int argc, char *argv[]) {
    uhd::set_thread_priority_safe();
    std::signal(SIGINT, &sig_int_handler);

    // ZMQ socket for data export 
    ZmqSender sender("tcp://*:5555");

    // Matlab Export destination file
    MatlabExport m_file_cfr(OUTFILE_CFR);
    MatlabExport m_file_cbdata(OUTFILE_CBDATA);

   // Receiver settings 
    unsigned long int ADC_RATE = 100e6;
    double rx_rate = 20e6f;
    // NOTE : the sample rate computation MUST be in double precision so
    //        that the UHD can compute its decimation rate properly
    unsigned int decim_rate = (unsigned int)(ADC_RATE / rx_rate);
    // ensure multiple of 2
    decim_rate = (decim_rate >> 1) << 1;
    // compute usrp sampling rate
    double usrp_rx_rate = ADC_RATE / (float)decim_rate;
    // compute the resampling rate
    float rx_resamp_rate = rx_rate / usrp_rx_rate;

    //create USRP devices
    std::array<uhd::usrp::multi_usrp::sptr, 2> usrps {
        uhd::usrp::multi_usrp::make("addr=192.168.10.3"), 
        uhd::usrp::multi_usrp::make("addr=192.168.168.2")
    };


    // Lock mboard clocks
    usrps[0]->set_clock_source("internal", 0);  // internal clock source for device 0 
    usrps[1]->set_time_source("mimo", 0);       // mimo clock source for device 1
    usrps[1]->set_clock_source("mimo", 0);      // mimo clock source for device 1
    usrps[0]->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 0 
    usrps[1]->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 1

    std::cout << boost::format("Device 1 Clock-src: %s") % usrps[1]->get_clock_source(0) << std::endl << std::endl;
    std::cout << boost::format("Device 1 Time-src: %s") % usrps[1]->get_time_source(0) << std::endl << std::endl;
    
    //sleep a bit while the slave locks its time to the master
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    //always select the subdevice first, the channel mapping affects the other settings
    usrps[0]->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 0 to use the A RX frontend (RX channel 0)
    usrps[1]->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 1 to use the A RX frontend (RX channel 0)
 
    // set sample rate
    usrps[0]->set_rx_rate(usrp_rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);
    usrps[1]->set_rx_rate(usrp_rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);
    std::cout << boost::format("Required USRP RX Rate: %f Msps...") % (usrp_rx_rate / 1e6) << std::endl;
    std::cout << boost::format("Device 0 RX Rate: %f Msps...") % (usrps[0]->get_rx_rate(0) / 1e6) << std::endl;
    std::cout << boost::format("Device 1 RX Rate: %f Msps...") % (usrps[1]->get_rx_rate(0) / 1e6) << std::endl;
    std::cout << boost::format("Resampling-Rate: %f Msps...") % (rx_resamp_rate) << std::endl;

    // set freq
    uhd::tune_request_t tune_request(2412e6); 
    tune_request.rf_freq_policy = uhd::tune_request_t::policy_t::POLICY_AUTO;
    std::cout << boost::format("Tune Policy: %f") % (tune_request.rf_freq_policy) << std::endl;
    usrps[0]->set_rx_freq(tune_request, 0);
    usrps[1]->set_rx_freq(tune_request, 0);
    std::cout << boost::format("Device 0 RX Freq: %f MHz...") % (usrps[0]->get_rx_freq(0) / 1e6) << std::endl;
    std::cout << boost::format("Device 1 RX Freq: %f MHz...") % (usrps[1]->get_rx_freq(0) / 1e6) << std::endl;

    // set the rf gain
    usrps[0]->set_rx_gain(30, 0);
    usrps[1]->set_rx_gain(30, 0);
    std::cout << boost::format("Device 0 RX Gain: %f dB...") % usrps[0]->get_rx_gain(0) << std::endl;
    std::cout << boost::format("Device 1 RX Gain: %f dB...") % usrps[1]->get_rx_gain(0) << std::endl;

    // Print Bandwidth 
    std::cout << boost::format("Device 0 RX Bandwidth: %f MHz...") % (usrps[0]->get_rx_bandwidth(0) / 1e6) << std::endl;
    std::cout << boost::format("Device 1 RX Bandwidth: %f MHz...") % (usrps[1]->get_rx_bandwidth(0) / 1e6) << std::endl << std::endl;

    // set the antenna
    usrps[0]->set_rx_antenna("RX2", 0);
    usrps[1]->set_rx_antenna("RX2", 0);
    
    // Receive stream confguration
    uhd::stream_args_t stream_args("fc32");                                                 // convert internal sc16 to complex float 32
    stream_args.args["recv_buff_size"] = "100000000"; // 100MB Buffer
    uhd::rx_streamer::sptr rx_stream_0 = usrps[0]->get_rx_stream(stream_args);                // create receive streams 
    uhd::rx_streamer::sptr rx_stream_1 = usrps[1]->get_rx_stream(stream_args);                // cretae a receive stream 
    size_t max_samps = rx_stream_0->get_max_num_samps();  

    // Resamplers
    std::array<resamp_crcf, 2> resamplers = {
        resamp_crcf_create_default(rx_resamp_rate),
        resamp_crcf_create_default(rx_resamp_rate)
    };
     
    // Thread-safe queues 
    std::array<RxSamplesQueue_t, 2> rx_queues;
    CfrQueue_t cfr_queue;
    CbDataQueue_t cbdata_queue;

    // callback data
    std::array<CallbackData_t, NUM_CHANNELS> cb_data;
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

    // Start receiving...
    std::thread t5(stream_worker<NUM_CHANNELS>, std::ref(usrps), std::ref(max_samps), std::ref(stop_signal_called));
    std::thread t0(rx_worker<4096>, rx_stream_0, std::ref(rx_queues[0]), std::ref(stop_signal_called));
    std::thread t1(rx_worker<4096>, rx_stream_1, std::ref(rx_queues[1]), std::ref(stop_signal_called));
    std::thread t2(sync_worker<NUM_CHANNELS, Sync_t, CallbackData_t>, 
        std::ref(resamplers), std::ref(ms), 
        std::ref(cb_data), std::ref(rx_queues), 
        std::ref(cfr_queue), std::ref(cbdata_queue),std::ref(stop_signal_called));
    std::thread t3(cfr_export_worker<NUM_CHANNELS>, std::ref(cfr_queue), double(0.05), std::ref(sender), std::ref(m_file_cfr), std::ref(stop_signal_called));
    std::thread t4(cbdata_export_worker, std::ref(cbdata_queue), std::ref(m_file_cbdata), std::ref(stop_signal_called));

    std::this_thread::sleep_for(std::chrono::milliseconds(20000));
    stop_signal_called.store(true);

    rx_queues[0].cv.notify_all();
    rx_queues[1].cv.notify_all();
    cfr_queue.cv.notify_all();
    cbdata_queue.cv.notify_all();

    t5.join();
    t0.join();
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    for (auto& r : resamplers) {
    resamp_crcf_destroy(r);
    }

    std::cout << "Stopped receiving...\n" << std::endl;

return EXIT_SUCCESS;
}