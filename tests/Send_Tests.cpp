#include "gtest/gtest.h"
#include "imkcpp.hpp"
#include "commands.hpp"

TEST(Send_Tests, Send_ValidValues) {
    using namespace imkcpp;

    constexpr size_t max_segment_size = constants::IKCP_MTU_DEF - constants::IKCP_OVERHEAD;
    constexpr size_t min_data_size = 1;
    constexpr size_t max_data_size = max_segment_size * 255;
    constexpr size_t step = max_segment_size / 2;

    for (size_t size = min_data_size; size < max_data_size; size += step) {
        ImKcpp kcp_output(0);
        kcp_output.set_wndsize(2048, 2048);
        kcp_output.set_congestion_window_enabled(false);

        auto segments_count = kcp_output.estimate_segments_count(size);

        std::vector<std::vector<std::byte>> captured_data;
        captured_data.reserve(segments_count);
        auto output_callback = [&captured_data](std::span<const std::byte> data) {
            captured_data.emplace_back(data.begin(), data.end());
        };

        kcp_output.update(0, output_callback);

        ImKcpp kcp_input(0);
        kcp_input.set_wndsize(2048, 2048);
        kcp_input.update(0, [](std::span<const std::byte>) { });

        std::vector<std::byte> send_buffer(size);
        for (u32 j = 0; j < size; ++j) {
            send_buffer[j] = static_cast<std::byte>(j);
        }

        auto send_result = kcp_output.send(send_buffer);
        ASSERT_TRUE(send_result.has_value()) << err_to_str(send_result.error());
        ASSERT_EQ(send_result.value(), size);

        auto update_result = kcp_output.update(200, output_callback);
        ASSERT_EQ(update_result.cmd_ack_count, 0);
        ASSERT_EQ(update_result.cmd_wask_count, 0);
        ASSERT_EQ(update_result.cmd_wins_count, 0);
        ASSERT_EQ(update_result.retransmitted_count, 0);

        for (auto& captured : captured_data) {
            auto input_result = kcp_input.input(captured);
            ASSERT_TRUE(input_result.has_value()) << err_to_str(input_result.error());
        }

        std::vector<std::byte> recv_buffer(size);
        auto recv_result = kcp_input.recv(recv_buffer);
        ASSERT_TRUE(recv_result.has_value()) << err_to_str(recv_result.error());

        for (size_t j = 0; j < size; ++j) {
            EXPECT_EQ(send_buffer.at(j), recv_buffer.at(j));
        }
    }
}

TEST(Send_Tests, Send_FragmentedValidValues) {
    using namespace imkcpp;

    ImKcpp kcp(0);
    kcp.set_wndsize(2048, 2048);

    auto data_size = kcp.get_max_segment_size() * 255;
    std::vector<std::byte> data(data_size);
    auto result = kcp.send(data);
    ASSERT_TRUE(result.has_value()) << err_to_str(result.error());
    ASSERT_EQ(result.value(), data_size);
}

TEST(Send_Tests, Send_SizeValid) {
    using namespace imkcpp;

    constexpr size_t max_segment_size = constants::IKCP_MTU_DEF - constants::IKCP_OVERHEAD;
    constexpr size_t max_segments_count = 255;
    constexpr size_t max_data_size = max_segment_size * max_segments_count;
    constexpr size_t step = max_segment_size / 2;

    std::vector<std::byte> buffer(max_data_size + 1);

    for (size_t size = 1; size < max_data_size; size += step) {
        ImKcpp kcp(0);
        kcp.set_wndsize(255, 255);

        auto result = kcp.send({buffer.data(), size});
        ASSERT_TRUE(result.has_value()) << err_to_str(result.error());
        ASSERT_EQ(result.value(), size);
    }

    // Exceeds max data size
    {
        ImKcpp kcp(0);
        kcp.set_wndsize(255, 255);

        auto result = kcp.send({buffer.data(), max_data_size + 1});
        ASSERT_EQ(result.error(), error::too_many_fragments);
    }
}

TEST(Send_Tests, Send_ZeroBytes) {
    using namespace imkcpp;

    ImKcpp kcp(0);

    std::vector<std::byte> data(0);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::buffer_too_small);
}

TEST(Send_Tests, Send_ExceedsWindowSize) {
    using namespace imkcpp;

    ImKcpp kcp(0);
    kcp.set_wndsize(128, 128);

    std::vector<std::byte> data(kcp.get_max_segment_size() * 128 + 1);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::exceeds_window_size);
}

TEST(Send_Tests, Send_ReceivedMalformedData) {
    using namespace imkcpp;

    ImKcpp kcp(0);

    {
        std::vector<std::byte> data(constants::IKCP_OVERHEAD - 1);
        ASSERT_EQ(kcp.input(data).error(), error::less_than_header_size);
    }

    {
        std::vector<std::byte> data(constants::IKCP_MTU_DEF + 1);
        ASSERT_EQ(kcp.input(data).error(), error::more_than_mtu);
    }

    {
        std::vector<std::byte> data(constants::IKCP_OVERHEAD);
        SegmentHeader header{};
        header.cmd = commands::IKCP_CMD_PUSH;
        header.len = 128;

        size_t offset = 0;
        header.encode_to(data, offset);

        ASSERT_EQ(kcp.input(data).error(), error::header_and_payload_length_mismatch);
    }
}