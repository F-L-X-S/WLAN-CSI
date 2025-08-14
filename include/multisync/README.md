# Multisync
### Multichannel Detection and Synchronization
The MultiSync class (defined in ./include/multisync/multisync.h) is an abstract C++ class-template designed to enable the simultaneous initialization and execution of multiple instances of generic frame synchronizers. It is templated on the synchronizer type and relies on a SyncTraits structure to bind the Liquid-DSP C API to a C++ interface. Consequently, the SyncTraits structure is re-defined for each Liquid-DSP synchronizer type intended for use as a MultiSync template parameter. ```struct SyncTraits<ofdmframesync>``` illustrates the definition of the SyncTraits structure for the ```ofdmframeync``` synchronizer type. Once the SyncTraits structure has been specialized for a given synchronizer type, MultiSync can be instantiated with this type as its template parameter, thereby providing an abstracted C++ API for streamlined multi-channel synchronization, which is illustrated in Fig. \ref{fig:multisync_example}. Additional details on defining the callback function and configuring the synchronizer parameters ```{M, cp_len, taper_len, p}``` are available at the Liquid-DSP API documentation.<br>
To ensure phase-aligned operation across channels, MultiSync compensates for the hardware-induced phase errors. Incoming sample blocks are first processed by the NCO to apply externally estimated phase or frequency corrections before being passed to the corresponding synchronizer for frame detection. 

#### Definition of the SyncTraits structure to enable the usage of MutliSync for frame-synchronization with ofdmframesync
```cpp
template<>
struct SyncTraits<ofdmframesync> {

    // Define the type of the callback function used in the OFDM frame synchronizer
    using CallbackType = ofdmframesync_callback;

    // Definition of the Parameters for the OFDM frame synchronizer Create function
    struct CreateParams {
        unsigned int M;           // number of subcarriers
        unsigned int cp_len;      // cyclic prefix length
        unsigned int taper_len;   // taper length
        unsigned char * p;        // modulation scheme
    };

    // Wrapper function to create an OFDM frame synchronizer
    static ofdmframesync Create(const CreateParams& params, ofdmframesync_callback callback, void* userdata) 
    {
        return ofdmframesync_create(params.M, params.cp_len, params.taper_len, params.p, callback, userdata);
    }

    // Wrapper function to reset an OFDM frame synchronizer
    static void Reset(ofdmframesync_s* fs) 
    {
        ofdmframesync_reset(fs);
    }

    // Wrapper function to execute an OFDM frame synchronizer
    static int Execute(ofdmframesync_s* fs, std::complex<float>* x, unsigned int n) 
    {
        return ofdmframesync_execute(fs, x, n);
    }

    // Wrapper function to destroy an OFDM frame synchronizer
    static void Destroy(ofdmframesync_s* fs) 
    {
        ofdmframesync_destroy(fs);
    }

    // Wrapper function to get CFR of the last frame received by the OFDM frame synchronizer
    static void GetCfr(ofdmframesync_s* fs, std::vector<std::complex<float>>* X) 
    {
        unsigned int fft_size = ofdmframesync_get_fft_size(fs);
        X->resize(fft_size);
        ofdmframesync_get_cfr(fs, X->data(), fft_size);
    }
};
```

#### Processing a vector of complex samples by using MutliSync with ofdmframesync as synchronizer type
```cpp
    // Received samples
    std::vector<std::complex<float>> samples;
    
    // Callback data
    std::array<CallbackData_t, NUM_CHANNELS> cb_data;
    void* userdata[NUM_CHANNELS];

    // Array of Pointers to Callback data
    for (unsigned int i = 0; i < NUM_CHANNELS; ++i)
        userdata[i] = &cb_data[i];

    // Initialize the multi-channel synchronizer for OFDM  
    MultiSync<ofdmframesync> ms(NUM_CHANNELS, 
                        {M, cp_len, taper_len, p}, callback, userdata);

    // Apply a phase correction of approximately pi to channel 1
    ms.AdjustNcoPhase(1, 3.14159);

    // ... Receive samples...

    // Process Channels
    for (i = 0; i < NUM_CHANNELS; ++i) {
            cb_data[i].buffer.clear();  // Clear callback-data
            ms.Execute(i, &samples);    // Process samples

            // Skip, if no frame was detected    
            if (cb_data[i].buffer.size()){
                // Store the CFR of the last frame detected in the sample block 
                ms.GetCfr(i, &cfr.cfr);
            };
    };
```