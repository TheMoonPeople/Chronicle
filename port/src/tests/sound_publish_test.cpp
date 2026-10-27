// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include "audio/hdbank.hpp"
#include "audio/sq.hpp"
#include "dataread.hpp"
#include "platform/paths.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "sound_data.hpp"

namespace dc::test {

using namespace dc::game;

namespace {

namespace fs = std::filesystem;
namespace sound = dcdata::sound;

std::string ReadText(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::span<const std::uint8_t> Bytes(const dcdata::PackMember &member) {
    return member.data;
}

void ExpectSameBank(const audio::Bank &raw, const audio::Bank &published, const std::string &where) {
    const audio::HdBank &a = raw.Header();
    const audio::HdBank &b = published.Header();
    ASSERT_EQ(a.programs.size(), b.programs.size()) << where;
    for (std::size_t p = 0; p < a.programs.size(); p++) {
        const audio::HdProgram &x = a.programs[p];
        const audio::HdProgram &y = b.programs[p];
        ASSERT_EQ(x.present, y.present) << where << " program " << p;
        EXPECT_EQ(std::tie(x.volume, x.pan, x.transpose, x.detune), std::tie(y.volume, y.pan, y.transpose, y.detune)) << where << " program " << p;
        ASSERT_EQ(x.splits.size(), y.splits.size()) << where << " program " << p;
        for (std::size_t s = 0; s < x.splits.size(); s++) {
            const audio::HdSplit &i = x.splits[s];
            const audio::HdSplit &j = y.splits[s];
            EXPECT_EQ(std::tie(i.sample_set, i.key_low, i.key_high, i.bend_low, i.bend_high, i.volume, i.pan, i.transpose, i.detune),
                      std::tie(j.sample_set, j.key_low, j.key_high, j.bend_low, j.bend_high, j.volume, j.pan, j.transpose, j.detune))
                << where << " program " << p << " split " << s;
        }
    }
    ASSERT_EQ(a.samples.size(), b.samples.size()) << where;
    for (std::size_t s = 0; s < a.samples.size(); s++) {
        const audio::HdSample &i = a.samples[s];
        const audio::HdSample &j = b.samples[s];
        EXPECT_EQ(std::tie(i.vag, i.vel_low, i.vel_high, i.base_note, i.detune, i.pan, i.volume, i.adsr1, i.adsr2, i.spu_attr),
                  std::tie(j.vag, j.vel_low, j.vel_high, j.base_note, j.detune, j.pan, j.volume, j.adsr1, j.adsr2, j.spu_attr))
            << where << " sample " << s;
    }
    // What the synth plays: every layer of every key-on, its sample's PCM and loop included.
    std::array<audio::Layer, 16> x{};
    std::array<audio::Layer, 16> y{};
    for (int program = 0; program < static_cast<int>(a.programs.size()); program++) {
        for (int note = 0; note < 128; note++) {
            for (const int velocity : {1, 64, 127}) {
                const int count = raw.Resolve(program, note, velocity, x);
                ASSERT_EQ(count, published.Resolve(program, note, velocity, y)) << where << " program " << program << " note " << note;
                for (int l = 0; l < count; l++) {
                    EXPECT_EQ(x[l].rate, y[l].rate) << where;
                    EXPECT_EQ(x[l].vel_curve, y[l].vel_curve) << where;
                    ASSERT_EQ(x[l].vag == nullptr, y[l].vag == nullptr) << where;
                    if (x[l].vag != nullptr) {
                        EXPECT_EQ(x[l].vag->pcm, y[l].vag->pcm) << where << " program " << program << " note " << note;
                        EXPECT_EQ(x[l].vag->loop_start, y[l].vag->loop_start) << where;
                        EXPECT_EQ(x[l].vag->loops, y[l].vag->loops) << where;
                    }
                }
            }
        }
    }
}

void ExpectSameSequence(const audio::SqFile &raw, const audio::SqFile &published, const std::string &where) {
    ASSERT_EQ(raw.Songs().size(), published.Songs().size()) << where;
    for (std::size_t s = 0; s < raw.Songs().size(); s++) {
        EXPECT_EQ(raw.Songs()[s].division, published.Songs()[s].division) << where;
        const auto x = raw.Events(raw.Songs()[s]);
        const auto y = published.Events(published.Songs()[s]);
        ASSERT_EQ(x.size(), y.size()) << where << " song " << s;
        for (std::size_t e = 0; e < x.size(); e++) {
            EXPECT_EQ(std::tie(x[e].delta, x[e].kind, x[e].status, x[e].data[0], x[e].data[1]),
                      std::tie(y[e].delta, y[e].kind, y[e].status, y[e].data[0], y[e].data[1]))
                << where << " song " << s << " event " << e;
            // A track's end stops playback whatever meta number it carries; the published document
            // keeps none, the raw reader its 0x2F.
            if (x[e].kind == audio::SqEventKind::Meta) {
                EXPECT_EQ(x[e].meta, y[e].meta) << where << " song " << s << " event " << e;
            }
            EXPECT_TRUE(std::ranges::equal(x[e].payload, y[e].payload)) << where << " song " << s << " event " << e;
        }
    }
}

} // namespace

TEST(SoundPublish, ALoadListNamesBanksAndSequencesByTheirFirstNineCharacters) {
    const auto entries = sound::ReadLoadList("e01a_c.hd\r\n.sq\nshort\ne01a_e.sq trailing\nnotes.txt\n\no01b_a.hd");
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].member, "e01a_c.hd");
    EXPECT_EQ(entries[0].slot, 'c');
    EXPECT_EQ(entries[0].kind, sound::LoadKind::Bank);
    EXPECT_EQ(entries[1].member, "e01a_e.sq");
    EXPECT_EQ(entries[1].kind, sound::LoadKind::Sequence);
    EXPECT_EQ(entries[2].member, "o01b_a.hd");
    EXPECT_EQ(entries[2].slot, 'a');
}

