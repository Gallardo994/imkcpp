#include "benchmark/benchmark.h"
#include "original/ikcp.h"
#include "types.hpp"

void BM_original_send_receive_cycle_unconstrained(benchmark::State& state) {
    const auto size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        ikcpcb* kcp_output = ikcp_create(0, nullptr);
        ikcp_wndsize(kcp_output, 2048, 2048);
        ikcp_nodelay(kcp_output, 0, 100, 0, 1);

        std::vector<std::vector<char>> captured_data;
        auto output_callback = [](const char* data, const int len, ikcpcb*, void* user) -> int {
            auto& c_data = *static_cast<std::vector<std::vector<char>>*>(user);
            c_data.emplace_back(data, data + len);
            return 0;
        };

        ikcp_update(kcp_output, 0);

        ikcpcb* kcp_input = ikcp_create(0, nullptr);
        ikcp_wndsize(kcp_input, 2048, 2048);
        ikcp_nodelay(kcp_input, 0, 100, 0, 1);
        ikcp_update(kcp_input, 0);

        std::vector<char> send_buffer(size);
        for (int j = 0; j < size; ++j) {
            send_buffer[j] = static_cast<char>(j);
        }

        std::vector<char> recv_buffer(size);

        std::vector<std::vector<char>> captured_acks;
        auto acks_callback = [](const char* data, const int len, ikcpcb*, void* user) -> int {
            auto& c_acks = *static_cast<std::vector<std::vector<char>>*>(user);
            c_acks.emplace_back(data, data + len);
            return 0;
        };

        auto dummy_callback = [](const char*, int, ikcpcb*, void*) -> int {
            return 0;
        };

        state.ResumeTiming();

        ikcp_send(kcp_output, send_buffer.data(), send_buffer.size());

        kcp_output->user = &captured_data;
        ikcp_setoutput(kcp_output, output_callback);
        ikcp_update(kcp_output, 200);
        kcp_output->user = nullptr;

        for (auto& captured : captured_data) {
            ikcp_input(kcp_input, captured.data(), captured.size());
        }

        const auto recvlen = ikcp_recv(kcp_input, recv_buffer.data(), recv_buffer.size());
        if (recvlen != size) {
            state.SkipWithError("ikcp_recv() failed");
        }

        kcp_input->user = &captured_acks;
        ikcp_setoutput(kcp_input, acks_callback);
        ikcp_update(kcp_input, 300);
        kcp_input->user = nullptr;

        for (auto& captured : captured_acks) {
            ikcp_input(kcp_output, captured.data(), captured.size());
        }

        ikcp_setoutput(kcp_output, dummy_callback);
        ikcp_update(kcp_output, 5000);

        state.PauseTiming();
        ikcp_release(kcp_input);
        ikcp_release(kcp_output);
        state.ResumeTiming();
    }
}

BENCHMARK(BM_original_send_receive_cycle_unconstrained)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000000)
    ->Arg(64)
    ->Arg(256)
    ->Arg(2048)
    ->Arg(16384)
    ->Arg(131072);