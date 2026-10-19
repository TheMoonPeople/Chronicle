#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "audio_retail.hpp"
#include "audio_retail_fixture.hpp"

namespace ca = chronicle_audio;
namespace fx = ca::fixture;

TEST(RetailAudio, BankKeepsPcmLoopPitchAndProgramBounds) {
    const auto hd = fx::MakeHd({.programs = 2, .key_low = 30, .key_high = 90, .base_note = 64, .rate = 32000});
    const auto bd = fx::SquareVag(2);
    auto       bank = ca::CreateBank(hd, bd);
    ASSERT_NE(bank, nullptr);
    std::array<audio::Layer, 8> layers;
    EXPECT_EQ(bank->Resolve(0, 29, 100, layers), 0);
    ASSERT_EQ(bank->Resolve(1, 64, 100, layers), 1);
    const auto &l = layers[0];
    EXPECT_EQ(l.rate, 32000);
    EXPECT_EQ(l.sample->base_note, 64);
    EXPECT_EQ(l.split->bend_low, 256);
    EXPECT_EQ(l.vag->loop_start, 0);
    EXPECT_TRUE(l.vag->loops);
    ASSERT_EQ(l.vag->pcm.size(), 56u);
    for (int i = 0; i < 56; ++i) {
        EXPECT_EQ(l.vag->pcm[i], i < 28 ? 28672 : -32768);
    }
    EXPECT_EQ(bank->Resolve(2, 64, 100, layers), 0);
}

TEST(RetailAudio, SequenceNormalizesCompressedDeltasRunningStatusAndPayloads) {
    auto sq = fx::MakeSq(60, 90, 240);
    auto sequence = ca::CreateSequence(sq);
    ASSERT_NE(sequence, nullptr);
    ASSERT_EQ(sequence->Songs().size(), 1u);
    EXPECT_EQ(sequence->Songs()[0].division, 480);
    auto events = sequence->Events(sequence->Songs()[0]);
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[1].data[0], 60);
    EXPECT_EQ(events[2].delta, 240u);
    EXPECT_EQ(events[2].status, 0x80);
    EXPECT_EQ(events[3].kind, audio::SqEventKind::End);
    std::vector<std::uint8_t> raw = {
        0, 0x90, 60, 0xC0, 61, 65,        // next note uses running status, with omitted delta
        12, 0xFF, 0x51, 3, 7, 0xA1, 0x20, // tempo
        0, 0xF0, 3, 1, 2, 3,
        0, 0xFF, 0x2F, 0};
    sq.resize(26);
    sq.insert(sq.end(), raw.begin(), raw.end());
    fx::Put(sq, 8, sq.size(), 4);
    sequence = ca::CreateSequence(sq);
    ASSERT_NE(sequence, nullptr);
    events = sequence->Events(sequence->Songs()[0]);
    ASSERT_EQ(events.size(), 5u);
    EXPECT_EQ(events[0].data[1], 64);
    EXPECT_EQ(events[1].delta, 0u);
    EXPECT_EQ(events[1].status, 0x90);
    EXPECT_EQ(events[1].data[0], 61);
    EXPECT_EQ(events[2].delta, 12u);
    EXPECT_EQ(events[2].meta, 0x51);
    EXPECT_EQ(events[2].payload[1], 0xA1);
    EXPECT_EQ(events[3].kind, audio::SqEventKind::SysEx);
    EXPECT_EQ(events[3].payload[2], 3);
    audio::SqReader reader(events);
    reader.Next();
    const auto position = reader.Save();
    const auto first = reader.Next();
    reader.Restore(position);
    EXPECT_EQ(reader.Next().data[0], first.data[0]);
}

TEST(RetailAudio, RejectsShortHeadersAndToleratesTruncatedTracks) {
    const auto hd = fx::MakeHd({});
    const auto bd = fx::SquareVag(2);
    const auto sq = fx::MakeSq(60, 90, 240);
    for (std::size_t size = 0; size < 16; ++size) {
        EXPECT_EQ(ca::CreateBank(std::span(hd).first(size), bd), nullptr);
        EXPECT_EQ(ca::CreateSequence(std::span(sq).first(size)), nullptr);
    }
    for (std::size_t size = 16; size < sq.size(); ++size) {
        EXPECT_NO_THROW(ca::CreateSequence(std::span(sq).first(size)));
    }
}

