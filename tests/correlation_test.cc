#include <correlation/delay_sample.h>
#include <gtest/gtest.h>

// Test, if the DelaySample class correctly stores and retrieves delayed samples
#define TESTCYCLES 100
#define BUFFER_SIZE 3
TEST(DelaySampleTest, DelayWindowCorrectness) {
    // Create an instance of DelaySample with a delay size of 16
    DelaySample<int32_t, BUFFER_SIZE> delaySample;

    // base-sample for multiplication
    int16_t i_component = 7, q_component = 13;
    
    // Push samples into the delay buffer
    for (int i = 0; i < TESTCYCLES; ++i) {
        // create example sample as combination of bas-sample and index
        int32_t iq_sample = ((i_component*i) << 16) | ((q_component+i) & 0xFFFF);

        // push the sample into the delay buffer
        delaySample.push(iq_sample);

        // create the expected sample as combination of base-sample and index
        // A Sample is expected to be delayed by BUFFER_SIZE-1
        uint32_t expected_sample;
        (i<(BUFFER_SIZE-1)) ?  expected_sample=0 : expected_sample = ((i_component*(i-(BUFFER_SIZE-1))) << 16) | ((q_component+(i-(BUFFER_SIZE-1))) & 0xFFFF);
        uint32_t delaySampleValue = delaySample.get();
        EXPECT_EQ(delaySampleValue, expected_sample) << "Delayed sample is incorrect at index " << i;
        }
}