#pragma once

#include <deque>
#include "types.hpp"
#include "segment.hpp"

namespace imkcpp {
    class SenderBuffer {
        SharedCtx& shared_ctx;
        std::deque<Segment> snd_buf{};

    public:
        explicit SenderBuffer(SharedCtx& shared_ctx) : shared_ctx(shared_ctx) {}

        void push_segment(Segment& segment) {
            snd_buf.push_back(std::move(segment));
        }

        void shrink() {
            if (!this->snd_buf.empty()) {
                const Segment& seg = this->snd_buf.front();
                shared_ctx.snd_una = seg.header.sn;
            } else {
                shared_ctx.snd_una = shared_ctx.snd_nxt;
            }
        }

        void erase(const u32 sn) {
            for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (sn == it->header.sn) {
                    this->snd_buf.erase(it);
                    break;
                }

                if (sn < it->header.sn) {
                    break;
                }

                ++it;
            }
        }

        void erase_before(const u32 sn) {
            for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (sn > it->header.sn) {
                    it = this->snd_buf.erase(it);
                } else {
                    break;
                }
            }
        }

        [[nodiscard]] bool empty() const {
            return this->snd_buf.empty();
        }

        std::deque<Segment>& get() {
            return this->snd_buf;
        }
    };
}