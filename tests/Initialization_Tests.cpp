#include "gtest/gtest.h"

#include "imkcpp.hpp"
#include "constants.hpp"

TEST(Initialization_Tests, Mtu_Sets) {
    imkcpp kcp(0);

    kcp.set_mtu(128);

    EXPECT_EQ(kcp.get_mtu(), 128);
    EXPECT_EQ(kcp.get_max_segment_size(), 128 - IKCP_OVERHEAD);
}