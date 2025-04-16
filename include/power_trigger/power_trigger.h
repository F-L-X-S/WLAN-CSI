// Original vlang-code by Jinghao Shi (openofdm/verilog/power_trigger.v)

#ifndef POWER_TRIGGER_H
#define POWER_TRIGGER_H

#include <stdint.h>

// Default power-trigger parameters 
#define SR_POWER_THRES  100       // Threshold for power trigger
#define SR_POWER_WINDOW 80        // Number of samples to wait before reset the trigger signal
#define SR_SKIP_SAMPLE  5000000   // Number of samples to skip initially

// Stepnumbers
#define S_SKIP      0
#define S_IDLE      1
#define S_PACKET    2

class PowerTrigger {
    public:
        /**
         * @brief Default constructor for `PowerTrigger`.
         * 
         * Initializes an empty PowerTrigger object
         */
        PowerTrigger();

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
        PowerTrigger(uint16_t power_thres, uint16_t window_size, uint32_t num_sample_to_skip);

        /**
         * @brief Destroy the PowerTrigger object
         * 
         */
        ~PowerTrigger();

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
        bool GetTrigger(int32_t* sample_in);

        /**
         * @brief Reset the internal state of the power trigger logic.
         *
         * This method resets all relevant internal variables of the PowerTrigger object,
         * including the sample counter, trigger flag, absolute I-value, and state machine.
         * After calling this method, the trigger logic will re-enter the initial S_SKIP state,
         * waiting to skip a predefined number of samples before evaluating new trigger conditions.
         */
        void Reset();
    
    private:
        /// @brief Power threshold to trigger signal detection (absolute I must exceed this).
        uint16_t power_thres_;

        /// @brief Number of low-power samples required to end a trigger condition.
        uint16_t window_size_;

        /// @brief Number of initial samples to ignore after a configuration change.
        uint32_t num_sample_to_skip_;

        /// @brief Current state of the trigger state machine (S_SKIP, S_IDLE, or S_PACKET).
        uint8_t state_;
        
        /// @brief Flag indicating that trigger-related parameters were recently changed.
        bool num_sample_changed_;

        /// @brief Counter for the number of processed samples below th power-threshold
        uint32_t sample_count_;

        /// @brief Extracted in-phase component (I) from a 32-bit IQ-sample.
        uint16_t input_i_;   

        /// @brief Absolute value of the I-component, used for threshold comparison.
        uint16_t abs_i_;    

        /// @brief Trigger flag: true if a trigger condition is currently active.
        bool trigger_; 

};

#endif // POWER_TRIGGER_H