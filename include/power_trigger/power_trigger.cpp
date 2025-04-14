#include "power_trigger.h"

/**
 * @brief Default constructor for `PowerTrigger`.
 * 
 * Initializes an empty PowerTrigger object
 */
PowerTrigger::PowerTrigger() {
    power_thres = SR_POWER_THRES;
    window_size = SR_POWER_WINDOW;
    num_sample_to_skip = SR_SKIP_SAMPLE;
}

/**
 * @brief Construct a new PowerTrigger object with custom trigger parameters.
 *
 * Initializes the power trigger logic with the given threshold, window size, 
 * and number of samples to skip after reset or parameter change.
 *
 * @param power_thres        Absolute threshold for the I-component to trigger detection.
 * @param window_size        Number of consecutive low-power samples required to deactivate the trigger.
 * @param num_sample_to_skip Number of samples to skip before the trigger becomes active after configuration-change.
 */
PowerTrigger::PowerTrigger(uint16_t power_thres, uint16_t window_size, uint32_t num_sample_to_skip){
    power_thres = power_thres;
    window_size = window_size;
    num_sample_to_skip = num_sample_to_skip;
};

/**
 * @brief Destroy the PowerTrigger object
 * 
 */
PowerTrigger::~PowerTrigger() {}

/**
 * @brief Evaluate an incoming IQ sample and determine if a power trigger should be activated.
 *
 * This function processes a 32-bit IQ sample and extracts the I-component (upper 16 bits).
 * It calculates the absolute value of the I-component to check if it exceeds a predefined
 * power threshold. The function uses a state machine with the following states:
 *
 * - S_SKIP: Skips an initial number of samples.
 * - S_IDLE: Waits for a sample that exceeds the power-threshold (trigger condition).
 * - S_PACKET: After triggering, waits for the signal to drop below the threshold for a
 *             specified number of samples before resetting the trigger.
 *
 * If a parameter affecting the trigger behavior (e.g., threshold or skip count) was
 * recently changed, the sample counter is reset to start the skip phase again.
 *
 * @param sample_in Pointer to a 32-bit IQ sample (upper 16 bits = I-component, lower 16 bits = Q).
 * @return true if the current state is actively triggering, false otherwise.
 */
bool PowerTrigger::getTrigger(int32_t* sample_in){
    input_i = (*sample_in >> 16) & 0xFFFF;          // extract I-component from IQ-sample (bit 16 to 31)
    abs_i = (input_i < 0) ? -input_i : input_i;     // convert negative values into positive 

    // ignore samples after trigger-parameter changed
    if (num_sample_changed){
        sample_count = 0;
        state = S_SKIP;
    }; 

    // Switch between ignoring incoming sample (S_SKIP), 
    switch(state){
        case S_SKIP: 
        // ignore sample
            if(sample_count > num_sample_to_skip){
                // start looking for power-trigger
                state = S_IDLE;
            } else {
                // ignore sample
                sample_count++;
            };

        case S_IDLE: 
        // trigger on any significant sample 
            if (abs_i > power_thres){
                trigger = 1;
                sample_count = 0;
                state = S_PACKET;
            }

        case S_PACKET: 
            if (abs_i < power_thres) {
                if (sample_count > window_size) {
                    // go back to idle after detecting specified number of low-power-samples in a row
                    trigger = 0;
                    state = S_IDLE;
                } else {
                    // increase counter for low-power-samples 
                    sample_count++;
                }
             } else {
                // don't start counting low-power-samples during active trigger-condition 
                sample_count = 0;
             }
    };
}

/**
 * @brief Reset the internal state of the power trigger logic.
 *
 * This method resets all relevant internal variables of the PowerTrigger object,
 * including the sample counter, trigger flag, absolute I-value, and state machine.
 * After calling this method, the trigger logic will re-enter the initial S_SKIP state,
 * waiting to skip a predefined number of samples before evaluating new trigger conditions.
 */
void PowerTrigger::reset(){
    sample_count = 0;
    trigger = false;
    abs_i = 0;
    state = S_SKIP;
}

