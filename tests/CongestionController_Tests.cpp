#include <gtest/gtest.h>
#include "imkcpp.hpp"

constexpr size_t TestMTU = 1500;
constexpr imkcpp::u32 WndSize = 128;

class CongestionControllerTest : public ::testing::Test {
protected:
    imkcpp::CongestionController<TestMTU> controller;

    void SetUp() override {
        controller.set_send_window(WndSize);
        controller.set_remote_window(WndSize);
    }
};

TEST_F(CongestionControllerTest, InitialState) {
    using namespace imkcpp;

    ASSERT_EQ(controller.get_receive_window(), constants::IKCP_WND_RCV);
    ASSERT_EQ(controller.get_remote_window(), WndSize);
    ASSERT_EQ(controller.get_send_window(), WndSize);
}

TEST_F(CongestionControllerTest, AdjustParametersEnabled) {
    controller.set_congestion_window_enabled(true);
    controller.adjust_parameters();
    ASSERT_EQ(controller.calculate_congestion_window(), 1);
}

TEST_F(CongestionControllerTest, AdjustParametersDisabled) {
    using namespace imkcpp;

    controller.set_congestion_window_enabled(false);
    controller.adjust_parameters();
    ASSERT_EQ(controller.calculate_congestion_window(), WndSize);
}

TEST_F(CongestionControllerTest, AdjustParametersUnderCongestion) {
    using namespace imkcpp;

    controller.packets_resent(50, 10);
    controller.adjust_parameters();

    constexpr u32 expectedCwnd = std::max<u32>(50 / 2, constants::IKCP_THRESH_MIN) + 10;
    ASSERT_EQ(controller.calculate_congestion_window(), std::min<u32>(expectedCwnd, WndSize));
}

TEST_F(CongestionControllerTest, PacketsResentEnabled) {
    using namespace imkcpp;

    controller.set_congestion_window_enabled(true);

    controller.packets_resent(60, 20);
    ASSERT_EQ(controller.get_ssthresh(), std::max<u32>(60 / 2, constants::IKCP_THRESH_MIN));
    ASSERT_EQ(controller.calculate_congestion_window(), 50);
}

TEST_F(CongestionControllerTest, PacketsResentDisabled) {
    using namespace imkcpp;

    controller.set_congestion_window_enabled(false);

    controller.packets_resent(60, 20);
    ASSERT_EQ(controller.get_ssthresh(), std::max<u32>(60 / 2, constants::IKCP_THRESH_MIN));
    ASSERT_EQ(controller.calculate_congestion_window(), WndSize);
}

TEST_F(CongestionControllerTest, PacketLost) {
    using namespace imkcpp;

    controller.packet_lost();
    ASSERT_EQ(controller.get_ssthresh(), constants::IKCP_THRESH_MIN);
    ASSERT_EQ(controller.calculate_congestion_window(), 1);
}

TEST_F(CongestionControllerTest, EnsureAtLeastOnePacket) {
    controller.packet_lost();
    controller.ensure_at_least_one_packet_in_flight();
    ASSERT_GE(controller.calculate_congestion_window(), 1);
}

