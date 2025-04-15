#include <power_trigger/power_trigger.h>
#include <gtest/gtest.h>
 
// Test, if the PowerTrigger keeps the trigger as long as defined
TEST(PowerTriggerTest, HoldsTriggerWindow) {
    // initialize the PowerTrigger with a threshold of 1000, window size of 5, and skip count of 2
    PowerTrigger power_trigger(1000, 5, 2);

    // simulate low- and high-power samples
    int16_t i_low = 10, q_low = 5;
    int16_t i_high = 1001, q_high = 800;
    int32_t iq_sample_low_power = (i_low << 16) | (q_low & 0xFFFF);
    int32_t iq_sample_high_power = (i_high << 16) | (q_high & 0xFFFF);

    // Set the initial state of the trigger_ to false
    bool trigger = false;

    // Simulate the trigger_ being initially deactivated and ignoring first samples after initialization (State S_Skip)
    for (int i = 0; i < 2; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_high_power);
        EXPECT_FALSE(trigger)<<"Trigger should ignore first samples after initialization with state false";
    }

    // Simulate the trigger_ being initially deactivated (State S_IDLE)
    for (int i = 0; i < 8; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_low_power);
        EXPECT_FALSE(trigger)<<"Trigger should be initially false for low-power samples";
    }

    // Simulate the trigger_ being activated by three high-power samples (State S_PACKET)
    for (int i = 0; i < 3; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_high_power);
        EXPECT_TRUE(trigger)<<"Trigger should be true for high-power samples";
    }

    // Simulate the trigger_ being activated by holding the window size during low-power samples (State S_PACKET)
    for (int i = 0; i < 5; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_low_power);
        EXPECT_TRUE(trigger)<<"Trigger should be true for low-power samples within window size";
    }

    // Simulate the trigger_ being deactivated by exceeding the window size of low-power samples (State S_IDLE)
    for (int i = 0; i < 8; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_low_power);
        EXPECT_FALSE(trigger)<<"Trigger should be false after exceeding window size of low-power samples";
    }
} 

// Test, if the PowerTrigger resets the trigger after calling the reset-function during the trigger-window
TEST(PowerTriggerTest, ResetsTriggerWindow) {
    // initialize the PowerTrigger with a threshold of 1000, window size of 5, and skip count of 2
    PowerTrigger power_trigger(1000, 5, 2);

    // simulate low- and high-power samples
    int16_t i_low = 10, q_low = 5;
    int16_t i_high = -1001, q_high = 800;
    int32_t iq_sample_low_power = (i_low << 16) | (q_low & 0xFFFF);
    int32_t iq_sample_high_power = (i_high << 16) | (q_high & 0xFFFF);

    // Set the initial state of the trigger_ to false
    bool trigger = false;

    // Simulate the trigger_ being initially deactivated and ignoring first samples after initialization (State S_Skip)
    for (int i = 0; i < 2; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_high_power);
        EXPECT_FALSE(trigger)<<"Trigger should ignore first samples after initialization with state false";
    }

    // Simulate the trigger_ being initially deactivated (State S_IDLE)
    for (int i = 0; i < 8; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_low_power);
        EXPECT_FALSE(trigger)<<"Trigger should be initially false for low-power samples";
    }

    // Simulate the trigger_ being activated by three high-power samples (State S_PACKET)
    for (int i = 0; i < 3; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_high_power);
        EXPECT_TRUE(trigger)<<"Trigger should be true for high-power samples";
    }

    // Simulate the trigger_ being activated by holding the window size during low-power samples (State S_PACKET)
    for (int i = 0; i < 2; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_low_power);
        EXPECT_TRUE(trigger)<<"Trigger should be true for low-power samples within window size";
    }

    // Reset the Trigger during window size
    power_trigger.Reset();

    // Simulate the trigger_ being deactivated after resetting during window (State S_IDLE)
    for (int i = 0; i < 8; ++i) {
        trigger = power_trigger.GetTrigger(&iq_sample_low_power);
        EXPECT_FALSE(trigger)<<"Trigger should be false after resetting the trigger during window size";
    }
} 