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

#include "liquid-wlan.h"
#include <multisync/multisync.h>
#include <zmq_socket/zmq_socket.h>
#include <matlab_export/matlab_export.h>

// Typedefs
using RxSample_t = std::complex<float>;   // Received samples type

struct RxSampleBlock_t
{
    std::vector<RxSample_t> samples;                    // Received samples
    uhd::time_spec_t timestamp;                         // Timestamp of the sample block
};

struct RxSamplesQueue_t
{
    std::queue<RxSampleBlock_t> queue;
    std::mutex mtx;
    std::condition_variable cv;
};

struct Cfr_t {
    std::vector<std::complex<float>> cfr;               // Channel Frequency Response
    uhd::time_spec_t  timestamp;                        // Timestamp of the CFR
    unsigned int channel;                               // Channel index
};

struct CfrQueue_t
{
    std::queue<Cfr_t> queue;
    std::mutex mtx;
    std::condition_variable cv;
};

struct CallbackData_t {
    std::vector<std::complex<float>> buffer;            // Buffer to store detected symbols 
    uhd::time_spec_t  timestamp;                        // Timestamp 
    unsigned int channel;                               // Channel index
};

struct CbDataQueue_t
{
    std::queue<CallbackData_t> queue;
    std::mutex mtx;
    std::condition_variable cv;
};


// RX-Worker receives samples from the USRP and pushes them into a thread-safe queue
template <std::size_t buffer_size>
void rx_worker( uhd::rx_streamer::sptr rx_stream,
                RxSamplesQueue_t& q,
                std::atomic<bool>& stop_signal_called) {
    uhd::rx_metadata_t md;
    std::vector<RxSample_t> buff(buffer_size);

    while (!stop_signal_called.load()) {
        size_t n_rx = rx_stream->recv(&buff.front(), buff.size(), md, 1.0);
        RxSampleBlock_t sample_block;
        sample_block.samples.assign(buff.begin(), buff.begin() + n_rx);
        sample_block.timestamp = md.time_spec;
        {
            std::lock_guard<std::mutex> lock(q.mtx);
            q.queue.push(std::move(sample_block));
        }
        q.cv.notify_one();
    }
}

// Sync-Worker processes queued samples from RX-workers and pushes the resulting CFRs into a thread-safe queue
template <std::size_t num_channels, typename syncronizer_type, typename cb_data_type>
void sync_worker(   std::array<resamp_crcf, num_channels>& resamplers,
                    syncronizer_type& ms,
                    std::array<CallbackData_t, num_channels>& cb_data,
                    std::array<RxSamplesQueue_t, num_channels>& rx_queues,
                    CfrQueue_t& cfr_queue,
                    CbDataQueue_t& cbdata_queue,
                    std::atomic<bool>& stop_signal_called
                ) {

    std::vector<RxSampleBlock_t> sample_blocks;
    std::vector<std::complex<float>> rx_sample(1);
    Cfr_t cfr;                                                        
    unsigned int i, j, num_written;

    while (!stop_signal_called.load()) {
        // Process channels
        for (i = 0; i < num_channels; ++i) {
                // Clear samples and Callback-data
                sample_blocks.clear();
                cb_data[i].buffer.clear();  

                // Process channel queue 
                std::unique_lock<std::mutex> lock_rx(rx_queues[i].mtx);
                rx_queues[i].cv.wait(lock_rx, [&rx_queues, i, &stop_signal_called] { 
                    return !rx_queues[i].queue.empty() || stop_signal_called.load(); 
                });

                while (!rx_queues[i].queue.empty()) {
                    sample_blocks.push_back(std::move(rx_queues[i].queue.front()));
                    rx_queues[i].queue.pop();
                }

                // Detect Packets 
                for (j = 0; j < sample_blocks.size(); ++j) {
                        // Process all Samples in Block 
                        for (unsigned int k = 0; k < sample_blocks[j].samples.size(); ++k) { 
                            // Resample to original carrier frequency
                            resamp_crcf_execute(    resamplers[i], 
                                                    sample_blocks[j].samples[k],
                                                    &rx_sample[0], &num_written);  
                                
                            // Execute Synchronizer for channel i 
                            ms.Execute(i, &rx_sample);          
                                
                            // Check, if callback-data was updated by synchronizer
                            if (cb_data[i].buffer.size()){     
                                // Push CFR to queue                    
                                ms.GetCfr(i, &cfr.cfr);                                                 // Write cfr to callback data
                                cfr.timestamp = sample_blocks[j].timestamp;                             // Update timestamp
                                cfr.channel = i;                                                        // Set channel index
                                {
                                    std::lock_guard<std::mutex> lock_cfr(cfr_queue.mtx);
                                    cfr_queue.queue.push(std::move(cfr));
                                }
                                cfr_queue.cv.notify_one();

                                // Push Callback-data to queue
                                cb_data[i].timestamp = sample_blocks[j].timestamp;                      // Update timestamp
                                cb_data[i].channel = i;                                                 // Set channel index
                                {
                                    std::lock_guard<std::mutex> lock_cb(cbdata_queue.mtx);
                                    cbdata_queue.queue.push(std::move(cb_data[i]));
                                }
                                cbdata_queue.cv.notify_one();

                                // Print debug info 
                                std::cout << "Captured CFR for channel "<< i <<" at timestamp "<< cfr.timestamp.get_full_secs() << std::endl;
                                break;
                            };
                        };
                    };
                };


        // Synchronize NCOs of all channels to the average NCO frequency and phase
        ms.SynchronizeNcos();
    }
}

