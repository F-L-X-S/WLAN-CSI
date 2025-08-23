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

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>

#include <multisync/multisync.h>
#include <zmq_socket/zmq_socket.h>
#include <matlab_export/matlab_export.h>

// Typedefs
using Sample_t = std::complex<float>;   // Received samples type

struct RxSampleBlock_t
{
    std::vector<Sample_t> samples;                    // Received samples
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

struct Phase_t {
    float phi;               // Phase data
    unsigned int channel;    // Channel index
};

struct PhaseQueue_t
{
    std::queue<Phase_t> queue;
    std::mutex mtx;
    std::condition_variable cv;
};

/**
 * @brief The stream_worker function references the UHD USRP interfaces, configuring all available instances simultaneously, 
 * with one designated as the master device for the MIMO configuration. Sample streaming to or from the USRP devices is accomplished 
 * by issuing stream commands. To prevent synchronization issues arising from variable program execution times, 
 * timed stream commands based on the device time are constructed and dispatched to the USRPs. 
 * For stable timing synchronization, the stream command that initiates continuous streaming is constructed and re-issued 
 * after a defined cycle time by the stream_worker function.
 * 
 * This function is executed within a separated thread.
 * 
 * @tparam num_channels 
 * @param usrps Reference to array with UHD USRP interfaces
 * @param max_samps Max. number of samples to receive per frame from the USRP
 * @param tx_rate TX Sample Rate [Sps]
 * @param rx_rate RX Sample Rate [Sps]
 * @param center_freq Center Frequency [Hz]
 * @param cycle_time Execution cycle time (Stream restart after 10 cycles) [ms]
 * @param stop_signal_called Stop signal to terminate the thread
 */
template <std::size_t num_channels>
void stream_worker( std::array<uhd::usrp::multi_usrp::sptr, num_channels>& usrps,
                    size_t& max_samps, 
                    double& tx_rate,
                    double& rx_rate,
                    double& center_freq,
                    unsigned int cycle_time,
                    std::atomic<bool>& stop_signal_called) {


    // Lock mboard clocks
    unsigned int i;
    for (i=0; i < num_channels; ++i){
        std::string clk_src = i==0 ? "internal":"mimo";
        usrps[i]->set_clock_source(clk_src, 0);                         // internal clock source for device 0 / mimo for other devices 
        if (i>0) usrps[i]->set_time_source(clk_src, 0);                 // mimo time source for devices > 0 
        if (i==0) usrps[i]->set_time_now(uhd::time_spec_t(0.0), 0);     // initialize device time
    };

    //sleep a while the slaves lock its time to the master
    boost::this_thread::sleep(boost::posix_time::milliseconds(cycle_time));

    // Configure tune request for desired center frequency 
    uhd::tune_request_t tune_request(center_freq); 
    tune_request.rf_freq_policy = uhd::tune_request_t::policy_t::POLICY_AUTO;
    std::cout << boost::format("Tune Policy: %f") % (tune_request.rf_freq_policy) << std::endl;

    for (auto usrp : usrps){
        //always select the subdevice first, the channel mapping affects the other settings
        usrp->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);           //set the device 0 to use the A RX frontend (RX channel 0)
        usrp->set_tx_subdev_spec(uhd::usrp::subdev_spec_t("A:0"), 0);           //set the device 0 to use the A TX frontend (TX channel 0)
        usrp->set_rx_rate(rx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);       // set RX sample rate
        usrp->set_tx_rate(tx_rate, uhd::usrp::multi_usrp::ALL_MBOARDS);       // set TX sample rate
        usrp->set_rx_freq(tune_request, 0);                                     // set RX Frequency  
        usrp->set_tx_freq(tune_request, 0);                                     // set TX Frequency  
        usrp->set_rx_gain(30, 0);                                               // set the RX gain
        usrp->set_tx_gain(15, 0);                                               // set the TX gain
        usrp->set_rx_antenna("RX2", 0);                                         // set the RX antenna
        usrp->set_tx_antenna("TX/RX", 0);                                       // set the TX antenna
    }

