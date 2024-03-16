#include "gtest/gtest.h"
#include "imkcpp.hpp"
#include <cstdio>
#include <numeric>
#include <random>

TEST(Send_Tests, Send_ValidValues) {
    using namespace imkcpp;

    constexpr size_t max_segment_size = MTU_TO_MSS<constants::IKCP_MTU_DEF>();
    constexpr size_t min_data_size = 1;
    constexpr size_t max_data_size = max_segment_size * 255;
    constexpr size_t step = max_segment_size / 2;

    constexpr size_t tests_count = (max_data_size - min_data_size) / step;

    struct DurationResult {
        std::chrono::microseconds duration;
        size_t size;
        size_t segments_count;
    };
    std::vector<DurationResult> durations(tests_count);

    std::cout << "Running " << tests_count << " Send_ValidValues tests" << std::endl;

    for (size_t size = min_data_size; size < max_data_size; size += step) {
        ImKcpp<constants::IKCP_MTU_DEF> kcp_output(Conv{0});
        kcp_output.set_send_window(2048);
        kcp_output.set_receive_window(2048);
        kcp_output.set_congestion_window_enabled(false);

        auto segments_count = kcp_output.estimate_segments_count(size);

        std::vector<std::vector<std::byte>> captured_data;
        captured_data.reserve(segments_count);
        auto output_callback = [&captured_data](std::span<const std::byte> data) {
            captured_data.emplace_back(data.begin(), data.end());
        };

        kcp_output.update(0, output_callback);

        ImKcpp<constants::IKCP_MTU_DEF> kcp_input(Conv{0});
        kcp_input.set_send_window(2048);
        kcp_input.set_receive_window(2048);
        kcp_input.update(0, [](std::span<const std::byte>) { });

        std::vector<std::byte> send_buffer(size);
        for (u32 j = 0; j < size; ++j) {
            send_buffer[j] = static_cast<std::byte>(j);
        }

        std::vector<std::byte> recv_buffer(size);

        std::vector<std::vector<std::byte>> captured_acks;
        captured_acks.reserve(segments_count);

        const auto start = std::chrono::steady_clock::now();

        auto send_result = kcp_output.send(send_buffer);
        ASSERT_TRUE(send_result.has_value()) << err_to_str(send_result.error());
        ASSERT_EQ(send_result.value(), size);

        auto update_result = kcp_output.update(200, output_callback);
        ASSERT_EQ(update_result.cmd_ack_count, 0);
        ASSERT_EQ(update_result.cmd_wask_count, 0);
        ASSERT_EQ(update_result.cmd_wins_count, 0);
        ASSERT_EQ(update_result.timeout_retransmitted_count, 0);
        ASSERT_EQ(update_result.fast_retransmitted_count, 0);
        ASSERT_GT(update_result.total_bytes_sent, 0);

        for (auto& captured : captured_data) {
            auto input_result = kcp_input.input(captured);
            ASSERT_TRUE(input_result.has_value()) << err_to_str(input_result.error());
            ASSERT_EQ(input_result.value().total_bytes_received, captured.size());
        }

        auto recv_result = kcp_input.recv(recv_buffer);
        ASSERT_TRUE(recv_result.has_value()) << err_to_str(recv_result.error());

        for (size_t j = 0; j < size; ++j) {
            EXPECT_EQ(send_buffer.at(j), recv_buffer.at(j));
        }

        auto capture_acks_result = kcp_input.update(300, [&captured_acks](std::span<const std::byte> data) {
            captured_acks.emplace_back(data.begin(), data.end());
        });

        ASSERT_EQ(capture_acks_result.cmd_ack_count, segments_count);
        ASSERT_EQ(capture_acks_result.cmd_wask_count, 0);
        ASSERT_EQ(capture_acks_result.cmd_wins_count, 0);
        ASSERT_EQ(capture_acks_result.timeout_retransmitted_count, 0);
        ASSERT_EQ(capture_acks_result.fast_retransmitted_count, 0);
        ASSERT_EQ(capture_acks_result.total_bytes_sent, capture_acks_result.cmd_ack_count * serializer::fixed_size<SegmentHeader>());

        InputResult acks_input_result{};
        for (auto& captured : captured_acks) {
            auto input_result = kcp_output.input(captured);
            ASSERT_TRUE(input_result.has_value()) << err_to_str(input_result.error());
            ASSERT_EQ(input_result.value().total_bytes_received, captured.size());

            acks_input_result += input_result.value();
        }

        ASSERT_EQ(acks_input_result.cmd_ack_count, segments_count);
        ASSERT_EQ(acks_input_result.cmd_push_count, 0);
        ASSERT_EQ(acks_input_result.cmd_wask_count, 0);
        ASSERT_EQ(acks_input_result.cmd_wins_count, 0);
        ASSERT_EQ(acks_input_result.dropped_push_count, 0);
        ASSERT_EQ(acks_input_result.total_bytes_received, acks_input_result.cmd_ack_count * serializer::fixed_size<SegmentHeader>());

        kcp_output.update(5000, [](std::span<const std::byte>) {
            FAIL() << "Should not be called because all data should be acknowledged";
        });

        const auto end = std::chrono::steady_clock::now();

        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        durations.emplace_back(duration, size, segments_count);
    }

    auto total_duration = std::accumulate(durations.begin(), durations.end(), std::chrono::microseconds(0),
                                      [](const auto& sum, const auto& dur) { return sum + dur.duration; });

    auto [min_it, max_it] = std::minmax_element(durations.begin(), durations.end(),
                                                [](const auto& a, const auto& b) { return a.duration < b.duration; });

    auto average_duration = total_duration / durations.size();

    std::cout << "Total time taken: " << total_duration.count() << " microseconds" << std::endl;
    std::cout << "Best result: " << min_it->duration.count() << " microseconds, Size: " << min_it->size << ", Segments: " << min_it->segments_count << std::endl;
    std::cout << "Worst result: " << max_it->duration.count() << " microseconds, Size: " << max_it->size << ", Segments: " << max_it->segments_count << std::endl;
    std::cout << "Average result: " << average_duration.count() << " microseconds" << std::endl;
}

