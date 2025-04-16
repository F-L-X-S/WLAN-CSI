#ifndef DELAY_SAMPLE_H
#define DELAY_SAMPLE_H


/**
 * @file delay_sample.h
 * @brief Header file for the DelaySample class.
 * 
 * This file contains the definition of the DelaySample class, which implements a
 * circular buffer to store and retrieve delayed samples. 
 * A Sample is expected to be delayed by BUFFER_SIZE-1.
 */
template<typename sample_type, int buffer_size>
class DelaySample {
    public:
        /**
         * @brief Default constructor for `DelaySample`.
         * 
         * Initializes an empty delay buffer and sets the delay index to 0.
         */
        DelaySample() : delay_index_(0) {
            for (int i = 0; i < buffer_size; ++i) {
                delay_buffer_[i] = 0;
            }
        }

        /**
         * @brief Push a new sample into the delay buffer.
         * 
         * This function stores the given sample in the delay buffer and updates the
         * delay index to point to the next position in the circular buffer.
         *
         * @param sample The sample to be pushed into the delay buffer.
         */
        DelaySample &push(sample_type sample) {
            delay_buffer_[delay_index_] = sample;
            delay_index_ = (delay_index_ + 1) % buffer_size;
            return *this;
        }

        /**
         * @brief Get the delayed sample.
         * 
         * This function retrieves the sample that is delayed by one position in the
         * circular buffer.
         *
         * @return The delayed sample.
         */
        sample_type get() const {
            int index = (delay_index_ + buffer_size) % buffer_size;
            return delay_buffer_[index];
        }

    private:
        /**
         * @brief The delay buffer.
         * 
         * This buffer stores the samples in a circular manner.
         */
        sample_type delay_buffer_[buffer_size];

        /**
         * @brief The index of the current position in the delay buffer.
         * 
         * This index is used to keep track of where to insert the next sample.
         */
        int delay_index_;
};

#endif // DELAY_SAMPLE_H