#include "gtest/gtest.h"

#include "imkcpp.hpp"

TEST(Send_Tests, Send_ValidValues) {
    using namespace imkcpp;

    for (size_t size = 1; size <= constants::IKCP_MTU_DEF - constants::IKCP_OVERHEAD; ++size) {
        ImKcpp kcp_output(0);
        kcp_output.update(0);

        ImKcpp kcp_input(0);
        kcp_input.update(0);

        std::vector<std::byte> send_buffer(size);
        for (u32 j = 0; j < size; ++j) {
            send_buffer[j] = static_cast<std::byte>(j);
        }

        auto segments_count = kcp_output.estimate_segments_count(size);

        std::vector<std::vector<std::byte>> captured_data;
        captured_data.reserve(segments_count);
        kcp_output.set_output([&captured_data](std::span<const std::byte> data, const ImKcpp&, std::optional<void*>) {
            captured_data.emplace_back(data.begin(), data.end());
        });

        auto send_result = kcp_output.send(send_buffer);
        EXPECT_TRUE(send_result.has_value()) << err_to_str(send_result.error());

        auto update_result = kcp_output.update(200);
        EXPECT_EQ(update_result, segments_count);

        ASSERT_EQ(captured_data.size(), segments_count);

        for (auto& captured : captured_data) {
            auto input_result = kcp_input.input(captured);
            EXPECT_TRUE(input_result.has_value()) << err_to_str(input_result.error());
        }

        std::vector<std::byte> recv_buffer(size);
        auto recv_result = kcp_input.recv(recv_buffer);
        EXPECT_TRUE(recv_result.has_value()) << err_to_str(recv_result.error());

        for (size_t j = 0; j < size; ++j) {
            EXPECT_EQ(send_buffer[j], recv_buffer[j]);
        }
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