TEST(Send_Tests, Send_LossyScenario) {
    using namespace imkcpp;

    constexpr float loss_ratio = 0.5f;

    constexpr size_t max_segment_size = MTU_TO_MSS<constants::IKCP_MTU_DEF>();
    constexpr size_t size = max_segment_size * 120;

    ImKcpp<constants::IKCP_MTU_DEF> kcp_output(Conv{0});
    kcp_output.set_send_window(2048);
    kcp_output.set_receive_window(2048);
    kcp_output.set_interval(10);
    kcp_output.set_congestion_window_enabled(false);
    kcp_output.update(0, [](std::span<const std::byte>) { });

    ImKcpp<constants::IKCP_MTU_DEF> kcp_input(Conv{0});
    kcp_input.set_send_window(2048);
    kcp_input.set_receive_window(2048);
    kcp_input.set_interval(10);
    kcp_input.set_congestion_window_enabled(false);
    kcp_input.update(0, [](std::span<const std::byte>) { });

    std::vector<std::byte> send_buffer(size);
    for (u32 j = 0; j < size; ++j) {
        send_buffer[j] = static_cast<std::byte>(j);
    }

    std::vector<std::byte> recv_buffer(size);

    auto send_result = kcp_output.send(send_buffer);
    ASSERT_TRUE(send_result.has_value()) << err_to_str(send_result.error());
    ASSERT_EQ(send_result.value(), size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution dis(0.0f, 1.0f);

    auto should_drop = [&]() -> bool {
        const auto random = dis(gen);
        return random < loss_ratio;
    };

    auto output_to_input = [&](const std::span<const std::byte> data) {
        if (should_drop()) {
            return;
        }

        kcp_input.input(data);
    };

    auto input_to_output = [&](const std::span<const std::byte> data) {
        if (should_drop()) {
            return;
        }

        kcp_output.input(data);
    };

    size_t update_idx = 0;

    while (kcp_output.get_state() == State::Alive && kcp_input.peek_size() != size) {
        const auto now = static_cast<u32>(update_idx * 10);

        kcp_output.update(now, output_to_input);
        kcp_input.update(now, input_to_output);

        ++update_idx;
    }

    ASSERT_EQ(kcp_output.get_state(), State::Alive);

    auto recv_result = kcp_input.recv(recv_buffer);
    ASSERT_TRUE(recv_result.has_value()) << err_to_str(recv_result.error());

    for (size_t j = 0; j < size; ++j) {
        EXPECT_EQ(send_buffer.at(j), recv_buffer.at(j));
    }

    std::cout << "Send_LossyScenario finished in simulated " << update_idx << " calls" << std::endl;
}

TEST(Send_Tests, Send_SendWindowSmallerThanReceive) {
    using namespace imkcpp;

    constexpr size_t max_segment_size = MTU_TO_MSS<constants::IKCP_MTU_DEF>();
    constexpr size_t size = max_segment_size * 250;

    ImKcpp<constants::IKCP_MTU_DEF> kcp_output(Conv{0});
    kcp_output.set_send_window(128);
    kcp_output.set_receive_window(256);
    kcp_output.set_interval(10);
    kcp_output.update(0, [](std::span<const std::byte>) { });

    ImKcpp<constants::IKCP_MTU_DEF> kcp_input(Conv{0});
    kcp_input.set_send_window(128);
    kcp_input.set_receive_window(256);
    kcp_input.set_interval(10);
    kcp_input.update(0, [](std::span<const std::byte>) { });

    std::vector<std::byte> send_buffer(size);
    for (u32 j = 0; j < size; ++j) {
        send_buffer[j] = static_cast<std::byte>(j);
    }

    auto send_result = kcp_output.send(send_buffer);
    ASSERT_TRUE(send_result.has_value()) << err_to_str(send_result.error());
    ASSERT_EQ(send_result.value(), size);

    auto output_to_input = [&](const std::span<const std::byte> data) {
        kcp_input.input(data);
    };

    auto input_to_output = [&](const std::span<const std::byte> data) {
        kcp_output.input(data);
    };

    size_t update_idx = 0;

    while (kcp_output.get_state() == State::Alive && kcp_input.peek_size() != size) {
        const auto now = static_cast<u32>(update_idx * 10);

        kcp_output.update(now, output_to_input);
        kcp_input.update(now, input_to_output);

        ++update_idx;
    }

    ASSERT_EQ(kcp_output.get_state(), State::Alive);

    std::vector<std::byte> recv_buffer(size);
    auto recv_result = kcp_input.recv(recv_buffer);
    ASSERT_TRUE(recv_result.has_value()) << err_to_str(recv_result.error());

    for (size_t j = 0; j < size; ++j) {
        EXPECT_EQ(send_buffer.at(j), recv_buffer.at(j));
    }

    std::cout << "Send_SendWindowSmallerThanReceive finished in simulated " << update_idx << " calls" << std::endl;
}

TEST(Send_Tests, Send_FragmentedValidValues) {
    using namespace imkcpp;

    ImKcpp<constants::IKCP_MTU_DEF> kcp(Conv{0});
    kcp.set_send_window(2048);
    kcp.set_receive_window(2048);

    constexpr auto data_size = MTU_TO_MSS<constants::IKCP_MTU_DEF>() * 255;
    std::vector<std::byte> data(data_size);
    auto result = kcp.send(data);
    ASSERT_TRUE(result.has_value()) << err_to_str(result.error());
    ASSERT_EQ(result.value(), data_size);
}

TEST(Send_Tests, Send_SizeValid) {
    using namespace imkcpp;

    constexpr size_t max_segment_size = constants::IKCP_MTU_DEF - serializer::fixed_size<SegmentHeader>();
    constexpr size_t max_segments_count = 255;
    constexpr size_t max_data_size = max_segment_size * max_segments_count;
    constexpr size_t step = max_segment_size / 2;

    std::vector<std::byte> buffer(max_data_size + 1);

    for (size_t size = 1; size < max_data_size; size += step) {
        ImKcpp<constants::IKCP_MTU_DEF> kcp(Conv{0});
        kcp.set_send_window(255);
        kcp.set_receive_window(255);

        auto result = kcp.send({buffer.data(), size});
        ASSERT_TRUE(result.has_value()) << err_to_str(result.error());
        ASSERT_EQ(result.value(), size);
    }

    // Exceeds max data size
    {
        ImKcpp<constants::IKCP_MTU_DEF> kcp(Conv{0});
        kcp.set_send_window(255);
        kcp.set_receive_window(255);

        auto result = kcp.send({buffer.data(), max_data_size + 1});
        ASSERT_EQ(result.error(), error::too_many_fragments);
    }
}

TEST(Send_Tests, Send_ZeroBytes) {
    using namespace imkcpp;

    ImKcpp<constants::IKCP_MTU_DEF> kcp(Conv{0});

    std::vector<std::byte> data(0);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::buffer_too_small);
}

TEST(Send_Tests, Send_ExceedsWindowSize) {
    using namespace imkcpp;

    ImKcpp<constants::IKCP_MTU_DEF> kcp(Conv{0});
    kcp.set_send_window(128);
    kcp.set_receive_window(128);

    std::vector<std::byte> data(MTU_TO_MSS<constants::IKCP_MTU_DEF>() * 128 + 1);
    auto result = kcp.send(data);
    EXPECT_EQ(result.error(), error::exceeds_window_size);
}

TEST(Send_Tests, Send_ReceivedMalformedData) {
    using namespace imkcpp;

    ImKcpp<constants::IKCP_MTU_DEF> kcp(Conv{0});

    {
        std::vector<std::byte> data(serializer::fixed_size<SegmentHeader>() - 1);
        ASSERT_EQ(kcp.input(data).error(), error::less_than_header_size);
    }

    {
        std::vector<std::byte> data(serializer::fixed_size<SegmentHeader>());
        SegmentHeader header{};
        header.cmd = commands::PUSH;
        header.len = PayloadLen(128);

        size_t offset = 0;
        serializer::serialize(header, data, offset);

        ASSERT_EQ(kcp.input(data).error(), error::header_and_payload_length_mismatch);
    }
}