TEST(RetailAudio, DiscSequencesAndBankSamplesSurviveConversion) {
    const char *data = std::getenv("DC_DATA");
    if (!data) {
        GTEST_SKIP() << "Set DC_DATA to extracted retail data";
    }
    auto read = [](const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
    };
    int sequences = 0, banks = 0, samples = 0;
    for (const auto &file : std::filesystem::recursive_directory_iterator(data)) {
        if (!file.is_regular_file()) {
            continue;
        }
        auto ext = file.path().extension().string();
        std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ext == ".sq") {
            const auto bytes = read(file.path());
            const auto original = ca::retail::SqFile::Create(bytes);
            const auto converted = ca::CreateSequence(bytes);
            ASSERT_NE(original, nullptr) << file.path();
            ASSERT_NE(converted, nullptr) << file.path();
            ASSERT_EQ(original->Songs().size(), converted->Songs().size());
            for (std::size_t i = 0; i < original->Songs().size(); ++i) {
                const auto &song = original->Songs()[i];
                EXPECT_EQ(song.division, converted->Songs()[i].division);
                ca::retail::SqReader old(original->Events(song));
                const auto           events = converted->Events(converted->Songs()[i]);
                for (const auto &event : events) {
                    const auto expected = old.Next();
                    EXPECT_EQ(event.delta, expected.delta);
                    EXPECT_EQ(int(event.kind), int(expected.kind));
                    EXPECT_EQ(event.status, expected.status);
                    if (event.kind == audio::SqEventKind::Meta) {
                        EXPECT_EQ(event.meta, expected.meta);
                    }
                    EXPECT_EQ(event.data[0], expected.data[0]);
                    EXPECT_EQ(event.data[1], expected.data[1]);
                    EXPECT_TRUE(std::ranges::equal(event.payload, expected.payload));
                }
            }
            ++sequences;
        } else if (ext == ".hd") {
            auto bdpath = file.path();
            bdpath.replace_extension(".bd");
            if (!std::filesystem::exists(bdpath)) {
                bdpath.replace_extension(".BD");
            }
            if (!std::filesystem::exists(bdpath)) {
                continue;
            }
            const auto hd = read(file.path()), bd = read(bdpath);
            const auto original = ca::retail::HdBank::Parse(hd);
            const auto converted = ca::CreateBank(hd, bd);
            ASSERT_TRUE(original) << file.path();
            ASSERT_NE(converted, nullptr) << file.path();
            ASSERT_EQ(original->programs.size(), converted->Header().programs.size());
            std::vector<bool> checked(original->vags.size());
            for (std::size_t program = 0; program < original->programs.size(); ++program) {
                for (std::size_t split = 0; split < original->programs[program].splits.size(); ++split) {
                    for (int velocity : {1, 64, 127}) {
                        std::array<audio::Layer, 128> layers;
                        const int                     count = converted->ResolveSplit(program, split, velocity, layers);
                        for (int n = 0; n < count; ++n) {
                            const auto &layer = layers[n];
                            const int   i = layer.sample->vag;
                            if (checked[i]) {
                                continue;
                            }
                            checked[i] = true;
                            const auto &v = original->vags[i];
                            const auto  expected = ca::retail::DecodeVag(std::span(bd).subspan(v.offset));
                            EXPECT_EQ(layer.rate, v.rate);
                            EXPECT_EQ(layer.vag->pcm, expected.pcm) << file.path();
                            EXPECT_EQ(layer.vag->loop_start, expected.loop_start);
                            EXPECT_EQ(layer.vag->loops, expected.loops);
                            ++samples;
                        }
                    }
                }
            }
            ++banks;
        }
    }
    EXPECT_GT(sequences, 0);
    EXPECT_GT(banks, 0);
    EXPECT_GT(samples, 0);
    std::printf("Retail audio: %d banks, %d sequences, %d samples verified\n", banks, sequences, samples);
}
