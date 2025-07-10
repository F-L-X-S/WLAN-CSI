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
#include <zmq_socket/zmq_socket.h>

#define NUM_CHANNELS 2                              // Number of Channels (USRP-devices)   
#define OUTFILE "./matlab/example_usrp_music.m"     // Output file in MATLAB-format to store results
#define EXPORT_INTERFACE 'tcp://localhost:5555'     // Interface for zmq socket

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

// Thread-safe queues for received samples
std::queue<std::vector<std::complex<float>>> rx_queue_0;
std::queue<std::vector<std::complex<float>>> rx_queue_1;
std::mutex mtx_0, mtx_1;
std::condition_variable cv_0, cv_1;
std::atomic<bool> finished{false};

void rx_worker(uhd::rx_streamer::sptr rx_stream,
               std::queue<std::vector<std::complex<float>>>& queue,
               std::mutex& mtx,
               std::condition_variable& cv,
               const std::string& name) {
    uhd::rx_metadata_t md;
    std::vector<std::complex<float>> buff(8192);

    while (!finished.load()) {
        size_t n_rx = rx_stream->recv(&buff.front(), buff.size(), md, 1.0);
        std::vector<std::complex<float>> samples(buff.begin(), buff.begin() + n_rx);
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(std::move(samples));
        }
        cv.notify_one();
    }
}