    //sleep a while...
    boost::this_thread::sleep(boost::posix_time::milliseconds(cycle_time));

    // Print device configuration 
    for (i=0; i < num_channels; ++i){
        std::cout << boost::format("---- Configuration  Device %1% ----") % i << std::endl;
        std::cout << boost::format("Clock-src: %s") % usrps[i]->get_clock_source(0) << std::endl;
        std::cout << boost::format("Time-src: %s") % usrps[i]->get_time_source(0) << std::endl;

        std::cout << boost::format("TX Configuration:") << std::endl;
        std::cout << boost::format("\tRequired TX Rate: %f Msps...") % (tx_rate / 1e6) << std::endl;
        std::cout << boost::format("\tTX Rate: %f Msps...") % (usrps[i]->get_tx_rate(0) / 1e6) << std::endl;
        std::cout << boost::format("\tRequired TX Freq: %f MHz...") % (center_freq / 1e6) << std::endl;
        std::cout << boost::format("\tTX Freq: %f MHz...") % (usrps[i]->get_tx_freq(0) / 1e6) << std::endl;
        std::cout << boost::format("\tTX Gain: %f dB...") % usrps[i]->get_tx_gain(0) << std::endl;
        std::cout << boost::format("\tTX Bandwidth: %f MHz...") % (usrps[i]->get_tx_bandwidth(0) / 1e6) << std::endl;

        std::cout << boost::format("RX Configuration:") << std::endl;
        std::cout << boost::format("\tRequired RX Rate: %f Msps...") % (rx_rate / 1e6) << std::endl;
        std::cout << boost::format("\tRX Rate: %f Msps...") % (usrps[i]->get_rx_rate(0) / 1e6) << std::endl;
        std::cout << boost::format("\tRequired RX Freq: %f MHz...") % (center_freq / 1e6) << std::endl;
        std::cout << boost::format("\tRX Freq: %f MHz...") % (usrps[i]->get_rx_freq(0) / 1e6) << std::endl;
        std::cout << boost::format("\tRX Gain: %f dB...") % usrps[i]->get_rx_gain(0) << std::endl;
        std::cout << boost::format("\tRX Bandwidth: %f MHz...") % (usrps[i]->get_rx_bandwidth(0) / 1e6) << std::endl << std::endl;
    };

    // Configure stream command
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.num_samps = max_samps; // number of samples to receive per frame
    stream_cmd.stream_now = false;  
    double seconds_in_future = 0.1; 

    // Cyclic burst stream
    while (!stop_signal_called.load()) {
            for (auto usrp : usrps){
            // Set the time spec to start receiving in the future
            stream_cmd.time_spec = usrp->get_time_now() + uhd::time_spec_t(seconds_in_future);  
            // Start USRPs streaming
            usrp->issue_stream_cmd(stream_cmd); 
            };

            // receive for cycle time 
            boost::this_thread::sleep(boost::posix_time::milliseconds(10*cycle_time));

            // Stop streaming 
            for (auto usrp : usrps){
                usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
            }

            // stop for cycle time -> synchronization
            boost::this_thread::sleep(boost::posix_time::milliseconds((unsigned int) (0.1*cycle_time)));

    };

    // Stop streaming 
    for (auto usrp : usrps){
        usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    }

}

/**
 * @brief tx_worker reads the referenced buffer at the specified cycle time and forwards the data 
 * to the streamer for transmission. The buffer contains the baseband time‑domain sequence to be transmitted 
 * in the next cycle.
 * 
 * This function is executed within a dedicated thread for each uhd::tx\_streamer::sptr instance.
 * 
 * @param tx_stream Reference to the TX streamer interface
 * @param buff Baseband time‑domain sequence to be transmitted
 * @param cycle_time Cycle time to read and transmit the buffer [ms]
 * @param stop_signal_called Stop signal to terminate the thread
 */
