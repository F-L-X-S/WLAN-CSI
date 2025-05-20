/**
 * @file multisync.h
 * @author Felix Schuelke (flxscode@gmail.com)
 * 
 * @brief This file contains the definition of the MultiSync class, which is a generic multi-synchronizer class to handle multiple receiving channels.
 * The class provides C++-interfaces for different types of synchronizers, such as OFDM frame synchronizers.
 * The Liquid-DSP-interface of the specified Synchronizer is defined in the SyncTraits struct.
 * Liquid's DSP-modules are based on https://github.com/jgaeddert/liquid-dsp (Copyright (c) 2007 - 2016 Joseph Gaeddert).
 * 
 * @version 0.1
 * @date 2025-05-20
 * 
 * 
 */

#ifndef MULTISYNC_H
#define MULTISYNC_H

#include <complex>
#include <cassert>
#include <liquid.h>

template<typename T>
struct SyncTraits {
    static_assert(sizeof(T) == 0, "Liquid-Functions not defined for this synchronizer type");
};


/**
 * @brief Define the Liquid functions for the OFDM frame synchronizer
 * 
 */
template<>
struct SyncTraits<ofdmframesync> {

    using CallbackType = ofdmframesync_callback;

    /**
     * @brief Define the Parameters for the OFDM frame synchronizer Create function
     * 
     */
    struct CreateParams {
        unsigned int M;           // number of subcarriers
        unsigned int cp_len;      // cyclic prefix length
        unsigned int taper_len;   // taper length
        unsigned char * p;      // modulation scheme
    };

    static ofdmframesync Create(const CreateParams& params, ofdmframesync_callback callback, void* userdata) 
    {
        return ofdmframesync_create(params.M, params.cp_len, params.taper_len, params.p, callback, userdata);
    }

    static void Reset(ofdmframesync_s* fs) 
    {
        ofdmframesync_reset(fs);
    }

    static int Execute(ofdmframesync_s* fs, std::complex<float>* x, unsigned int n) 
    {
        return ofdmframesync_execute(fs, x, n);
    }

    static void Destroy(ofdmframesync_s* fs) 
    {
        ofdmframesync_destroy(fs);
    }

    static void GetCfr(ofdmframesync_s* fs, std::vector<std::complex<float>>* X, unsigned int fft_size) 
    {
        ofdmframesync_get_cfr(fs, X->data(), fft_size);
    }

    static nco_crcf* GetNco(ofdmframesync_s* fs){
        return ofdmframesync_get_nco(fs);
    }
};

/**
 * @brief Generic multi-synchronizer class to handle multiple Receiving channels e.g. in an antenna array
 * 
 * @tparam synchronizer_type 
 */
template<typename synchronizer_type>          
class MultiSync {
public:

    /**
     * @brief Generic type-definitions,depending on the synchronizer type
     * 
     */
    using ParamsType   = typename SyncTraits<synchronizer_type>::CreateParams;
    using CallbackType = typename SyncTraits<synchronizer_type>::CallbackType;


    /**
     * @brief Construct a new MultiSync object by using a synchronizer-structure
     * 
     * @param _num_channels number of channels
     * @param _synchronizer_params synchronizer parameters
     * @param _userdata user-defined data structure array
     * @param _callback user-defined callback function
     */
MultiSync(  unsigned int             num_channels,
            const ParamsType&        synchronizer_params, 
            CallbackType             callback,
            void **                  userdata):
            num_channels_(num_channels), userdata_(userdata), callback_(callback)
            {
                // create frame synchronizers
                framesync_ = new synchronizer_type[num_channels_];
                for (unsigned int i=0; i<num_channels; i++) {
                    userdata_[i]  = userdata[i];
                    framesync_[i] = SyncTraits<synchronizer_type>::Create(synchronizer_params, callback_, userdata_[i]);
                }
            }

    /**
     * @brief Destroy the MultiSync object
     * 
     */
    ~MultiSync()
            {
                for (unsigned int i = 0; i < num_channels_; i++)
                    SyncTraits<synchronizer_type>::Destroy(framesync_[i]);
                delete[] framesync_;
            }

