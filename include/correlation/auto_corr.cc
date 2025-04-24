#include "auto_corr.h"

/**
 * @brief Default constructor for `AutoCorr`.
 * 
 * Initializes an empty AutoCorr object.
 */
AutoCorr::AutoCorr(float min_plateau, unsigned int delay): 
    min_plateau_(min_plateau),
    corr_(autocorr_cccf_create(delay, delay)){};

/**
 * @brief Destroy the AutoCorr object
 * 
 */
AutoCorr::~AutoCorr(){
    autocorr_cccf_destroy(corr_);
};

/**
 * @brief Push a new sample into the `AutoCorr` object.
 * 
 * This function stores the given sample in the auto-correlation object and
 * updates the plateau-detection state.
 *
 * @param sample The sample to be pushed into the auto-correlation object.
 */
AutoCorr& AutoCorr::Push(std::complex<float> sample){
    // push sample into the autocorr object
    autocorr_cccf_push(corr_, sample);
    // execute the autocorr-calculation
    autocorr_cccf_execute(corr_, &rxx_);
    return *this;
};

/**
 * @brief Reset the internal state of the AutoCorr object.
 * 
 * This method resets all relevant internal variables of the AutoCorr object.
 */
AutoCorr& AutoCorr::Reset(){
    rxx_ = 0.0f;
    autocorr_cccf_reset(corr_);
    return *this;
};

/**
 * @brief Set a new Value for the plateau detection threshold.
 *
 * @param min_plateu plateau detection threshold
 */
AutoCorr& AutoCorr::SetMinPlateau(float min_plateau){
    Reset();
    min_plateau_ = min_plateau;
    return *this;
};

/**
 * @brief Get the Inidcator, if a plateau was detected.
 * 
 * @return true 
 * @return false 
 */
bool AutoCorr::PlateauDetected(){
    return std::abs(rxx_) > min_plateau_;
};

/**
 * @brief Get the current unnormalized complex auto-correlation value.
 * 
 * @return The current unnormalized complex auto-correlation value.
 */
std::complex<float> AutoCorr::GetRxx(){
    return rxx_;
};
