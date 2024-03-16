#include <gtest/gtest.h>
#include "imkcpp.hpp"

class FlusherTest : public testing::Test {
protected:
    static constexpr size_t MTU = 1500;
    static constexpr size_t MSS = imkcpp::MTU_TO_MSS<MTU>();

    imkcpp::Flusher<MTU> flusher;

    size_t callback_invocations = 0;
    size_t last_flush_size = 0;

    imkcpp::output_callback_t mock_callback = [this](std::span<const std::byte> data) {
        ++callback_invocations;
        last_flush_size = data.size();
    };

    static imkcpp::Segment create_mock_segment(const size_t data_size) {
        imkcpp::Segment segment;

        std::vector<std::byte> data(data_size);
        for (size_t i = 0; i < data_size; ++i) {
            data[i] = static_cast<std::byte>(i % 256);
        }
        segment.data_assign(data);

        return segment;
    };
};

TEST_F(FlusherTest, IsEmptyInitially) {
    ASSERT_TRUE(flusher.is_empty());
}

TEST_F(FlusherTest, FlushIfFull) {
    using namespace imkcpp;

    const Segment segment = create_mock_segment(MSS - serializer::fixed_size<SegmentHeader>() - 1);
    flusher.emplace(segment.header, segment.data);
    ASSERT_EQ(flusher.flush_if_full(mock_callback), 0);
    ASSERT_EQ(callback_invocations, 0);

    const Segment segment2 = create_mock_segment(1);
    flusher.emplace(segment2.header, segment2.data);
    ASSERT_EQ(flusher.flush_if_full(mock_callback), MTU);
    ASSERT_EQ(callback_invocations, 1);
}

TEST_F(FlusherTest, FlushIfDoesNotFit) {
    using namespace imkcpp;

    constexpr size_t segment_size = MSS / 2;

    const Segment segment = create_mock_segment(segment_size);
    flusher.emplace(segment.header, segment.data);
    ASSERT_EQ(flusher.flush_if_does_not_fit(mock_callback, segment_size + 1), segment_size + serializer::fixed_size<SegmentHeader>());
    ASSERT_EQ(callback_invocations, 1);

    ASSERT_EQ(flusher.flush_if_does_not_fit(mock_callback, segment_size), 0);
    ASSERT_EQ(callback_invocations, 1);
}

TEST_F(FlusherTest, FlushIfNotEmpty) {
    using namespace imkcpp;

    const Segment segment = create_mock_segment(1);
    flusher.emplace(segment.header, segment.data);
    ASSERT_EQ(flusher.flush_if_not_empty(mock_callback), serializer::fixed_size<SegmentHeader>() + 1);
    ASSERT_EQ(callback_invocations, 1);

    ASSERT_EQ(flusher.flush_if_not_empty(mock_callback), 0);
    ASSERT_EQ(callback_invocations, 1);
}