void main_processor(std::array<resamp_crcf, 2>& resamplers, ZmqSender& sender) {

    // callback data
    struct callback_data cb_data[NUM_CHANNELS]; 
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
    MultiSync<ofdmframesync> ms(NUM_CHANNELS, {M, cp_len, taper_len, p}, callback, userdata);

    // CFR Buffer 
    std::vector<std::vector<std::complex<float>>> cfr(NUM_CHANNELS);

    while (!finished.load()) {
        std::vector<std::complex<float>> samples_0, samples_1;

        // Process queue 0
        {
            std::unique_lock<std::mutex> lock(mtx_0);
            cv_0.wait(lock, [] { return !rx_queue_0.empty() || finished.load(); });

            if (!rx_queue_0.empty()) {
                samples_0 = std::move(rx_queue_0.front());
                rx_queue_0.pop();
            }
        }

        // Process queue 1
        {
            std::unique_lock<std::mutex> lock(mtx_1);
            cv_1.wait(lock, [] { return !rx_queue_1.empty() || finished.load(); });

            if (!rx_queue_1.empty()) {
                samples_1 = std::move(rx_queue_1.front());
                rx_queue_1.pop();
            }
        }

        // Detect Packets 
        std::vector<std::complex<float>> rx_sample(1); 
        unsigned int num_written = 0; 
        auto now = std::chrono::steady_clock::now();
        std::array<std::chrono::steady_clock::time_point, NUM_CHANNELS> last_cfr_time = {now, now};
        unsigned int max_age = 1000;                                            // Maximum age of CFR in ms before discard

        for (unsigned int i = 0; i < samples_0.size(); ++i) {
                // Process Channel 0
                resamp_crcf_execute(resamplers[0], samples_0[i], &rx_sample[0], &num_written);   // Downsampling to 20MHz 
                ms.Execute(0, &rx_sample);                                                          // Execute Synchronizer 
                if (cb_data[0].buffer.size() && cfr[0].empty()){                            // Store the CFR 
                    cfr[0].assign(M, std::complex<float>(0.0f, 0.0f));                      // initialize CFR Buffer 
                    ms.GetCfr(0, &cfr[0], M);                                               // Write cfr to callback data
                    last_cfr_time[0] = now;                                                 // Update timestamp
                    std::cout << "Captured CFR for channel 0!" << std::endl;      
                    break;
                } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cfr_time[0]).count()>max_age && !cfr[0].empty())
                {
                    cfr[0].clear();             // Clear the CFR buffer for channel 0
                    cb_data[0].buffer.clear();  // Clear the buffer for channel 0
                    std::cout << "Discarded CFR for channel 0!" << std::endl;      
                };
        };
        now = std::chrono::steady_clock::now();
        for (unsigned int i = 0; i < samples_1.size(); ++i) {
                // Process Channel 1
                resamp_crcf_execute(resamplers[1], samples_1[i], &rx_sample[0], &num_written);   // Downsampling to 20MHz 
                ms.Execute(1, &rx_sample);                                                          // Execute Synchronizer      
                if (cb_data[1].buffer.size() && cfr[1].empty()){                            // Store the CFR 
                    cfr[1].assign(M, std::complex<float>(0.0f, 0.0f));                      // initialize CFR Buffer 
                    ms.GetCfr(1, &cfr[1], M);                                               // Write cfr to callback data            
                    last_cfr_time[1] = now;                                                 // Update timestamp
                    std::cout << "Captured CFR for channel 1!" << std::endl;    
                    break;                                       
                } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cfr_time[1]).count()>max_age && !cfr[1].empty())
                {
                    cfr[1].clear();             // Clear the CFR buffer for channel 1
                    cb_data[1].buffer.clear();  // Clear the buffer for channel 1
                    std::cout << "Discarded CFR for channel 1!" << std::endl;   
                };
            };
        
        // Synchronize NCOs of all channels to the average NCO frequency and phase
        ms.SynchronizeNcos();

        // Export CFR to ZMQ socket
        if (!cfr[0].empty() && !cfr[1].empty()){
            std::cout << "Exporting..." << std::endl;
            sender.send(cfr); 
            cfr[0].clear();  // Clear the CFR buffer for channel 0
            cfr[1].clear();  // Clear the CFR buffer for channel 1
            cb_data[0].buffer.clear();  // Clear the buffer for channel 0
            cb_data[1].buffer.clear();  // Clear the buffer for channel 1
            std::cout << "Exported CFRs to ZMQ!\n" << std::endl;
        };
    }
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
    // compute the resampling rate
    double rx_resamp_rate = 0.5* rx_rate / usrp_rx_rate;


    //create USRP devices
    std::string device_args_1("addr=192.168.10.3");
    uhd::usrp::multi_usrp::sptr usrp_0 = uhd::usrp::multi_usrp::make(device_args_1);
    std::string device_args_2("addr=192.168.168.2");
    uhd::usrp::multi_usrp::sptr usrp_1 = uhd::usrp::multi_usrp::make(device_args_2);

    // Lock mboard clocks
    usrp_0->set_clock_source("internal", 0);  // internal clock source for device 0 
    usrp_1->set_time_source("mimo", 0);       // mimo clock source for device 1
    usrp_1->set_clock_source("mimo", 0);      // mimo clock source for device 1
    usrp_0->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 0 
    usrp_1->set_time_now(uhd::time_spec_t(0.0), 0);   // set time for device 1
    
    //sleep a bit while the slave locks its time to the master
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    //always select the subdevice first, the channel mapping affects the other settings
    usrp_0->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 0 to use the A RX frontend (RX channel 0)
    usrp_1->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);  //set the device 1 to use the A RX frontend (RX channel 0)
 
    // set sample rate
    usrp_0->set_rx_rate(usrp_rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);
    usrp_1->set_rx_rate(usrp_rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);
    std::cout << boost::format("Device 0 RX Rate: %f Msps...") % (usrp_0->get_rx_rate(0) / 1e6) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Rate: %f Msps...") % (usrp_1->get_rx_rate(0) / 1e6) << std::endl << std::endl;

    // set freq
    uhd::tune_request_t tune_request(2220e6, 227e6);  // create a tune request with the desired frequency and local oscillator offset
    std::cout << boost::format("Tune Policy: %f") % (tune_request.rf_freq_policy) << std::endl;
    usrp_0->set_rx_freq(tune_request, 0);
    usrp_1->set_rx_freq(tune_request, 0);
    std::cout << boost::format("Device 0 RX Freq: %f MHz...") % (usrp_0->get_rx_freq(0) / 1e6) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Freq: %f MHz...") % (usrp_1->get_rx_freq(0) / 1e6) << std::endl << std::endl;

    // set the rf gain
    usrp_0->set_rx_gain(20, 0);
    usrp_1->set_rx_gain(20, 0);
    std::cout << boost::format("Device 0 RX Gain: %f dB...") % usrp_0->get_rx_gain(0) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Gain: %f dB...") % usrp_1->get_rx_gain(0) << std::endl << std::endl;

    // Print Bandwidth 
    std::cout << boost::format("Device 0 RX Bandwidth: %f MHz...") % (usrp_0->get_rx_bandwidth(0) / 1e6) << std::endl << std::endl;
    std::cout << boost::format("Device 1 RX Bandwidth: %f MHz...") % (usrp_1->get_rx_bandwidth(0) / 1e6) << std::endl << std::endl;

    // set the antenna
    usrp_0->set_rx_antenna("RX2", 0);
    usrp_1->set_rx_antenna("RX2", 0);
    
    // Resamplers
    std::array<resamp_crcf, 2> resamplers = {
        resamp_crcf_create(rx_resamp_rate, 12, 0.45f, 60.0f, 32),
        resamp_crcf_create(rx_resamp_rate, 12, 0.45f, 60.0f, 32)
    };
     
    // Receive stream confguration
    uhd::stream_args_t stream_args("fc32");                                                 // convert internal sc16 to complex float 32
    stream_args.args["recv_buff_size"] = "100000000"; // 100MB Buffer
    uhd::rx_streamer::sptr rx_stream_0 = usrp_0->get_rx_stream(stream_args);                // create receive streams 
    uhd::rx_streamer::sptr rx_stream_1 = usrp_1->get_rx_stream(stream_args);                // cretae a receive stream 
    size_t max_samps = rx_stream_0->get_max_num_samps();  

    // ZMQ socket for data export 
    ZmqSender sender("tcp://*:5555");

    // Start receiving samples 
    uhd::rx_metadata_t md;                      // Metadata Buffer for recv
    double seconds_in_future = 1.0;             // delay between receive cycles in seconds  
    double timeout = seconds_in_future+0.2;     //timeout (delay before receive + padding)

    // Stream command
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.num_samps = max_samps;                                                   // number of samples to receive per frame
    stream_cmd.stream_now = true;  
    //stream_cmd.time_spec = usrp_0->get_time_now() + uhd::time_spec_t(seconds_in_future);  // set the time spec to start receiving in the future
    usrp_0->issue_stream_cmd(stream_cmd); 
    usrp_1->issue_stream_cmd(stream_cmd); 

    std::thread t0(rx_worker, rx_stream_0, std::ref(rx_queue_0), std::ref(mtx_0), std::ref(cv_0), "RX0");
    std::thread t1(rx_worker, rx_stream_1, std::ref(rx_queue_1), std::ref(mtx_1), std::ref(cv_1), "RX1");
    std::thread main_thread(main_processor,  std::ref(resamplers), std::ref(sender));

    // Let it run for a while...
    std::this_thread::sleep_for(std::chrono::seconds(500));

    // Signal stop
    finished = true;
    cv_0.notify_all();
    cv_1.notify_all();

    t0.join();
    t1.join();
    main_thread.join();

    usrp_0->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    usrp_1->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    std::cout << "Stopped receiving...\n" << std::endl;

    // Destroy objects
    for (auto& r : resamplers) {
    resamp_crcf_destroy(r);
}

