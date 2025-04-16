#ifndef AUTO_CORR_H
#define AUTO_CORR_H
#include <include/correlation/delay_sample.h>
#include <include/correlation/moving_avg.h>

/**
 * @file auto_corr.h
 * @brief Header file for the AutoCorr class.
 * 
 * This file contains the definition of the AutoCorr class, which implements an
 * auto-correlation algorithm for a given sample type.
 */

 /**
  * @brief Calculate the complex conjugate of a sample.
  * 
  * This function computes the complex conjugate of a given sample. The sample is
  * expected to be a 32-bit integer, where the upper 16 bits represent the real part
  * and the lower 16 bits represent the imaginary part.
  *
  * @param sample The input sample.
  * @return The complex conjugate of the input sample.
  */
template<typename sample_type>
sample_type ComplexConjugate(sample_type sample) {
    static_assert(std::is_integral<sample_type>::value, "sample_type must be an integral type");
    static_assert(sizeof(sample_type) == 4, "sample_type must be 32-bit wide");
    return (sample & 0xFFFF0000) | (~sample & 0x0000FFFF);
}

/**
 * @brief Perform complex multiplication of two samples.
 * 
 * This function computes the complex multiplication of two given samples. The samples
 * are expected to be 32-bit integers, where the upper 16 bits represent the real part
 * and the lower 16 bits represent the imaginary part.
 *
 * @param sample_a Pointer to the first sample.
 * @param sample_b Pointer to the second sample.
 * @return The result of the complex multiplication.
 */
template<typename sample_type>
sample_type ComplexMultiplication(const sample_type* sample_a, const sample_type* sample_b) {
    static_assert(std::is_integral<sample_type>::value, "sample_type must be an integral type");
    static_assert(sizeof(sample_type) == 4, "sample_type must be 32-bit wide");

    // extract I- and Q-Component
    int16_t a_i = static_cast<int16_t>(*sample_a >> 16);
    int16_t a_q = static_cast<int16_t>(*sample_a & 0xFFFF);
    int16_t b_i = static_cast<int16_t>(*sample_b >> 16);
    int16_t b_q = static_cast<int16_t>(*sample_b & 0xFFFF);

    // complex multiplication (a_i + j*a_q) * (b_i + j*b_q)
    int32_t i = static_cast<int32_t>(a_i) * b_i - static_cast<int32_t>(a_q) * b_q;
    int32_t q = static_cast<int32_t>(a_i) * b_q + static_cast<int32_t>(a_q) * b_i;

    // clip to 16 bit
    int16_t i_out = static_cast<int16_t>(i >> 15); 
    int16_t q_out = static_cast<int16_t>(q >> 15);  

    return (static_cast<sample_type>(i_out) << 16) | (static_cast<uint16_t>(q_out));
}

/**
 * @brief Approximates the magnitude of a complex number stored in a 32-bit integer.
 * 
 * The input sample is assumed to be a packed 32-bit complex number:
 * - Upper 16 bits: signed I (in-phase) component
 * - Lower 16 bits: signed Q (quadrature) component
 * 
 * The function extracts the I and Q components, takes their absolute values,
 * and returns an approximate magnitude using the formula:
 *     max + (min >> 2)
 * This is a computationally efficient approximation of sqrt(I² + Q²).
 * 
 * @tparam sample_type A 32-bit integral type (e.g., int32_t or uint32_t)
 * @param sample Pointer to the IQ sample to process
 * @return sample_type Approximate magnitude of the complex input
 */
template<typename sample_type>
sample_type ComplexToMag(const sample_type* sample) {
    static_assert(std::is_integral<sample_type>::value, "sample_type must be an integral type");
    static_assert(sizeof(sample_type) == 4, "sample_type must be 32-bit wide");

    uint16_t i = (*sample_in >> 16) & 0xFFFF;           // extract I-component from IQ-sample (bit 16 to 31)
    uint16_t q = (*sample_in) & 0xFFFF;                 // extract Q-component from IQ-sample (bit 0 to 15)

    // convert negative values into positive 
    uint16_t abs_i = (i < 0) ? -i : i;                  
    uint16_t abs_q = (q < 0) ? -q : q;                  

    // Use max + min/4 as a cheap magnitude approximation
    uint16_t max = abs_i > abs_q? abs_i: abs_q;         
    uint16_t min = abs_i > abs_q? abs_q: abs_i;

    return (max + (min>>2));

}

