#ifndef AUTO_CORR_H
#define AUTO_CORR_H
#include <complex.h>
#include <liquid/liquid.h>

/**
 * @brief Class for auto-correlation of samples.
 * 
 * This class implements an auto-correlation algorithm for a given sample type. It
 * uses a moving average to calculate the correlation and detect plateaus in the data.
 */
class AutoCorr {
    public: 
        /**
         * @brief Default constructor for `AutoCorr`.
         * 
         * Initializes an empty AutoCorr object.
         */
        AutoCorr(float min_plateau, unsigned int delay);

        /**
         * @brief Destroy the AutoCorr object
         * 
         */
        ~AutoCorr();

        /**
         * @brief Push a new sample into the `AutoCorr` object.
         * 
         * This function stores the given sample in the auto-correlation object and
         * updates the plateau-detection state.
         *
         * @param sample The sample to be pushed into the auto-correlation object.
         */
        AutoCorr &Push(std::complex<float> sample);

        /**
         * @brief Reset the internal state of the AutoCorr object.
         * 
         * This method resets all relevant internal variables of the AutoCorr object.
         */
        AutoCorr &Reset();

        /**
         * @brief Set a new Value for the plateau detection threshold.
         *
         * @param min_plateu plateau detection threshold
         */
        AutoCorr &SetMinPlateau(float min_plateau);

        /**
         * @brief Get the Inidcator, if a plateau was detected.
         * 
         * @return true 
         * @return false 
         */
        bool PlateauDetected();

        /**
         * @brief Get the current normalized auto-correlation value.
         * 
         * @return The current normalized auto-correlation value.
         */
        std::complex<float>  GetRxx();

    private:
        /** 
         * @brief Auto-correlation object
         */
        autocorr_cccf corr_;

        /**
         * @brief Normalized Auto-correlation result 
         * 
         */
        std::complex<float> rxx_ = 0.0f;

        /**
         * @brief Minimum plateau detection threshold 
         * 
         */
        float min_plateau_ = 0.0f; 
    };

#endif // AUTO_CORR_H