#include "power_trigger.h"

/**
 * @brief Default constructor for `PowerTrigger`.
 * 
 * Initializes an empty PowerTrigger object
 */
PowerTrigger::PowerTrigger() {
    power_thres_ = SR_POWER_THRES;
    window_size_ = SR_POWER_WINDOW;
    num_sample_to_skip_ = SR_SKIP_SAMPLE;
}

/**
 * @brief Construct a new PowerTrigger object with custom trigger_ parameters.
 *
 * Initializes the power trigger_ logic with the given threshold, window size, 
 * and number of samples to skip after reset or parameter change.
 *
 * @param power_thres_        Absolute threshold for the I-component to trigger_ detection.
 * @param window_size        Number of consecutive low-power samples required to deactivate the trigger_.
 * @param num_sample_to_skip_ Number of samples to skip before the trigger_ becomes active after configuration-change.
 */
PowerTrigger::PowerTrigger(uint16_t power_thres_, uint16_t window_size, uint32_t num_sample_to_skip_){
    power_thres_ = power_thres_;
    window_size_ = window_size;
    num_sample_to_skip_ = num_sample_to_skip_;
};

/**
 * @brief Destroy the PowerTrigger object
 * 
 */
PowerTrigger::~PowerTrigger() {}

/**
 * @brief Evaluate an incoming IQ sample and determine if a power trigger_ should be activated.
 *
 * This function processes a 32-bit IQ sample and extracts the I-component (upper 16 bits).
 * It calculates the absolute value of the I-component to check if it exceeds a predefined
 * power threshold. The function uses a state_ machine with the following state_s:
 *
 * - S_SKIP: Skips an initial number of samples.
 * - S_IDLE: Waits for a sample that exceeds the power-threshold (trigger_ condition).
 * - S_PACKET: After triggering, waits for the signal to drop below the threshold for a
 *             specified number of samples before resetting the trigger_.
 *
 * If a parameter affecting the trigger_ behavior (e.g., threshold or skip count) was
 * recently changed, the sample counter is reset to start the skip phase again.
 *
 * @param sample_in Pointer to a 32-bit IQ sample (upper 16 bits = I-component, lower 16 bits = Q).
 * @return true if the current state_ is actively triggering, false otherwise.
 */
bool PowerTrigger::GetTrigger(int32_t* sample_in){
    input_i_ = (*sample_in >> 16) & 0xFFFF;          // extract I-component from IQ-sample (bit 16 to 31)
    abs_i_ = (input_i_ < 0) ? -input_i_ : input_i_;     // convert negative values into positive 

    // ignore samples after trigger_-parameter changed
    if (num_sample_changed_){
        sample_count_ = 0;
        state_ = S_SKIP;
    }; 

    // Switch between ignoring incoming sample (S_SKIP), 
    switch(state_){
        case S_SKIP: 
        // ignore sample
            if(sample_count_ > num_sample_to_skip_){
                // start looking for power-trigger_
                state_ = S_IDLE;
            } else {
                // ignore sample
                sample_count_++;
            };

        case S_IDLE: 
        // trigger_ on any significant sample 
            if (abs_i_ > power_thres_){
                trigger_ = 1;
                sample_count_ = 0;
                state_ = S_PACKET;
            }

        case S_PACKET: 
            if (abs_i_ < power_thres_) {
                if (sample_count_ > window_size_) {
                    // go back to idle after detecting specified number of low-power-samples in a row
                    trigger_ = 0;
                    state_ = S_IDLE;
                } else {
                    // increase counter for low-power-samples 
                    sample_count_++;
                }
             } else {
                // don't start counting low-power-samples during active trigger_-condition 
                sample_count_ = 0;
             }
    };
}

/**
 * @brief Reset the internal state_ of the power trigger_ logic.
 *
 * This method resets all relevant internal variables of the PowerTrigger object,
 * including the sample counter, trigger_ flag, absolute I-value, and state_ machine.
 * After calling this method, the trigger_ logic will re-enter the initial S_SKIP state_,
 * waiting to skip a predefined number of samples before evaluating new trigger_ conditions.
 */
void PowerTrigger::Reset(){
    sample_count_ = 0;
    trigger_ = false;
    abs_i_ = 0;
    state_ = S_SKIP;
}