// Export-Worker processes queued CFRs from Sync-workers and either discards them or exports them to a ZMQ socket
template <std::size_t num_channels>
void cfr_export_worker( CfrQueue_t& cfr_queue, 
                    uhd::time_spec_t max_age,
                    ZmqSender& sender,
                    MatlabExport& m_file,
                    std::atomic<bool>& stop_signal_called) {

    // Queued CFRs of all channels and all times 
    std::vector<Cfr_t> cfr_buffer;         

    // Sorted and time-matched CFRs of all channels
    std::vector<std::vector<std::complex<float>>> cfr_group(num_channels);

    unsigned int i;
    while (!stop_signal_called.load()) {
        // Move cfr queue to buffer
        std::unique_lock<std::mutex> lock_cfr(cfr_queue.mtx);
        cfr_queue.cv.wait(lock_cfr, [&cfr_queue, &stop_signal_called] { 
            return !cfr_queue.queue.empty() || stop_signal_called.load(); 
        });

        if (!cfr_queue.queue.empty()) {
            cfr_buffer.push_back(std::move(cfr_queue.queue.front()));
            cfr_queue.queue.pop();
        }

        // Sort CFRs by timestamp
        std::sort(cfr_buffer.begin(), cfr_buffer.end(),
            [](const Cfr_t& a, const Cfr_t& b) {
                return a.timestamp < b.timestamp;
            });

        // Find a group of CFRs from all channels within the max_age window
        unsigned int i;
        for (i = 0; i < cfr_buffer.size(); ++i) {
            std::vector<const Cfr_t*> group(num_channels, nullptr); // Group of CFRs from each channel
            const auto& base = cfr_buffer[i];                       // Add initial CFR to group
            group[base.channel] = &base;

            // Find all CFRs around base within the max_age window
            for (size_t j = i + 1; j < cfr_buffer.size(); ++j) {
                // Next timestamp out of range -> no group existing for this base-CFR
                if ((cfr_buffer[j].timestamp - base.timestamp) > max_age)
                    break;
                // Found CFR for another channel within the max_age window
                if (!group[cfr_buffer[j].channel])
                    group[cfr_buffer[j].channel] = &cfr_buffer[j];
            }

            // Check if group is complete (all channels have a CFR)
            bool complete = true;
            for (auto ptr : group) {
                if (!ptr) { complete = false; break; }
            }

            // Export if a complete group was found
            if (complete) {
                // Reset the CFR group
                cfr_group = std::vector<std::vector<std::complex<float>>>(num_channels);
                // Prepare CFRs sorted by channel
                for (size_t ch = 0; ch < num_channels; ++ch)
                    cfr_group[ch] = group[ch]->cfr;

                //----------------- ZMQ Export ----------------------
                sender.send(cfr_group);
                std::cout << "Exported CFR at timestamps ";
                for (const auto& cfr : group) {
                    std::cout << cfr[0].timestamp.get_full_secs() << " ";
                }
                std::cout <<"!"<< std::endl;

                // Clear the buffer up to the current index
                cfr_buffer.erase(cfr_buffer.begin(), cfr_buffer.end());


                //----------------- MATLAB Export ----------------------
                // Export CFRs to MATLAB file
                for (unsigned int i = 0; i < num_channels; ++i) {
                    std::string suffix = std::to_string(i);
                    m_file.Add(cfr_group[i], "cfr_" + suffix);
                }

                // Add combined plot-commands to the MATLAB file
                std::stringstream matlab_cmd;

                matlab_cmd << "figure;";
                // CFR Magnitude
                matlab_cmd << "subplot(2,1,1); hold on;";
                for (unsigned int i = 0; i < num_channels; ++i) {
                    std::string suffix = std::to_string(i);
                    matlab_cmd << "plot(abs(cfr_" << suffix << "), 'DisplayName', 'RX-Channel " << suffix << "');";
                }
                matlab_cmd << "title('Channel Frequency Response Gain'); legend; grid on;";
                matlab_cmd << std::endl;

                // CFR Phase
                matlab_cmd << "subplot(2,1,2); hold on;";
                for (unsigned int i = 0; i < num_channels; ++i) {
                    std::string suffix = std::to_string(i);
                    matlab_cmd << "plot(angle(cfr_" << suffix << "), 'DisplayName', 'RX-Channel " << suffix << "');";
                }
                matlab_cmd << "title('Channel Frequency Response Phase'); legend; grid on;";
                matlab_cmd << std::endl;

                // CFR in Complex 
                matlab_cmd << "figure;";
                for (unsigned int i = 0; i < num_channels; ++i) {
                    std::string suffix = std::to_string(i);
                    matlab_cmd << "plot(real(cfr_" << suffix << "), imag(cfr_" << suffix
                            << "), '.', 'DisplayName', 'RX-Channel " << suffix << "'); hold on;";
                }
                matlab_cmd << "title('CFR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
                matlab_cmd << std::endl;

                // Add the complete command string to MATLAB export
                m_file.Add(matlab_cmd.str());

                // Break queue processing 
                break;
            }
        }

        // Clear all processed CFRs from the buffer, keep min. one CFR for each channel
        if (cfr_buffer.size() > num_channels){
            int rest = static_cast<int>(cfr_buffer.size()) - static_cast<int>(i) - 1;
            cfr_buffer.erase(cfr_buffer.begin(), cfr_buffer.end() - (rest>static_cast<int>(num_channels) ? rest : static_cast<int>(num_channels)));
        };
    }

}

