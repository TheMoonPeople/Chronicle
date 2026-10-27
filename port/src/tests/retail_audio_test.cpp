#include <gtest/gtest.h>

#include "audio/hdbank.hpp"
#include "audio/sq.hpp"
#include "audio/vag.hpp"

namespace dc::test {

TEST(RetailAudio, SequenceConversionPreservesRunningDeltasAndNoteOff) {
    std::vector<uint8_t> bytes(0x1a);
    const auto           put = [&](size_t at, uint32_t value, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            bytes[at + i] = static_cast<uint8_t>(value >> (8 * i));
        }
    };
    put(0, audio::kChunkCreator, 4);
    put(4, audio::kChunkMidi, 4);
    put(8, 0x24, 4);
    put(0x10, 0x14, 4);
    put(0x14, 6, 4);
    put(0x18, 480, 2);
    bytes.insert(bytes.end(), {0, 0x90, 60, 0xe4, 0x80, 60, 0, 0xff, 0x2f, 0});
    const auto sequence = audio::SqFile::Create(bytes);
    ASSERT_NE(sequence, nullptr);
    ASSERT_EQ(sequence->Songs().size(), 1u);
    EXPECT_EQ(sequence->Songs()[0].division, 480);
    audio::SqReader reader(sequence->Events(sequence->Songs()[0]));
    auto            event = reader.Next();
    EXPECT_EQ(event.status, 0x90);
    EXPECT_EQ(event.data[1], 100);
    event = reader.Next();
    EXPECT_EQ(event.status, 0x80);
    EXPECT_EQ(event.data[0], 60);
    EXPECT_EQ(event.delta, 0u);
    EXPECT_EQ(reader.Next().kind, audio::SqEventKind::End);
}

TEST(RetailAudio, RawSamplesKeepTheSpuLoopBoundary) {
    std::vector<uint8_t> bytes(32, 0x77);
    bytes[0] = bytes[16] = 0;
    bytes[1] = audio::kVagLoopStart;
    bytes[17] = audio::kVagLoopEnd | audio::kVagLoopRepeat;
    const auto sample = audio::DecodeVag(bytes);
    EXPECT_EQ(sample.pcm.size(), 56u);
    EXPECT_TRUE(sample.loops);
    EXPECT_EQ(sample.loop_start, 0);
    EXPECT_EQ(sample.pcm[0], 28672);
}

} // namespace dc::test