void tx_worker(uhd::tx_streamer::sptr tx_stream,
                std::vector<Sample_t>& buff,
                unsigned int cycle_time,
                std::atomic<bool>& stop_signal_called){

    uhd::tx_metadata_t md;
    std::vector<std::complex<float>*> buffs(1, &buff.front());
    size_t samples_sent=0;
    while (!stop_signal_called.load()) {
        const size_t n_tx = tx_stream->send(buffs, buff.size(), md);
        samples_sent+=n_tx;
        md.start_of_burst = false;
        md.has_time_spec  = false;

        //sleep after transmitting buffer 
        if (samples_sent>=buff.size()){
            samples_sent = 0;
            boost::this_thread::sleep(boost::posix_time::milliseconds(cycle_time));
        };
    }
}


/**
 * @brief rx_worker reads the complex samples from the referenced uhd::rx\_streamer::sptr instance and 
 * forwards each cycle's batch of samples as a block to a dedicated queue. To enable precise frame timing identification, 
 * the rx_worker attaches the USRP device time to the corresponding sample block. This timestamp is supplied by the 
 * streamer instance as metadata for each packet received from the USRP. The performance of this function is critical, 
 * as insufficient processing efficiency may result in overflow errors of the UHD driver.
 * 
 * This function is executed within a dedicated thread for each uhd::rx\_streamer::sptr instance.
 * 
 * @tparam buffer_size Max. number of samples to receive per frame from the USRP
 * @param rx_stream Reference to the RX streamer interface
 * @param q Reference to the thread-safe queue to push the received sample blocks
 * @param stop_signal_called Stop signal to terminate the thread
 */
template <std::size_t buffer_size>
void rx_worker( uhd::rx_streamer::sptr rx_stream,
                RxSamplesQueue_t& q,
                std::atomic<bool>& stop_signal_called) {
    uhd::rx_metadata_t md;
    std::vector<Sample_t> buff(buffer_size);

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

/**
 * @brief The sync_worker function continuously retrieves timestamped sample-blocks from the channel-specific queues and 
 * executes the synchronization algorithm on these samples through the MultiSync instance. 
 * When a frame is detected by a channel’s synchronizer, the resulting CFR and associated callback data are pushed into 
 * thread-safe queues for use in subsequent processing stages. 
 * The queued data is tagged with the timestamp and the channel number of the synchronized sample-block, in which the frame was detected.
 * 
 * Throughout the synchronization process, phase corrections can be applied to the NCOs of the MultiSync instance to compensate 
 * the phase errors introduced by the hardware instances.
 * 
 * This function is executed within a dedicated thread.
 * 
 * @tparam num_channels Number of Channels to synchronize
 * @tparam syncronizer_type Type of the synchronizer to use (e.g. ofdmframesync)
 * @tparam cb_data_type Callback-data type to use 
 * @param ms Reference to the MultiSync instance
 * @param cb_data Reference to the array of callback-data structures (one for each channel)
 * @param rx_queues Reference to the array of thread-safe queues storing received sample blocks (one for each channel)
 * @param cfr_queue Reference to the thread-safe queue to push the detected CFRs
 * @param cbdata_queue Reference to the thread-safe queue to push the Callback-data
 * @param phi_error_queue Reference to the thread-safe queue to receive phase corrections for the NCOs
 * @param stop_signal_called Stop signal to terminate the thread
 */
template <std::size_t num_channels, typename syncronizer_type, typename cb_data_type>
void sync_worker(   syncronizer_type& ms,
                    std::array<CallbackData_t, num_channels>& cb_data,
                    std::array<RxSamplesQueue_t, num_channels>& rx_queues,
                    CfrQueue_t& cfr_queue,
                    CbDataQueue_t& cbdata_queue,
                    PhaseQueue_t& phi_error_queue,
                    std::atomic<bool>& stop_signal_called
                ) {

    std::vector<RxSampleBlock_t> sample_blocks;
    std::vector<std::complex<float>> rx_sample(1);
    Cfr_t cfr;                                                        
    unsigned int i, j, num_written;

    while (!stop_signal_called.load()) {
        // Process Phase Error queue to adjust NCO phase for the channel
        std::unique_lock<std::mutex> lock_phi(phi_error_queue.mtx);
        phi_error_queue.cv.wait_for(lock_phi, std::chrono::milliseconds(100), [&]() {
            return !phi_error_queue.queue.empty() || stop_signal_called.load();
        });

        while (!phi_error_queue.queue.empty()) {
            Phase_t phi_error = std::move(phi_error_queue.queue.front());
            std::cout<< "Adjusted NCO of CH"<< phi_error.channel<<" from"<< ms.GetNcoPhase(phi_error.channel)<<" rad";
            ms.AdjustNcoPhase(phi_error.channel, phi_error.phi);  // Adjust NCO phase for the channel
            std::cout<< "to "<< ms.GetNcoPhase(phi_error.channel)<<" rad!"<<std::endl;
            phi_error_queue.queue.pop();
        }
        lock_phi.unlock();

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

                            // Execute Synchronizer for channel i 
                            rx_sample[0]= sample_blocks[j].samples[k];
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
                            };
                        };
                    };
                };
    }
}

