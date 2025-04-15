#ifndef MOVING_AVG_H
#define MOVING_AVG_H

#include<correlation/delay_sample.h>

/**
 * @file moving_avg.h
 * @brief Header file for the MovingAverage class.
 * 
 * This file contains the definition of the MovingAverage class, which calculates
 * the moving average of a given number of samples.
 */
template <typename sample_type, int buffer_size>
class MovingAverage {
    public:
        /**
         * @brief Default constructor for `MovingAverage`.
         * 
         * Initializes the moving average with a delay buffer of the specified size.
         */
        MovingAverage &calculate(sample_type input) {
            sum += input;
            sum -= buffer.get();
            buffer.push(input);
            return *this;
        }

        /**
         * @brief Get the current moving average.
         * 
         * This function calculates the moving average based on the samples in the
         * delay buffer.
         *
         * @return The current moving average.
         */
        sample_type avg() const { return sum / buffer_size; }

    private:
        /**
         * @brief The delay buffer.
         * 
         * This buffer stores the samples in a circular manner.
         */
        DelaySample<sample_type, buffer_size> buffer;

        /**
         * @brief The sum of the samples in the delay buffer.
         * 
         * This variable stores the sum of the samples in the delay buffer.
         */
        sample_type sum = {};
};

#endif // MOVING_AVG_H