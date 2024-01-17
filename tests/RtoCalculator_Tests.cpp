#include <gtest/gtest.h>
#include "imkcpp.hpp"

class RtoCalculatorTest : public testing::Test {
protected:
    imkcpp::RtoCalculator rto_calculator;

    void SetUp() override {
        rto_calculator.set_interval(10);
    }
};

TEST_F(RtoCalculatorTest, InitialRtoIsDefault) {
    EXPECT_EQ(rto_calculator.get_rto(), imkcpp::constants::IKCP_RTO_DEF);
}

TEST_F(RtoCalculatorTest, InitialLastRttIsZero) {
    EXPECT_EQ(rto_calculator.get_last_rtt(), 0);
}

TEST_F(RtoCalculatorTest, UpdateRtoWithValidRtt) {
    using namespace imkcpp;

    constexpr u32 current_time = 1000;
    constexpr u32 timestamp = 950; // RTT of 50

    rto_calculator.update_rto(current_time, timestamp);

    EXPECT_EQ(rto_calculator.get_last_rtt(), 50);
    EXPECT_EQ(rto_calculator.get_rto(), 150);
}

TEST_F(RtoCalculatorTest, UpdateRtoWithNegativeRtt) {
    using namespace imkcpp;

    constexpr u32 current_time = 1000;
    constexpr u32 timestamp = 1100; // Negative RTT

    rto_calculator.update_rto(current_time, timestamp);

    EXPECT_EQ(rto_calculator.get_last_rtt(), 0);
    EXPECT_EQ(rto_calculator.get_rto(), constants::IKCP_RTO_DEF);
}