/**
 * @brief The cfr_export_worker first identifies queued CFRs whose timestamps all fall within a specified time range 
 * and whose channel numbers are all unique within that group. 
 * Grouping minimizes the number of forwarded CFRs that correspond to frames not detected across all channels. 
 * Once a complete group is identified, the function exports the group simultaneously through the referenced ZmqSender and MatlabExport instances. 
 * The function includes buffer management to maintain efficiency by removing processed CFRss without valid groups while retaining a minimum number of entries per channel. 
 * At termination, it adds plotting commands to the MatlabExport instance to visualize magnitude, phase, and complex representation of the exported CFRs. 
 * 
 * This function is executed within a dedicated thread.
 * 
 * @tparam num_channels Number of Channels CFRs belong to
 * @param cfr_queue Reference to the thread-safe queue storing the detected CFRs
 * @param max_age Time-range within which the timestamps of a CFRs corresponding to one frame must fall
 * @param sender Reference to the ZmqSender instance for exporting the CFR groups
 * @param m_file Reference to the MatlabExport instance for exporting the CFR groups
 * @param stop_signal_called Stop signal to terminate the thread
 */
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
        unsigned int i, j;
        for (i = 0; i < cfr_buffer.size(); ++i) {
            std::vector<const Cfr_t*> group(num_channels, nullptr); // Group of CFRs from each channel
            const auto& base = cfr_buffer[i];                       // Add initial CFR to group
            group[base.channel] = &base;

            // Find all CFRs around base within the max_age window
            for (j = i + 1; j < cfr_buffer.size(); ++j) {
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
                for (const auto& cfr : group) {
                    cfr_group[cfr->channel] = cfr->cfr;;
                }

                // ZMQ Export
                sender.send(cfr_group);

                // MATLAB Export 
                std::cout << "Exported CFR at timestamps ";
                for (const auto& cfr : group) {
                    std::string timestamp = std::to_string(cfr->timestamp.get_full_secs())+std::to_string(cfr->timestamp.get_tick_count(1000));
                    std::cout << "CH"<<cfr->channel<<": "<<timestamp << " ";
                    m_file.Add(cfr->cfr, "CH" + std::to_string(cfr->channel) +"_"+timestamp);
                }
                std::cout <<"!"<< std::endl;

                // Clear the buffer up to the current index
                cfr_buffer.erase(cfr_buffer.begin(), cfr_buffer.begin()+j);

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

    // Add combined plot-commands to the MATLAB file
    for (auto& varname : m_file.GetVarNames()) {
        std::stringstream matlab_cmd;
        matlab_cmd << "figure;";

        // CFR Magnitude
        matlab_cmd << "subplot(2,1,1); hold on;";
        matlab_cmd << "plot(abs("<< varname <<"), 'DisplayName', '" << varname << "');";
        matlab_cmd << "title('Channel Frequency Response Gain'); legend; grid on;";
        matlab_cmd << std::endl;

        // CFR Phase
        matlab_cmd << "subplot(2,1,2); hold on;";
        matlab_cmd << "plot(angle("<< varname <<"), 'DisplayName', '" << varname << "');";
        matlab_cmd << "title('Channel Frequency Response Phase'); legend; grid on;";
        matlab_cmd << std::endl;

        // CFR in Complex 
        matlab_cmd << "figure;";
        matlab_cmd << "plot(real("<< varname <<"), imag("<< varname
                <<"), '.', 'DisplayName', '"<< varname <<"'); hold on;";
        matlab_cmd << "title('CFR'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
        matlab_cmd << std::endl;

        // Add the complete command string to MATLAB export
        m_file.Add(matlab_cmd.str());
    }

}

/**
 * @brief cbdata_export_worker is responsible for forwarding the queued callback-data items to the associated MatlabExport instance.
 * Upon termination, the worker additionally generates the Matlab plotting commands required to visualize the constellation diagrams 
 * of all received frames and appends them to the MatlabExport instance.
 * 
 * This function is executed within a dedicated thread.
 * 
 * @param cbdata_queue Reference to thread-safe queue storing the callback-data items
 * @param m_file Reference to the MatlabExport instance for exporting the callback-data items
 * @param stop_signal_called Stop signal to terminate the thread
 */
void cbdata_export_worker(  CbDataQueue_t& cbdata_queue, 
                            MatlabExport& m_file,
                            std::atomic<bool>& stop_signal_called) {

    // Queued cb-data 
    std::vector<CallbackData_t> cbdata_buffer;    

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
            std::string timestamp = std::to_string(cbdata_buffer[i].timestamp.get_full_secs())+std::to_string(cbdata_buffer[i].timestamp.get_tick_count(1000));
            std::string suffix = "CH" + std::to_string(cbdata_buffer[i].channel)+"_"+ timestamp;
            m_file.Add(cbdata_buffer[i].buffer, suffix);
        }

        // Clear the buffer
        cbdata_buffer.clear();
    }

    // Add combined plot-command to the MATLAB file
    std::stringstream matlab_cmd;

    // CFR in Complex 
    matlab_cmd << "figure;";
    for (auto& varname : m_file.GetVarNames()) {
        matlab_cmd << "plot(real("<< varname <<"), imag("<< varname
                <<"), '.', 'DisplayName', '"<< varname <<"'); hold on;";
        matlab_cmd << std::endl;
    }
    matlab_cmd << "title('Received Data'); xlabel('Real'); ylabel('Imag'); axis equal; legend; grid on;";
    matlab_cmd << std::endl;

    // Add the complete command string to MATLAB export
    m_file.Add(matlab_cmd.str());
}

