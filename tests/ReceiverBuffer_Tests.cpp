#include <gtest/gtest.h>
#include "imkcpp.hpp"

class ReceiverBufferTest : public ::testing::Test {
protected:
    imkcpp::ReceiverBuffer buffer;
};

TEST_F(ReceiverBufferTest, QueueLimit) {
    using namespace imkcpp;

    buffer.set_queue_limit(10);
    ASSERT_EQ(buffer.get_queue_limit(), 10);

    for (u32 i = 0; i < 20; i++) {
        SegmentData data{};
        buffer.emplace_segment(SegmentHeader{ .sn = i }, data);
    }

    // ASSERT_EQ(buffer.size(), 10); // TODO: Does receiver have to enforce queue limit?
}

TEST_F(ReceiverBufferTest, InitiallyEmpty) {
    ASSERT_TRUE(buffer.empty());
}

TEST_F(ReceiverBufferTest, AddAndCheckFront) {
    using namespace imkcpp;

    constexpr SegmentHeader header{ .sn = 1 };
    SegmentData data{};
    buffer.emplace_segment(header, data);

    ASSERT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.front().header.sn, header.sn);
}

// Test pop_front
TEST_F(ReceiverBufferTest, PopFront) {
    using namespace imkcpp;

    constexpr SegmentHeader header{ .sn = 1 };
    SegmentData data{};
    buffer.emplace_segment(header, data);

    buffer.pop_front();
    ASSERT_TRUE(buffer.empty());
}

TEST_F(ReceiverBufferTest, SegmentOrder) {
    using namespace imkcpp;

    constexpr SegmentHeader header1 { .sn = 0 }, header2{ .sn = 1 };
    SegmentData data1{}, data2{};

    buffer.emplace_segment(header1, data1);
    buffer.emplace_segment(header2, data2);

    ASSERT_EQ(buffer.front().header.sn, header1.sn);
    buffer.pop_front();
    ASSERT_EQ(buffer.front().header.sn, header2.sn);
}

TEST_F(ReceiverBufferTest, NoDuplicateSegments) {
    using namespace imkcpp;

    constexpr SegmentHeader header{ .sn = 1 };
    SegmentData data{};

    buffer.emplace_segment(header, data);
    buffer.emplace_segment(header, data);

    ASSERT_EQ(buffer.size(), 1);
}