    /**
     * @brief Reset the MultiSync object and all associated synchronizers
     * 
     */
    void Reset()
            {
                for (unsigned int i = 0; i < num_channels_; ++i) 
                    SyncTraits<synchronizer_type>::Reset(framesync_[i]);
            };

    /**
     * @brief Push samples into the channel's synchronizer object
     * @param channel_id identifier of the channel
     * @param x pointer to the input samples
     * @param num_samples number of input samples to process
     */
    void Execute(unsigned int           channel_id,
                std::vector<std::complex<float>>*   x)
                {
                    SyncTraits<synchronizer_type>::Execute(framesync_[channel_id], x->data(), x->size());
                };
    
    /**
     * @brief  Synchronize NCOs of all channels to the average NCO frequency and phase
     * 
     */
    void SynchronizeNcos()
                {
                    // Initialize 
                    nco_freq_ = 0.0f;
                    nco_phase_ = 0.0f;

                    // Synchronize NCOs
                    for (unsigned int i = 0; i < num_channels_; ++i) {
                        nco_crcf* nco = SyncTraits<synchronizer_type>::GetNco(framesync_[i]);
                        nco_freq_ += nco_crcf_get_frequency(*nco);
                        nco_phase_ += nco_crcf_get_phase(*nco);
                    }

                    // Average NCO frequency and phase
                    nco_freq_ /= num_channels_;
                    nco_phase_ /= num_channels_;

                    // Set NCO frequency and phase for all channels
                    for (unsigned int i = 0; i < num_channels_; ++i) {
                        nco_crcf* nco = SyncTraits<synchronizer_type>::GetNco(framesync_[i]);
                        nco_crcf_set_frequency(*nco, nco_freq_);
                        nco_crcf_set_phase(*nco, nco_phase_);
                    }
                };

    /**
     * @brief Synchronize NCOs of all channels to the master channel
     * 
     * @param channel_id Master channel id
     */
    void SynchronizeNcos(unsigned int channel_id)
                {
                    // Initialize by master-channel
                    nco_crcf* nco = SyncTraits<synchronizer_type>::GetNco(framesync_[channel_id]);
                    nco_freq_ = nco_crcf_get_frequency(*nco);
                    nco_phase_ = nco_crcf_get_phase(*nco);

                    // Set NCO frequency and phase for all channels
                    for (unsigned int i = 0; i < num_channels_; ++i) {
                        // skip master channel
                        if (i == channel_id) continue; 
                        // set NCO frequency and phase
                        nco = SyncTraits<synchronizer_type>::GetNco(framesync_[i]);
                        nco_crcf_set_frequency(*nco, nco_freq_);
                        nco_crcf_set_phase(*nco, nco_phase_);
                    }
                };

    /**
     * @brief Get the Channel frequency response (CFR) of the specified channel
     * 
     * @param channel_id Id of the channel 
     * @param X Array to store the spectrum of the CFR
     * @param n Arraysize 
     */
    void GetCfr(unsigned int                         channel_id, 
                std::vector<std::complex<float>>*    X,
                unsigned int                         fft_size)
                {
                     X->resize(fft_size);
                    SyncTraits<synchronizer_type>::GetCfr(framesync_[channel_id], X, fft_size);
                };


private:
    /**
     * @brief number of receiving channels 
     * 
     */
    unsigned int num_channels_;      

    /**
     * @brief array of pointers to synchronizer
     * 
     */
    synchronizer_type* framesync_; 

    /**
     * @brief Synchronizer parameters
     * 
     */
    ParamsType params_;

    /**
     * @brief Array of pointers to userdata
     * 
     */
    void ** userdata_;               

    /**
     * @brief Pointer to callback function
     * 
     */
    CallbackType callback_;  

    /**
     * @brief Synchronized NCO frequency
     * 
     */
    float nco_freq_;

    /**
     * @brief Synchronized NCO phase
     * 
     */
    float nco_phase_; 
    
};

#endif // MULTISYNC_H