/**
 * @file multi_rx.h
 * @author Felix Schuelke (flxscode@gmail.com)
 * 
 * @brief 
 * @version 0.1
 * @date 2025-07-11
 * 
 * 
 */

#ifndef MULTIRX_H
#define MULTIRX_H

#include <uhd/usrp/multi_usrp.hpp>    
              
#include <vector>                     
#include <complex>      

#include <queue>       
#include <mutex>                    
#include <condition_variable>         
#include <string>              
#include <atomic>                     

#include <multisync/multisync.h>
#include <zmq_socket/zmq_socket.h>

// Typedefs
using RxSample_t = std::complex<float>;   // Received samples type
struct RxSamplesQueue_t
{
    std::queue<std::vector<RxSample_t>>& queue,
    std::mutex& mtx,
    std::condition_variable& cv
};

struct Cfr_t {
    std::vector<std::complex<float>> cfr;               // Channel Frequency Response
    std::chrono::steady_clock::time_point timestamp;    // Timestamp of the CFR
    unsigned int channel;                               // Channel index
};
struct CfrQueue_t
{
    std::queue<Cfr_t>& queue,
    std::mutex& mtx,
    std::condition_variable& cv
};


// RX-Worker receives samples from the USRP and pushes them into a thread-safe queue
template <std::size_t buffer_size>
void rx_worker( uhd::rx_streamer::sptr rx_stream,
                RxSamplesQueue_t& q) {
    uhd::rx_metadata_t md;
    std::vector<RxSample_t> buff(buffer_size);

    while (!finished.load()) {
        size_t n_rx = rx_stream->recv(&buff.front(), buff.size(), md, 1.0);
        std::vector<RxSample_t> samples(buff.begin(), buff.begin() + n_rx);
        {
            std::lock_guard<std::mutex> lock(q.mtx);
            q.queue.push(std::move(samples));
        }
        q.cv.notify_one();
    }
}

// Sync-Worker processes queued samples from RX-workers and pushes the resulting CFRs into a thread-safe queue
template <std::size_t num_channels, type_t syncronizer_type, type_t cb_data_type>
void sync_worker(   std::array<resamp_crcf, num_channels>& resamplers,
                    syncronizer_type& ms,
                    std::array<cb_data_type, num_channels>& cb_data,
                    std::array<RxSamplesQueue_t, num_channels>& rx_queues,
                    CfrQueue_t& cfr_queue,
                ) {

    auto now = std::chrono::steady_clock::now();
    std::vector<std::complex<float>> rx_sample(1); 
    std::array< std::vector<RxSample_t>, num_channels> samples;
    Cfr_t cfr;                                                        
    unsigned int i, j, num_written;

    while (!stop_signal_called.load()) {

        // Update timestamp
        now = std::chrono::steady_clock::now();

        // Process channels
        for (i = 0; i < num_channels; ++i) {
                // Clear samples
                samples[i].clear();

                // Process channel queue 
                std::unique_lock<std::mutex> lock_rx(rx_queues[i].mtx);
                rx_queues[i].cv.wait(lock_rx, [] { return !rx_queues[i].queue.empty() || stop_signal_called.load(); });

                if (!rx_queues[i].queue.empty()) {
                    samples[i] = std::move(rx_queues[i].queue.front());
                    rx_queues[i].queue.pop();
                }
                
                // Detect Packets 
                for (j = 0; j < samples[i].size(); ++j) {
                    // Resample to original carrier frequency
                    resamp_crcf_execute(resamplers[0], samples[i][j], &rx_sample[0], &num_written);  
                    
                    // Execute Synchronizer for channel i 
                    ms.Execute(i, &rx_sample);          
                    
                    // Check, if callback-data was updated by synchronizer
                    if (cb_data[i].buffer.size()){                         
                        cfr.cfr.assign(M, std::complex<float>(0.0f, 0.0f));                     // initialize CFR Buffer 
                        ms.GetCfr(0, &cfr.cfr, M);                                              // Write cfr to callback data
                        cfr.timestamp = now;                                                    // Update timestamp
                        cfr.channel = i;                                                        // Set channel index

                        // Push CFR to queue
                        {
                            std::lock_guard<std::mutex> lock_cfr(cfr_queue.mtx);
                            cfr_queue.queue.push(std::move(cfr));
                        }
                        cfr_queue.cv.notify_one();
                        std::cout << "Captured CFR for channel "<< i <<"!" << std::endl;      
                        break;
                    };
                };
        }

        // Synchronize NCOs of all channels to the average NCO frequency and phase
        ms.SynchronizeNcos();
    }
}

// Export-Worker processes queued CFRs from Sync-workers and either discards them or exports them to a ZMQ socket
template <std::size_t num_channels>
void export_worker(CfrQueue_t& cfr_queue, 
                    unsigned int max_age,
                    ZmqSender& sender) {

    // Queued CFRs of all channels and all times 
    std::vector<Cfr_t> cfr_buffer;         

    // Sorted and time-matched CFRs of all channels
    std::array<std::vector<std::complex<float>>, num_channels> cfr;

    unsigned int i;
    while (!stop_signal_called.load()) {
        // Clear samples
        cfr_buffer.clear();
        cfr.clear();

        // Move cfr queue to buffer
        std::lock_guard<std::mutex> lock_cfr(cfr_queue.mtx);
        cfr_queue.cv.wait(lock_cfr, [] { return !cfr_queue.queue.empty() || stop_signal_called.load(); });

        if (!cfr_queue.queue.empty()) {
            cfr_buffer.push_back(std::move(cfr_queue.queue.front()));
            cfr_queue.queue.pop();
        }

        // Sort CFRs by timestamp
        std::sort(cfr_buffer.begin(), cfr_buffer.end(),
            [](const Cfr_t& a, const Cfr_t& b) {
                return a.timestamp < b.timestamp;
            });

        // Find matching CFR groups within max_age
        for (i = 0; i < cfr_buffer.size(); ++i) {
            
        }

        // Export CFR to ZMQ socket
        if (!cfr.empty()) {
            sender.send(cfr); 
            std::cout << "Exported CFRs to ZMQ!\n" << std::endl;
        };
                
        }
}

#endif // MULTIRX_H