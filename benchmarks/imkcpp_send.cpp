#include "benchmark/benchmark.h"
#include "imkcpp.hpp"

void BM_imkcpp_send_receive_cycle_unconstrained(benchmark::State& state) {
    using namespace imkcpp;

    const auto size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        ImKcpp<constants::IKCP_MTU_DEF> kcp_output(0);

        kcp_output.set_send_window(2048);
        kcp_output.set_receive_window(2048);
        kcp_output.set_congestion_window_enabled(false);

        const auto segments_count = kcp_output.estimate_segments_count(size);

        std::vector<std::vector<std::byte>> captured_data;
        captured_data.reserve(segments_count);
        auto output_callback = [&captured_data](std::span<const std::byte> data) {
            captured_data.emplace_back(data.begin(), data.end());
        };

        kcp_output.update(0, output_callback);

        ImKcpp<constants::IKCP_MTU_DEF> kcp_input(0);
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
        auto acks_callback = [&captured_acks](std::span<const std::byte> data) {
            captured_acks.emplace_back(data.begin(), data.end());
        };

        auto dummy_callback = [](std::span<const std::byte>) { };

        state.ResumeTiming();

        kcp_output.send(send_buffer);
        kcp_output.update(200, output_callback);

        for (auto& captured : captured_data) {
            kcp_input.input(captured);
        }

        const auto recvlen = kcp_input.recv(recv_buffer);
        if (recvlen.value_or(0) != size) {
            state.SkipWithError("kcp_input.recv() failed");
        }

        kcp_input.update(300, acks_callback);

        for (auto& captured : captured_acks) {
            kcp_output.input(captured);
        }

        kcp_output.update(5000, dummy_callback);
    }
}

void BM_imkcpp_send(benchmark::State& state) {
    using namespace imkcpp;

    const auto size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        ImKcpp<constants::IKCP_MTU_DEF> kcp_output(0);

        kcp_output.set_send_window(2048);
        kcp_output.set_receive_window(2048);
        kcp_output.set_congestion_window_enabled(false);

        auto output_callback = [](std::span<const std::byte>) {

        };

        kcp_output.update(0, output_callback);

        std::vector<std::byte> send_buffer(size);
        for (u32 j = 0; j < size; ++j) {
            send_buffer[j] = static_cast<std::byte>(j);
        }

        state.ResumeTiming();

        kcp_output.send(send_buffer);
        kcp_output.update(200, output_callback);
    }
}

void BM_imkcpp_input_receive(benchmark::State& state) {
    using namespace imkcpp;

    const auto size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        ImKcpp<constants::IKCP_MTU_DEF> kcp_output(0);

        kcp_output.set_send_window(2048);
        kcp_output.set_receive_window(2048);
        kcp_output.set_congestion_window_enabled(false);

        const auto segments_count = kcp_output.estimate_segments_count(size);

        std::vector<std::vector<std::byte>> captured_data;
        captured_data.reserve(segments_count);
        auto output_callback = [&captured_data](std::span<const std::byte> data) {
            captured_data.emplace_back(data.begin(), data.end());
        };

        kcp_output.update(0, output_callback);

        ImKcpp<constants::IKCP_MTU_DEF> kcp_input(0);
        kcp_input.set_send_window(2048);
        kcp_input.set_receive_window(2048);
        kcp_input.update(0, [](std::span<const std::byte>) { });

        std::vector<std::byte> send_buffer(size);
        for (u32 j = 0; j < size; ++j) {
            send_buffer[j] = static_cast<std::byte>(j);
        }

        std::vector<std::byte> recv_buffer(size);

        kcp_output.send(send_buffer);
        kcp_output.update(200, output_callback);

        state.ResumeTiming();

        for (auto& captured : captured_data) {
            kcp_input.input(captured);
        }

        const auto recvlen = kcp_input.recv(recv_buffer);
        if (recvlen.value_or(0) != size) {
            state.SkipWithError("kcp_input.recv() failed");
        }
    }
}

BENCHMARK(BM_imkcpp_send_receive_cycle_unconstrained)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000000)
    ->Arg(64)
    ->Arg(256)
    ->Arg(2048)
    ->Arg(16384)
    ->Arg(131072);

BENCHMARK(BM_imkcpp_send)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000000)
    ->Arg(64)
    ->Arg(256)
    ->Arg(2048)
    ->Arg(16384)
    ->Arg(131072);

BENCHMARK(BM_imkcpp_input_receive)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(100000)
    ->Arg(64)
    ->Arg(256)
    ->Arg(2048)
    ->Arg(16384)
    ->Arg(131072);