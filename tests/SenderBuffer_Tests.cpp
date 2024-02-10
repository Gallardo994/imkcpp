#include <gtest/gtest.h>
#include "imkcpp.hpp"

class SenderBufferTest : public ::testing::Test {
protected:
    imkcpp::SenderBuffer buffer;
};

TEST_F(SenderBufferTest, InitiallyEmpty) {
    ASSERT_TRUE(buffer.empty());
}

TEST_F(SenderBufferTest, PushSegment) {
    using namespace imkcpp;

    auto segment_data = SegmentData{};
    Segment segment{ SegmentHeader{ .sn = 1 }, segment_data };
    buffer.push_segment(segment);

    ASSERT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.get_first_sequence_number_in_flight(), 1);
}

TEST_F(SenderBufferTest, Erase) {
    using namespace imkcpp;

    auto segment_data = SegmentData{};
    Segment segment{ SegmentHeader{ .sn = 2 }, segment_data };
    buffer.push_segment(segment);

    buffer.erase(2);
    ASSERT_TRUE(buffer.empty());
}

TEST_F(SenderBufferTest, EraseBefore) {
    using namespace imkcpp;

    SegmentData data1{}, data2{}, data3{};

    Segment segment1{ SegmentHeader{ .sn = 2 }, data1 };
    Segment segment2{ SegmentHeader{ .sn = 3 }, data2 };
    Segment segment3{ SegmentHeader{ .sn = 4 }, data3 };

    buffer.push_segment(segment1);
    buffer.push_segment(segment2);
    buffer.push_segment(segment3);

    buffer.erase_before(3);

    ASSERT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.get_first_sequence_number_in_flight(), 3);
    ASSERT_EQ(buffer.size(), 2);

    buffer.erase_before(4);
    ASSERT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.get_first_sequence_number_in_flight(), 4);

    buffer.erase_before(5);
    ASSERT_TRUE(buffer.empty());
    ASSERT_EQ(buffer.get_first_sequence_number_in_flight(), std::nullopt);
}

TEST_F(SenderBufferTest, IncrementFastackBefore) {
    using namespace imkcpp;

    SegmentData data1{}, data2{}, data3{};

    Segment segment1{ SegmentHeader{ .sn = 2 }, data1 };
    Segment segment2{ SegmentHeader{ .sn = 3 }, data2 };
    Segment segment3{ SegmentHeader{ .sn = 4 }, data3 };

    buffer.push_segment(segment1);
    buffer.push_segment(segment2);
    buffer.push_segment(segment3);

    buffer.increment_fastack_before(4);

    ASSERT_EQ(buffer.begin()->metadata.fastack, 1);
    ASSERT_EQ((buffer.begin() + 1)->metadata.fastack, 1);
    ASSERT_EQ((buffer.begin() + 2)->metadata.fastack, 0);
}

TEST_F(SenderBufferTest, GetEarliestTransmitDelta) {
    using namespace imkcpp;

    SegmentData data1{}, data2{}, data3{};

    const auto now = std::chrono::steady_clock::now();

    Segment segment1{ SegmentHeader{ .sn = 2 }, data1 };
    segment1.metadata.resendts = now + 100ms;
    Segment segment2{ SegmentHeader{ .sn = 3 }, data2 };
    segment2.metadata.resendts = now + 200ms;
    Segment segment3{ SegmentHeader{ .sn = 4 }, data3 };
    segment3.metadata.resendts = now + 300ms;

    buffer.push_segment(segment1);
    buffer.push_segment(segment2);
    buffer.push_segment(segment3);

    const auto earliest_delta = buffer.get_earliest_transmit_delta(now + 10ms);
    ASSERT_TRUE(earliest_delta.has_value());
    ASSERT_EQ(earliest_delta.value(), 90ms);
}