// Export-Worker processes queued CB-Data from a Sync-worker and exports them to a MATLAB file
void cbdata_export_worker(  CbDataQueue_t& cbdata_queue, 
                            MatlabExport& m_file,
                            std::atomic<bool>& stop_signal_called) {

    // Queued cb-data 
    std::vector<CallbackData_t> cbdata_buffer; 
    std::vector<std::string> cbdata_varnames;       

    unsigned int i;
    while (!stop_signal_called.load()) {
        // Move CB-Data queue to buffer
        std::unique_lock<std::mutex> lock_cb(cbdata_queue.mtx);
        cbdata_queue.cv.wait(lock_cb, [&cbdata_queue, &stop_signal_called] { 
            return !cbdata_queue.queue.empty() || stop_signal_called.load(); 
        });

        if (stop_signal_called.load()) break;

        if (!cbdata_queue.queue.empty()) {
            cbdata_buffer.push_back(std::move(cbdata_queue.queue.front()));
            cbdata_queue.queue.pop();
        }

        // Export CB-Data buffer to MATLAB file
        for (unsigned int i = 0; i < cbdata_buffer.size(); ++i) {
            std::string suffix = "CH" + std::to_string(cbdata_buffer[i].channel)+"_"+ std::to_string(cbdata_buffer[i].timestamp.get_tick_count(1000));
            m_file.Add(cbdata_buffer[i].buffer, suffix);
            cbdata_varnames.push_back(suffix);
        }

        // Clear the buffer
        cbdata_buffer.clear();
    }

    // Add combined plot-command to the MATLAB file
    std::stringstream matlab_cmd;

    // CFR in Complex 
    matlab_cmd << "figure;";
    for (auto& varname : cbdata_varnames    ) {
        matlab_cmd << "plot(real("<< varname <<"), imag("<< varname
                <<"), '.', 'DisplayName', '"<< varname <<"'); hold on;";
    }
    matlab_cmd << "title('Received Data'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
    matlab_cmd << std::endl;

    // Add the complete command string to MATLAB export
    m_file.Add(matlab_cmd.str());
}


#endif // MULTIRX_H