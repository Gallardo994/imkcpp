#pragma once

#include <vector>
#include <cassert>
#include "types.hpp"
#include "segment.hpp"
#include "shared_ctx.hpp"

namespace imkcpp {
    class Flusher {
        SharedCtx& shared_ctx;

        std::vector<std::byte> buffer{};
        size_t offset = 0;

        // Flushes the buffer to the given output
        [[nodiscard]] size_t flush(const output_callback_t& callback) {
            const auto size = this->offset;

            assert(size <= this->buffer.size());

            callback({this->buffer.data(), size});
            this->offset = 0;

            return size;
        }

    public:
        explicit Flusher(SharedCtx& shared_ctx) : shared_ctx(shared_ctx) {}

        // Returns true if the buffer is empty.
        [[nodiscard]] bool is_empty() const {
            return this->offset == 0;
        }

        // Resizes the buffer to the given size.
        void resize(const size_t size) {
            this->buffer.resize(size);
        }

        // Flushes the buffer to the given output if it exceeds Max Segment Size
        [[nodiscard]] size_t flush_if_full(const output_callback_t& target) {
            if (this->offset > this->shared_ctx.mss) {
                return this->flush(target);
            }

            return 0;
        }

        // Flushes the buffer to the given output if adding "size" bytes to the buffer would exceed Max Segment Size
        [[nodiscard]] size_t flush_if_does_not_fit(const output_callback_t& target, const size_t size) {
            if (this->offset + size > this->shared_ctx.mss) {
                return this->flush(target);
            }

            return 0;
        }

        // Flushes the buffer to the given output if it's not empty
        [[nodiscard]] size_t flush_if_not_empty(const output_callback_t& target) {
            if (!this->is_empty()) {
                return this->flush(target);
            }

            return 0;
        }

        // Encodes the given segment into the buffer
        void encode(const Segment& segment) {
            assert(this->offset + segment.size() <= this->buffer.size());

            segment.encode_to(this->buffer, this->offset);
        }
    };
}