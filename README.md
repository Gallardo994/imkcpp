# imkcpp
C++20 implementation of KCP protocol with improvements ontop

# Why?
- Upstream implementation is written in C and is hard to read and modify
- Upstream implementation has type issues and depends on undefined behavior and compiler luck
- Upstream repository seems to be abandoned (last release was 2020)
- Upstream repository has no practical tests and benchmarks
- Upstream repository has unmerged bugfixes like [this one](https://github.com/skywind3000/kcp/pull/291)

# How to use
- Copy `imkcpp/include` and `imkcpp/src` to your project
- Include `imkcpp.hpp` in your source code
- Create and use `imkcpp` object as you would use `ikcp` object

# Documentation
- TODO

# Special thanks
- [skywind3000](https://github.com/skywind3000) for the original [kcp](https://github.com/skywind3000/kcp) implementation