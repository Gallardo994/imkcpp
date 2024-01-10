# imkcpp
C++20 implementation of KCP protocol with improvements ontop

# Why?
- Upstream implementation is written in C and is hard to read and modify
- Upstream implementation has type issues and depends on undefined behavior and compiler luck
- Upstream implementation is missing several key features like knowing max packet size
- Upstream implementation is limited to 255 segments per packet which may be too low for some use cases
- Upstream implementation is poorly documented leaving users to guess how to use it and what parameters mean
- Upstream repository seems to be abandoned (last release was 2020)
- Upstream repository has no practical tests and benchmarks
- Upstream repository has unmerged bugfixes like [this one](https://github.com/skywind3000/kcp/pull/291)

# How to use
- Copy `imkcpp/include` to your project
- Include `imkcpp.hpp` in your source code
- Create and use `imkcpp::ImKcpp` object as you would use `ikcp` object

# Documentation
- TODO

# Special thanks
- [skywind3000](https://github.com/skywind3000) for the original [kcp](https://github.com/skywind3000/kcp) implementation
- [TartanLlama](https://github.com/TartanLlama) for the [unexpected](https://github.com/TartanLlama/expected) implementation