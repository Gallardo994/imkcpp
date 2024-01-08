#include "gtest/gtest.h"

#include "imkcpp.hpp"
#include "constants.hpp"

TEST(Initialization_Tests, Mtu_ValidValues) {
    using namespace imkcpp;

    ImKcpp kcp(0);

    for (uint32_t i = constants::IKCP_OVERHEAD + 1; i < constants::IKCP_MTU_DEF; ++i) {
        EXPECT_TRUE(kcp.set_mtu(i).has_value());
        EXPECT_EQ(kcp.get_mtu(), i);
        EXPECT_EQ(kcp.get_max_segment_size(), i - constants::IKCP_OVERHEAD);
    }
}

TEST(Initialization_Tests, Mtu_InvalidValues) {
    using namespace imkcpp;

    ImKcpp kcp(0);

    for (u32 i = 0; i < constants::IKCP_OVERHEAD; ++i) {
        EXPECT_FALSE(kcp.set_mtu(i).has_value());
        EXPECT_EQ(kcp.get_mtu(), constants::IKCP_MTU_DEF);
        EXPECT_EQ(kcp.get_max_segment_size(), constants::IKCP_MTU_DEF - constants::IKCP_OVERHEAD);
    }
}