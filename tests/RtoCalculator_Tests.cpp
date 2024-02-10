#include <gtest/gtest.h>
#include "imkcpp.hpp"
#include <chrono>

class RtoCalculatorTest : public testing::Test {
protected:
    imkcpp::RtoCalculator rto_calculator;

    void SetUp() override {
        rto_calculator.set_interval(std::chrono::milliseconds(10));
    }
};

TEST_F(RtoCalculatorTest, InitialRtoIsDefault) {
    using namespace imkcpp;
    EXPECT_EQ(rto_calculator.get_rto(), milliseconds_t(imkcpp::constants::IKCP_RTO_DEF));
}

TEST_F(RtoCalculatorTest, InitialLastRttIsZero) {
    using namespace imkcpp;
    EXPECT_EQ(rto_calculator.get_last_rtt(), milliseconds_t(0));
}

TEST_F(RtoCalculatorTest, UpdateRtoWithValidRtt) {
    using namespace imkcpp;

    const timepoint_t now = std::chrono::steady_clock::now();

    const timepoint_t current_time = now + 1000ms;
    const timepoint_t timestamp = now + 950ms; // RTT of 50

    rto_calculator.update_rto(current_time, timestamp);

    EXPECT_EQ(rto_calculator.get_last_rtt(), milliseconds_t(50));
    EXPECT_EQ(rto_calculator.get_rto(), milliseconds_t(150));
}

TEST_F(RtoCalculatorTest, UpdateRtoWithNegativeRtt) {
    using namespace imkcpp;

    const timepoint_t now = std::chrono::steady_clock::now();

    const timepoint_t current_time = now + 1000ms;
    const timepoint_t timestamp = now + 1100ms; // Negative RTT

    rto_calculator.update_rto(current_time, timestamp);

    EXPECT_EQ(rto_calculator.get_last_rtt(), milliseconds_t(0));
    EXPECT_EQ(rto_calculator.get_rto(), milliseconds_t(constants::IKCP_RTO_DEF));
}