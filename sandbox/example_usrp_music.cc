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
#define SYMBOLS_PER_FRAME 1                                     // Number of Symbols to send per frame 
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

    // USRP Constants
    unsigned long int DAC_RATE = 400e6;             // USRP DAC Rate (N210 fixed to 400MHz)
    unsigned long int ADC_RATE = 100e6;             // USRP ADC Rate (N210 fixed to 100MHz)

    // TX/RX Settings 
    double bandwidth = 3e6f;                        // Bandwidth 
    double center_freq = 433.55e6;                  // Carrier frequency in free band 
    double txrx_rate = 4*bandwidth;                 // Sample rate  
    unsigned int tx_cycle = 250;                    // Transmit every ... [ms]
    double max_age = 0.45*(double)tx_cycle/1000;    // max time delta between CFRs to group together [s]

    // TX 
    // NOTE : the sample rate computation MUST be in double precision so
    //        that the UHD can compute its interpolation rate properly
    unsigned int interp_rate = (unsigned int)(DAC_RATE / txrx_rate);
    interp_rate = (interp_rate >> 2) << 2;      // ensure multiple of 4
    double usrp_tx_rate = DAC_RATE / (double)interp_rate;

    // RX
    // NOTE : the sample rate computation MUST be in double precision so
    //        that the UHD can compute its decimation rate properly
    unsigned int decim_rate = (unsigned int)(ADC_RATE / txrx_rate);
    decim_rate = (decim_rate >> 1) << 1;        // ensure multiple of 2
    double usrp_rx_rate = ADC_RATE / (float)decim_rate;

    // ---------------------- Signal Generation in complex baseband ----------------------
    unsigned int M           = 32;      // number of subcarriers 
    unsigned int cp_len      = 8 ;      // cyclic prefix length (800ns for 20MHz => 16 Sample)
    unsigned int taper_len   = 2;       // window taper length 
    unsigned char p[M];                 // subcarrier allocation array

    unsigned int frame_len   = M + cp_len;
    unsigned int frame_samples = (3+SYMBOLS_PER_FRAME)*frame_len; // S0a + S0b + S1 + data symbols

    // initialize subcarrier allocation
    ofdmframe_init_default_sctype(M, p);

    // create subcarrier notch in upper half of band
    unsigned int n0 = (unsigned int) (0.13 * M);    // lower edge of notch
    unsigned int n1 = (unsigned int) (0.21 * M);    // upper edge of notch
    for (size_t i=n0; i<n1; i++)
        p[i] = OFDMFRAME_SCTYPE_NULL;

    // create frame generator
    ofdmframegen fg = ofdmframegen_create(M, cp_len, taper_len, p);

    std::vector<std::complex<float>> tx_base(frame_samples);     // complex baseband signal buffer
    std::vector<std::complex<float>> X(M);                       // channelized symbols
    unsigned int n=0;                                            // Sample number in time domains

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
        ofdmframegen_writesymbol(fg, X.data(), &tx_base[n]);
        n += frame_len;
    }

    // TX half band resampler -> interpolation by 2 
    resamp2_crcf interp = resamp2_crcf_create(7,0.0f,40.0f);
    std::vector<std::complex<float>> tx_base_interp(2*n);
    for (unsigned int j=0; j<n; j++)
        resamp2_crcf_interp_execute(interp, tx_base[j], &tx_base_interp[2*j]);
    resamp2_crcf_destroy(interp);


   // ---------------------- Configure USRPs ----------------------
    //create USRP devices
    std::array<uhd::usrp::multi_usrp::sptr, 2> usrps {
        uhd::usrp::multi_usrp::make("addr=192.168.10.3"), 
        uhd::usrp::multi_usrp::make("addr=192.168.168.2")
    };

    // Receive stream confguration
    uhd::stream_args_t stream_args("fc32");                                                   // convert internal sc16 to complex float 32
    stream_args.args["recv_buff_size"] = "100000000"; // 100MB Buffer
    uhd::rx_streamer::sptr rx_stream_0 = usrps[0]->get_rx_stream(stream_args);                // create receive streams 
    uhd::rx_streamer::sptr rx_stream_1 = usrps[1]->get_rx_stream(stream_args);                // cretae a receive stream 
    size_t max_samps = rx_stream_0->get_max_num_samps();  

    // Start streaming
    std::thread t0(stream_worker<NUM_CHANNELS>, std::ref(usrps), 
        std::ref(max_samps), std::ref(usrp_tx_rate), std::ref(usrp_rx_rate), std::ref(center_freq), 
        double(750) ,std::ref(stop_signal_called));
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // ---------------------- Configure Receive workers ----------------------
    // TX stream configuration 
    uhd::tx_streamer::sptr tx_stream_0 = usrps[0]->get_tx_stream(stream_args); 

    // RX Resampling rate = baseband-bw/rx-bandwidth
    usrp_rx_rate = usrps[0]->get_rx_rate(0);
    double rx_resamp_rate = txrx_rate / usrp_rx_rate;
    std::cout << boost::format("RX Resampling Rate (usrp-rate/rx-rate): %f ") % (rx_resamp_rate) << std::endl;

    // RX Resamplers
    std::array<resamp_crcf, 2> resamplers = {
        resamp_crcf_create_default(0.5*rx_resamp_rate),
        resamp_crcf_create_default(0.5*rx_resamp_rate)
    };

    // callback data
    std::array<CallbackData_t, NUM_CHANNELS> cb_data;
    void* userdata[NUM_CHANNELS];

    // Array of Pointers to CB-Data 
    for (unsigned int i = 0; i < NUM_CHANNELS; ++i)
        userdata[i] = &cb_data[i];
        
    // Create multi frame synchronizer
    ofdmframe_init_default_sctype(M, p);
    Sync_t ms(NUM_CHANNELS, {M, cp_len, taper_len, p}, callback, userdata);

    // Thread-safe queues 
    std::array<RxSamplesQueue_t, 2> rx_queues;

    std::thread t1(rx_worker<4096>, rx_stream_0, std::ref(rx_queues[0]), std::ref(stop_signal_called));
    std::thread t2(rx_worker<4096>, rx_stream_1, std::ref(rx_queues[1]), std::ref(stop_signal_called));

    // ---------------------- Configure Export workers ----------------------
    // Thread-safe queues 
    CfrQueue_t cfr_queue;
    CbDataQueue_t cbdata_queue;

    std::thread t3(sync_worker<NUM_CHANNELS, Sync_t, CallbackData_t>, 
        std::ref(resamplers), std::ref(ms), 
        std::ref(cb_data), std::ref(rx_queues), 
        std::ref(cfr_queue), std::ref(cbdata_queue),std::ref(stop_signal_called));
    
    std::thread t4(cfr_export_worker<NUM_CHANNELS>, std::ref(cfr_queue), 
        max_age, std::ref(sender), std::ref(m_file_cfr), std::ref(stop_signal_called));
    
    std::thread t5(cbdata_export_worker, std::ref(cbdata_queue), std::ref(m_file_cbdata), std::ref(stop_signal_called));

    // ---------------------- Configure Transmit workers ----------------------

    // TX Arbitrary Resampler 
    usrp_tx_rate = usrps[0]->get_tx_rate(0);
    double tx_resamp_rate = usrp_tx_rate / txrx_rate;
    std::cout << boost::format("TX Resampling Rate (usrp-rate/tx-rate): %f ") % (tx_resamp_rate) << std::endl;

    unsigned int nw, tx_len = (unsigned int)(frame_samples*ceil(tx_resamp_rate)*2);
    std::vector<std::complex<float>> tx_data(tx_len);
    resamp_crcf resamp_tx = resamp_crcf_create(tx_resamp_rate,7,0.4f,60.0f,64);
    resamp_crcf_set_rate(resamp_tx, tx_resamp_rate);
    n=0;
    for (unsigned int j=0; j<tx_base_interp.size(); j++) {
        resamp_crcf_execute(resamp_tx, tx_base_interp[j], &tx_data[n], &nw);
        n += nw;
    };
    resamp_crcf_destroy(resamp_tx);

    // Transmission thread 
    std::thread t6(tx_worker, std::ref(tx_stream_0), std::ref(tx_data), tx_cycle, std::ref(stop_signal_called));

    // ---------------------- Continue in main thread ----------------------
    std::this_thread::sleep_for(std::chrono::milliseconds(60000));
    stop_signal_called.store(true);

    rx_queues[0].cv.notify_all();
    rx_queues[1].cv.notify_all();
    cfr_queue.cv.notify_all();
    cbdata_queue.cv.notify_all();

    t0.join();
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();

    for (auto& r : resamplers) {
    resamp_crcf_destroy(r);
    }

    std::cout << "Stopped receiving...\n" << std::endl;

return EXIT_SUCCESS;
}