TEST(SoundPublish, APackConfigReadsItsTwoCommandsAndLeavesTheRestZero) {
    const auto both = sound::ReadPackConfig("REVERBE 2,3\nTABLE 1, 4\n\n");
    EXPECT_EQ(both.reverb_mode, 2);
    EXPECT_EQ(both.reverb_depth, 3);
    EXPECT_EQ(both.se_table, 1);
    EXPECT_EQ(both.se_table_type, 4);
    const auto reverb = sound::ReadPackConfig("REVERBE 4,30\n");
    EXPECT_EQ(reverb.reverb_mode, 4);
    EXPECT_EQ(reverb.reverb_depth, 30);
    EXPECT_EQ(reverb.se_table, 0);
    EXPECT_EQ(sound::ReadPackConfig("").reverb_mode, 0);
}

TEST(SoundPublish, TheGlobalTablesSkipTheirHeaderAndReadAsTheGameDoes) {
    const auto sequences = sound::ReadSequenceVolumes("filename vol\nb01a_a.sq\t94\nlongername1.sq 12;\n");
    ASSERT_EQ(sequences.size(), 2u);
    EXPECT_EQ(sequences[0].name, "b01a_a.sq");
    EXPECT_EQ(sequences[0].volume, 94);
    EXPECT_EQ(sequences[1].name, "longernam");
    const auto effects = sound::ReadEffectVolumes("PRG KEY VOL\n16\t20\t38\n123456,7,8\n");
    ASSERT_EQ(effects.size(), 2u);
    EXPECT_EQ(effects[0].bank, 16);
    EXPECT_EQ(effects[0].program, 20);
    EXPECT_EQ(effects[0].volume, 38);
    EXPECT_EQ(effects[1].bank, 1234);
}

TEST(SoundPublish, SharedBanksAreNamedForTheirMemberAndContents) {
    const std::array<unsigned char, 3> hd = {1, 2, 3};
    const std::array<unsigned char, 2> bd = {4, 5};
    const std::array<unsigned char, 2> other = {4, 6};
    const std::string                  key = sound::SharedBankKey("E01A_C.hd", hd, bd);
    EXPECT_TRUE(key.starts_with("sound/banks/e01a_c-"));
    EXPECT_TRUE(key.ends_with(".hd"));
    EXPECT_EQ(key, sound::SharedBankKey("E01A_C.hd", hd, bd));
    EXPECT_NE(key, sound::SharedBankKey("E01A_C.hd", hd, other));
    EXPECT_TRUE(sound::SharedBankKey(".hd", hd, bd).starts_with("sound/banks/unnamed-"));
    EXPECT_TRUE(sound::SharedBankKey("\x81\x9b.hd", hd, bd).starts_with("sound/banks/__-"));
}

TEST(SoundPublish, AMemberIsTheFirstWhoseNameMatchesIgnoringCase) {
    const std::vector<dcdata::PackMember> members = {
        {"s04a_e.sq", {1}},
        {"S04A_E.SQ", {2}},
    };
    const dcdata::PackMember *found = sound::FindMember(members, "S04a_e.sq");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->data[0], 1);
    EXPECT_EQ(sound::FindMember(members, "missing.sq"), nullptr);
}