// ----------------- MATLAB output ----------------------
// MatlabExport m_file(OUTFILE);

// // Export CFRs to MATLAB file
// for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
//     std::string suffix = std::to_string(i);
//     m_file.Add(cfr[i], "cfr_" + suffix);
// }

// // Add combined plot-commands to the MATLAB file
// std::stringstream matlab_cmd;

// matlab_cmd << "figure;";
// // CFR Magnitude
// matlab_cmd << "subplot(2,1,1); hold on;";
// for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
//     std::string suffix = std::to_string(i);
//     matlab_cmd << "plot(abs(cfr_" << suffix << "), 'DisplayName', 'RX-Channel " << suffix << "');";
// }
// matlab_cmd << "title('Channel Frequency Response Gain'); legend; grid on;";
// matlab_cmd << std::endl;

// // CFR Phase
// matlab_cmd << "subplot(2,1,2); hold on;";
// for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
//     std::string suffix = std::to_string(i);
//     matlab_cmd << "plot(angle(cfr_" << suffix << "), 'DisplayName', 'RX-Channel " << suffix << "');";
// }
// matlab_cmd << "title('Channel Frequency Response Phase'); legend; grid on;";
// matlab_cmd << std::endl;

// // CFR in Complex 
// matlab_cmd << "figure;";
// for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
//     std::string suffix = std::to_string(i);
//     matlab_cmd << "plot(real(cfr_" << suffix << "), imag(cfr_" << suffix
//                << "), '.', 'DisplayName', 'RX-Channel " << suffix << "'); hold on;";
// }
// matlab_cmd << "title('CFR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
// matlab_cmd << std::endl;

// // Add the complete command string to MATLAB export
// m_file.Add(matlab_cmd.str());

//     return EXIT_SUCCESS;
}