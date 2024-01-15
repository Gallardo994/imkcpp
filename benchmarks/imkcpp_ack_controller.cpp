#include "benchmark/benchmark.h"
#include "imkcpp.hpp"

void BM_imkcpp_ack_controller_fastackctx_update(benchmark::State& state) {
    using namespace imkcpp;

    for (auto _ : state) {
        state.PauseTiming();

        FastAckCtx fast_ack_ctx;

        state.ResumeTiming();

        for (u32 i = 0; i < 4096; ++i) {
            fast_ack_ctx.update(i, i);
        }
    }
}

BENCHMARK(BM_imkcpp_ack_controller_fastackctx_update)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(1000000);