// DC_DATA is a data folder `dcdata publish-sound` has published sound into.
TEST(SoundPublishRealData, EveryPublishedPackPlaysWhatTheOriginalsDo) {
    const char *env = std::getenv("DC_DATA");
    if (env == nullptr || *env == '\0') {
        GTEST_SKIP() << "DC_DATA is not set";
    }
    const fs::path        data = env;
    std::vector<fs::path> documents;
    for (const auto &entry : fs::recursive_directory_iterator(data)) {
        if (entry.is_regular_file() && entry.path().filename().string().ends_with(".sound.json")) {
            documents.push_back(entry.path());
        }
    }
    if (documents.empty()) {
        GTEST_SKIP() << "no published sound in DC_DATA; run dcdata publish-sound";
    }
    const auto resolve = [&](const fs::path &path) { return data / path; };
    int        banks = 0;
    int        sequences = 0;
    int        configs = 0;
    for (const fs::path &document : documents) {
        fs::path pack_path = document;
        pack_path.replace_extension().replace_extension(".snd");
        const auto members = dcdata::ReadPack(dcdata::ReadFile(pack_path));
        const auto json = nlohmann::json::parse(ReadText(document));
        ASSERT_EQ(json["kind"], "sound_pack") << document;
        for (const auto &list : json["lists"]) {
            const std::string name = list["name"];
            for (const auto &load : list["load"]) {
                const std::string         member = load["member"];
                const std::string         where = document.string() + " " + member;
                const dcdata::PackMember *source = sound::FindMember(members, member);
                ASSERT_NE(source, nullptr) << where;
                if (load.contains("bank")) {
                    const dcdata::PackMember *bd = sound::FindMember(members, member.substr(0, 7) + "bd");
                    ASSERT_NE(bd, nullptr) << where;
                    const auto raw = audio::Bank::Create(Bytes(*source), Bytes(*bd));
                    const auto published = audio::Bank::CreateFromJson(ReadText(data / load["bank"].get<std::string>()), resolve);
                    ASSERT_NE(raw, nullptr) << where;
                    ASSERT_NE(published, nullptr) << where;
                    ExpectSameBank(*raw, *published, where);
                    banks++;
                } else {
                    const auto raw = audio::SqFile::Create(Bytes(*source));
                    const auto published = audio::SqFile::CreateFromJson(ReadText(data / load["sequence"].get<std::string>()));
                    ASSERT_NE(raw, nullptr) << where;
                    ASSERT_NE(published, nullptr) << where;
                    ExpectSameSequence(*raw, *published, where);
                    sequences++;
                }
            }
            const dcdata::PackMember *cfg = sound::FindMember(members, name.substr(0, name.find('.')) + ".cfg");
            ASSERT_EQ(cfg != nullptr, !list["config"].is_null()) << document << " " << name;
            if (cfg != nullptr) {
                std::vector<char> script(cfg->data.begin(), cfg->data.end());
                SND_INFO          info{};
                LoadSoundInfo(&info, script.data(), static_cast<int>(script.size()));
                EXPECT_EQ(list["config"]["reverb_mode"], info.reverb_mode) << document << " " << name;
                EXPECT_EQ(list["config"]["reverb_depth"], info.reverb_depth) << document << " " << name;
                EXPECT_EQ(list["config"]["se_table"], info.se_table) << document << " " << name;
                EXPECT_EQ(list["config"]["se_table_type"], info.se_table_type) << document << " " << name;
                configs++;
            }
        }
    }
    EXPECT_GT(banks, 0);
    EXPECT_GT(sequences, 0);
    EXPECT_GT(configs, 0);
    std::printf("checked %d bank loads, %d sequence loads and %d .cfg files in %zu packs\n", banks, sequences, configs, documents.size());

    // The sequence volumes' reader has no accessor; the table test above covers ReadSequenceVolumes.
    if (!fs::exists(data / "data.hd2")) {
        return;
    }
    PathsSetDataRoot(data);
    InitCDFile();
    std::vector<unsigned int> buffer(1 << 20);
    CSnd.LoadSeInf(const_cast<char *>("sound/tbl/setbl.txt"), buffer.data());
    const auto *effects = reinterpret_cast<const SE_INF_TABLE *>(CSnd.GetSeInfTbl());
    const auto  effect_table = nlohmann::json::parse(ReadText(data / "sound/tbl/setbl.json"));
    ASSERT_EQ(effect_table["rows"].size(), static_cast<std::size_t>(effects->count));
    for (int i = 0; i < effects->count; i++) {
        EXPECT_EQ(effect_table["rows"][i]["bank"], effects->entry[i].bank) << i;
        EXPECT_EQ(effect_table["rows"][i]["program"], effects->entry[i].program) << i;
        EXPECT_EQ(effect_table["rows"][i]["volume"], effects->entry[i].volume) << i;
    }
}

} // namespace dc::test
