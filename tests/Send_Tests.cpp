#include "gtest/gtest.h"

#include "imkcpp.hpp"

TEST(Send_Tests, Send_ValidValues) {
    using namespace imkcpp;

    ImKcpp kcp(0);

    for (u32 i = 1; i <= kcp.get_max_segment_size(); ++i) {
        std::vector<std::byte> data(i);
        auto result = kcp.send(data);
        EXPECT_TRUE(result.has_value()) << err_to_str(result.error());
    }
}

TEST(Send_Tests, Send_FragmentedValidValues) {
    using namespace imkcpp;

    ImKcpp kcp(0);
    kcp.set_wndsize(2048, 2048);

    std::vector<std::byte> data(kcp.get_max_segment_size() * 255);
    auto result = kcp.send(data);
    EXPECT_TRUE(result.has_value()) << err_to_str(result.error());
    EXPECT_EQ(result.value(), 255);
}

TEST(Send_Tests, Send_ZeroBytes) {
    using namespace imkcpp;

    ImKcpp kcp(0);

    std::vector<std::byte> data(0);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::buffer_too_small);
}

TEST(Send_Tests, Send_TooManyFragments) {
    using namespace imkcpp;

    ImKcpp kcp(0);
    kcp.set_wndsize(2048, 2048);

    std::vector<std::byte> data(kcp.get_max_segment_size() * 255 + 1);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::too_many_fragments);
}

TEST(Send_Tests, Send_ExceedsWindowSize) {
    using namespace imkcpp;

    ImKcpp kcp(0);
    kcp.set_wndsize(128, 128);

    std::vector<std::byte> data(kcp.get_max_segment_size() * 255);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::exceeds_window_size);
}