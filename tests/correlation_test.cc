#include <correlation/delay_sample.h>
#include <correlation/moving_avg.h>
#include <correlation/auto_corr.h>
#include <liquid.h>
#include <gtest/gtest.h>

#define TESTCYCLES 100
#define BUFFER_SIZE 3

// Test, if the DelaySample class correctly stores and retrieves delayed samples
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

// Test, if the MovingAverage class correctly calculates the moving average for int32_t samples
TEST(MovingAverageTest, MovingAvgInt) {
    // Create an instance of MovingAverage 
    MovingAverage<int32_t, 5> movingAvg;

    // example samples 
    int32_t samples[20] = {
        -1024, 2031, 578, -4300, 1234,
        6000, -3050, 0, 1450, -1250,
        3000, 4200, -6000, 1700, 800,
        -400, 1100, -1100, 2900, -2900
    };

    // expected moving average values (as int32_t)
    int32_t expected_avgs[20] = {
        -1024 / 5,                                  // Ø = -204.8
        ( -1024 + 2031 ) / 5,                       // Ø = 201.4
        ( -1024 + 2031 + 578 ) / 5,                 // Ø = 317
        ( -1024 + 2031 + 578 - 4300 ) / 5,          // Ø = -543
        ( -1024 + 2031 + 578 - 4300 + 1234 ) / 5,   // Ø = -296.2000
        (2031 + 578 -4300 +1234 + 6000) / 5,        // Ø = 1108.6
        (578 -4300 +1234 +6000 -3050) / 5,          // Ø = 92.4
        (-4300 +1234 +6000 -3050 + 0) / 5,          // Ø = -23.2
        (1234 +6000 -3050 + 0 +1450) / 5,           // Ø = 1126.8
        (6000 -3050 + 0 +1450 -1250) / 5,           // Ø = 630
        (-3050 + 0 +1450 -1250 + 3000) / 5,         // Ø = 30
        (0 +1450 -1250 + 3000 + 4200) / 5,          // Ø = 1480
        (1450 -1250 + 3000 + 4200 -6000) / 5,       // Ø = 280
        (-1250 + 3000 + 4200 -6000 + 1700) / 5,     // Ø = 330
        (3000 + 4200 -6000 + 1700 + 800) / 5,       // Ø = 740
        (4200 -6000 + 1700 + 800 -400) / 5,         // Ø = 60
        (-6000 + 1700 + 800 -400 + 1100) / 5,       // Ø = -560
        (1700 + 800 -400 + 1100 -1100) / 5,         // Ø = 420
        (800 -400 + 1100 -1100 + 2900) / 5,         // Ø = 660
        (-400 + 1100 -1100 + 2900 -2900) / 5        // Ø = -80
    };

    // Push samples into the moving average
    for (int i = 0; i < 20; ++i) {
        // push index as sample into the moving average
        movingAvg.push(samples[i]);

        // get the moving average value
        int32_t avg = movingAvg.avg();
        EXPECT_EQ(avg, expected_avgs[i])<< "at sample " << i;
    }
}


// Test, if the MovingAverage class correctly calculates the moving average for float samples
TEST(MovingAverageTest, MovingAvgFloat) {
    // Create an instance of MovingAverage 
    MovingAverage<float, 5> movingAvg;

    // example samples 
    float samples[20] = {
        -1024, 2031, 578, -4300, 1234,
        6000, -3050, 0, 1450, -1250,
        3000, 4200, -6000, 1700, 800,
        -400, 1100, -1100, 2900, -2900
    };

    // expected moving average values (as float)
    float expected_avgs[20] = {
        -1024 / 5.0f,                                  // Ø = -204.8
        ( -1024 + 2031 ) / 5.0f,                       // Ø = 201.4
        ( -1024 + 2031 + 578 ) / 5.0f,                 // Ø = 317
        ( -1024 + 2031 + 578 - 4300 ) / 5.0f,          // Ø = -543
        ( -1024 + 2031 + 578 - 4300 + 1234 ) / 5.0f,   // Ø = -296.2000
        (2031 + 578 -4300 +1234 + 6000) / 5.0f,        // Ø = 1108.6
        (578 -4300 +1234 +6000 -3050) / 5.0f,          // Ø = 92.4
        (-4300 +1234 +6000 -3050 + 0) / 5.0f,          // Ø = -23.2
        (1234 +6000 -3050 + 0 +1450) / 5.0f,           // Ø = 1126.8
        (6000 -3050 + 0 +1450 -1250) / 5.0f,           // Ø = 630
        (-3050 + 0 +1450 -1250 + 3000) / 5.0f,         // Ø = 30
        (0 +1450 -1250 + 3000 + 4200) / 5.0f,          // Ø = 1480
        (1450 -1250 + 3000 + 4200 -6000) / 5.0f,       // Ø = 280
        (-1250 + 3000 + 4200 -6000 + 1700) / 5.0f,     // Ø = 330
        (3000 + 4200 -6000 + 1700 + 800) / 5.0f,       // Ø = 740
        (4200 -6000 + 1700 + 800 -400) / 5.0f,         // Ø = 60
        (-6000 + 1700 + 800 -400 + 1100) / 5.0f,       // Ø = -560
        (1700 + 800 -400 + 1100 -1100) / 5.0f,         // Ø = 420
        (800 -400 + 1100 -1100 + 2900) / 5.0f,         // Ø = 660
        (-400 + 1100 -1100 + 2900 -2900) / 5.0f        // Ø = -80
    };

    // Push samples into the moving average
    for (int i = 0; i < 20; ++i) {
        // push index as sample into the moving average
        movingAvg.push(samples[i]);

        // get the moving average value
        float avg = movingAvg.avg();
        EXPECT_EQ(avg, expected_avgs[i])<< "at sample " << i;
    }
}

// Test, if the AutoCorr class computes correct RXX for a simple real-valued triangular signal 
TEST(AutoCorrelationTest, TriangularAutoCorr) {
    // real-valued triangular signal
    std::vector<std::complex<float>> x = {
        {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0},
        {5, 0}, {4, 0}, {3, 0}, {2, 0}, {1, 0}
    };

    // MATLAB-computed expected values
    std::vector<float> expected_rxx = {
        0.0f, 0.0f, 0.0f, 4.0f, 14.0f,
        29.0f, 41.0f, 46.0f, 41.0f, 29.0f
    };

    // Create an instance of AutoCorr with a delay of 3 samples and a plateau threshold of 0.9
    AutoCorr auto_corr(0.9f, 3);

    // Process samples
    for (unsigned int i = 0; i < x.size(); ++i) {
        auto_corr.Push(x[i]);
        std::complex<float> rxx = auto_corr.GetRxx();
        EXPECT_EQ(std::abs(rxx), expected_rxx[i])<< "at sample " << i;
    }
};