/**
 * @brief terminal_worker reads terminal inputs. 
 * Possible commands:
 * adjust_phase <channel> <phase in rad> : push phase correction for the specified channel to the phi_error_queue 
 * exit : terminate program
 * q : terminate program
 * quit : terminate program
 * 
 * @param phi_error_queue Reference to the thread-safe queue to send phase corrections for channel synchronizers
 * @param stop_signal_called Stop signal to terminate the program
 */
void terminal_worker(    PhaseQueue_t& phi_error_queue,
                        std::atomic<bool>& stop_signal_called) {
    
    while (!stop_signal_called.load()) {
        // Read User Input 
        std::string input;
        std::getline(std::cin, input);

        // Parse single token commands 
        if (input == "exit" || input == "quit" || input == "q") {
            stop_signal_called.store(true);
            break;
        }

        // Parse multi token commands 
        std::vector<std::string> tokens;
        boost::split(tokens, input, boost::is_any_of(" "));

        // Adjust NCO phase for a specific channel
        if (tokens.size() == 3 && tokens[0] == "adjust_phase") {
            // Set NCO phase for a specific channel
            unsigned int channel_id = std::stoi(tokens[1]);
                Phase_t phase_data;
                phase_data.channel = channel_id;
                phase_data.phi = static_cast<float>(std::stod(tokens[2]));
                {
                    std::lock_guard<std::mutex> lock_phi(phi_error_queue.mtx);
                    phi_error_queue.queue.push(std::move(phase_data));
                }
                phi_error_queue.cv.notify_one();
            
        } else {
            std::cerr << "Unknown command: " << input << std::endl;
        }

    }
}


#endif // MULTIRX_H