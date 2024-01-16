#pragma once

#include <array>
#include <cassert>
#include "types.hpp"
#include "segment.hpp"
#include "utility.hpp"

namespace imkcpp {
    template <size_t MTU>
    class Flusher final {
        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        std::array<std::byte, MTU> buffer{};
        size_t offset = 0;

        /// Flushes the buffer to the given output
        [[nodiscard]] size_t flush(const output_callback_t& callback) {
            const auto size = this->offset;

            assert(size <= this->buffer.size());

            callback({this->buffer.data(), size});
            this->offset = 0;

            return size;
        }

    public:
        /// Returns true if the buffer is empty.
        [[nodiscard]] bool is_empty() const {
            return this->offset == 0;
        }

        /// Flushes the buffer to the given output if it exceeds Max Segment Size
        [[nodiscard]] size_t flush_if_full(const output_callback_t& target) {
            if (this->offset > MAX_SEGMENT_SIZE) {
                return this->flush(target);
            }

            return 0;
        }

        /// Flushes the buffer to the given output if adding "size" bytes to the buffer would exceed Max Segment Size
        [[nodiscard]] size_t flush_if_does_not_fit(const output_callback_t& target, const size_t size) {
            if (this->offset + size > MAX_SEGMENT_SIZE) {
                return this->flush(target);
            }

            return 0;
        }

        /// Flushes the buffer to the given output if it's not empty
        [[nodiscard]] size_t flush_if_not_empty(const output_callback_t& target) {
            if (!this->is_empty()) {
                return this->flush(target);
            }

            return 0;
        }

        /// Emplaces the given segment into the buffer
        void emplace_segment(const Segment& segment) {
            assert(this->offset + segment.size() <= this->buffer.size());

            segment.encode_to(this->buffer, this->offset);
        }
    };
}