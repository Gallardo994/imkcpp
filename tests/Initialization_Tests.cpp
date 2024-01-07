#include "gtest/gtest.h"

#include "imkcpp.hpp"
#include "constants.hpp"

TEST(Initialization_Tests, Mtu_Sets) {
    imkcpp kcp(0);

    kcp.set_mtu(128);

    EXPECT_EQ(kcp.get_mtu(), 128);
    EXPECT_EQ(kcp.get_max_segment_size(), 128 - IKCP_OVERHEAD);
}

TEST(Initialization_Tests, Mtu_InvalidValues) {
    imkcpp kcp(0);

    for (u32 i = 0; i < IKCP_OVERHEAD; ++i) {
        kcp.set_mtu(i);

        EXPECT_EQ(kcp.get_mtu(), IKCP_MTU_DEF);
        EXPECT_EQ(kcp.get_max_segment_size(), IKCP_MTU_DEF - IKCP_OVERHEAD);
    }
}