/**
 * @brief Calculates the squared magnitude of a complex IQ sample.
 * 
 * The sample is expected to be a packed 32-bit integer, where:
 * - Upper 16 bits: signed I (real) component
 * - Lower 16 bits: signed Q (imaginary) component
 * 
 * This function computes the magnitude squared using:
 *     |z|² = z * conj(z)
 * 
 * @tparam sample_type A 32-bit integral type (e.g., int32_t or uint32_t)
 * @param sample Pointer to the input sample
 * @return sample_type Squared magnitude of the complex input
 */
template<typename sample_type>
sample_type ComplexToMagSq(const sample_type* sample) {
    static_assert(std::is_integral<sample_type>::value, "sample_type must be an integral type");
    static_assert(sizeof(sample_type) == 4, "sample_type must be 32-bit wide");

    uint32_t conj_sample = ComplexConjugate<uint32_t>(*sample);      // conjugate the sample
    return ComplexMultiplication<uint32_t>(&sample, &conj_sample);  // multiply the sample with its conjugate    
}


/**
 * @brief Class for auto-correlation of samples.
 * 
 * This class implements an auto-correlation algorithm for a given sample type. It
 * uses a moving average to calculate the correlation and detect plateaus in the data.
 */
template<typename sample_type, int shift>
class AutoCorr {
    public: 
        /**
         * @brief Default constructor for `AutoCorr`.
         * 
         * Initializes an empty AutoCorr object.
         */
        AutoCorr(uint32_t min_plateau): min_plateau_(min_plateau) {};

        /**
         * @brief Destroy the AutoCorr object
         * 
         */
        ~AutoCorr();

        /**
         * @brief Reset the internal state of the AutoCorr object.
         * 
         * This method resets all relevant internal variables of the AutoCorr object,
         * including the plateau detection flag and the moving average buffers.
         */
        AutoCorr &Reset(){
            plateau_detected_ = false;
            kDelayInSample.Reset();
            kMovingAvgProd.Reset();
            kMovingAvgMagSq.Reset();
            return *this;
        };

        AutoCorr &Add(sample_type sample_in){
            kDelayInSample.push(sample_in);
            sample_type delayed_sample_conj = ComplexConjugate<sample_type>(.get());
            kMovingAvgProd.push(ComplexMultiplication(&sample_in, &delayed_sample_conj));
            sample_type prod_avg_mag = kMovingAvgProd.avg();
            ComplexToMag<sample_type>(&prod_avg_mag);

            // Moving Average of squared magnitude of incoming samples 
            kMovingAvgMagSq.push(ComplexToMagSq<sample_type>(&sample_in));

            return *this;
        };

        bool PlateauDetected(){
            return plateau_detected_;
        };

    private:
        /**
         * @brief The min_plateau_ value.
         * 
         * This variable stores the minimum plateau length for the auto-correlation.
         */
        uint32_t min_plateau_;

        /**
         * @brief The kDelayInSample object.
         * 
         * This object is used to delay the incoming samples.
         */
        static const DelaySample<sample_type, shift> kDelayInSample;

        /**
         * @brief The kMovingAvgProd object.
         * 
         * This object is used to calculate the moving average of the product of the delayed sample and the compl.conj. sample.
         * The Moving Average of kMovingAvgProd is mathematically a complex value.
         */
        static const MovingAverage<sample_type, shift> kMovingAvgProd;

        /**
         * @brief The kMovingAvgMagSq object.
         * 
         * This object is used to calculate the moving average of the product of squared magnitude of the incoming sample.
         * The Moving Average of kMovingAvgMagSq is mathematically a real value.
         */
        static const MovingAverage<sample_type, shift> kMovingAvgMagSq;

        /**
         * @brief Flag indicating if a plateau was detected
         */
        bool plateau_detected_ = false; 
    };

#endif // AUTO_CORR_H