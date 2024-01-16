# imkcpp

ImKcpp is a C++20 header-only implementation of KCP protocol with improvements ontop.

Original protocol explanation is available [here](https://github.com/skywind3000/kcp/blob/master/README.en.md), but in general the explanation is as follows:
> KCP is a fast and reliable protocol that can achieve the transmission effect of a reduction of the average latency by 30% to 40% and reduction of the maximum delay by a factor of three, at the cost of 10% to 20% more bandwidth wasted than TCP. It is implemented by using the pure algorithm, and is not responsible for the sending and receiving of the underlying protocol (such as UDP), requiring the users to define their own transmission mode for the underlying data packet, and provide it to KCP in the way of callback. Even the clock needs to be passed in from the outside, without any internal system calls.

# Why?
- Upstream implementation is written in C and is hard to read and modify
- Upstream implementation has type issues and depends on undefined behavior and compiler luck
- Upstream implementation is missing several key features like knowing max packet size
- Upstream implementation is poorly documented leaving users to guess how to use it and what parameters mean
- Upstream repository seems to be abandoned (last release was 2020)
- Upstream repository has no practical tests and benchmarks
- Upstream repository has unmerged bugfixes like [this one](https://github.com/skywind3000/kcp/pull/291)

# Is this compatible with original KCP?
Yes and no. At the current state, this implementation is compatible with original KCP on the byte level, however, window calculation for transmission has been changed, and "stream"/FASTACK_CONSERVE have been removed. Also, further changes and updates may break compatibility with upstream. Use with original KCP only at your own risk.

# How to use
- Copy `imkcpp/include` to your project
- Include `imkcpp.hpp` in your source code
- Create and use `imkcpp::ImKcpp<size_t MTU>` object as you would use `ikcp` object

# Documentation

Documentation is available [here](https://gallardo994.github.io/imkcpp/html/index.html).

# Special thanks
- [skywind3000](https://github.com/skywind3000) for the original [kcp](https://github.com/skywind3000/kcp) implementation
- [TartanLlama](https://github.com/TartanLlama) for the [unexpected](https://github.com/TartanLlama/expected) implementation

# Contacts

- Telegram: [@boikoigorigorevich](https://t.me/boikoigorigorevich)
- Email: [gx994@proton.me](mailto:gx994@proton.me)
