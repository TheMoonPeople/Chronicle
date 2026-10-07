// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "clsmes.hpp"
#include "data_fixture.hpp"
#include "gametext.hpp"
#include "localize.hpp"
#include "mainselect.hpp"

namespace {

namespace fs = std::filesystem;

// A retail-shaped message file of the given texts, as the loaders hand it over.
std::vector<s16> File(const std::map<int, std::string> &texts) {
    GameTextFile file;
    for (const auto &[id, text] : texts) {
        EXPECT_EQ(file.Set(id, text), 0);
    }
    const s16 *data = file.Data();
    // The last message by offset ends the file.
    size_t end = 2 + static_cast<size_t>(data[0]) * 2;
    for (int i = 0; i < data[0]; i++) {
        size_t at = 1 + static_cast<size_t>(data[0]) + static_cast<size_t>(data[3 + i * 2]);
        while (data[at] != MES_CODE_END) {
            at++;
        }
        end = std::max(end, at + 1);
    }
    return std::vector<s16>(data, data + end);
}

std::string Text(std::vector<s16> &file, int id) {
    ClsMes mes;
    mes.SetBuff(file.data());
    const s16 *top = mes.GetTextLineDataTop(id);
    return top == nullptr ? "<none>" : GameTextDecode(top);
}

void Write(const fs::path &path, const std::string &text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

struct LangDir {
    fs::path dir = datafix::TempDir("localize");

    LangDir() {
        LocalizeReset();
        LocalizeSetDirectories({dir});
        LanguageCode = 3;
    }

    ~LangDir() {
        LocalizeReset();
        LocalizeSetDirectories({});
        std::error_code error;
        fs::remove_all(dir, error);
    }
};

// A buffer as LoadFile2 leaves one: the file, then zeros to whole sectors.
struct Loaded {
    alignas(64) unsigned char bytes[4096] = {};
    int size = 0;

    explicit Loaded(const std::vector<s16> &file) {
        size = static_cast<int>(file.size() * 2);
        std::memcpy(bytes, file.data(), static_cast<size_t>(size));
    }
};

} // namespace

TEST(Localize, ParsesTheFormat) {
    LocalizeTable table;
    std::string   error;
    ASSERT_TRUE(LocalizeParse(R"({"dun.script.d01.d01.0": "a", "dun.script.d01.d01.-5": "b{{", "meswin.system14.12": "c"})",
                              table, error))
        << error;
    ASSERT_EQ(table.files.count("dun.script.d01.d01"), 1U);
    EXPECT_EQ(table.files["dun.script.d01.d01"].at(0), "a");
    EXPECT_EQ(table.files["dun.script.d01.d01"].at(-5), "b{{");
    EXPECT_EQ(table.files["meswin.system14"].at(12), "c");
}

TEST(Localize, MessageKeysAreTheSameInEveryLanguage) {
    EXPECT_EQ(LocalizeMessageKey("dun/script/d01/d01_3.mes"), "dun.script.d01.d01");
    EXPECT_EQ(LocalizeMessageKey("dun/script/d01/d01_6.mes"), "dun.script.d01.d01");
    EXPECT_EQ(LocalizeMessageKey("cdrom0:\\DUN\\SCRIPT\\D01\\D01_3.MES"), "dun.script.d01.d01");
    EXPECT_EQ(LocalizeMessageKey("dun/message/ww_mes/steve07_1_4.mes"), "dun.message.ww_mes.steve07_1");
    EXPECT_EQ(LocalizeMessageKey("commenu/a_fre/dungeon/dunmenu5.pak/allmenu.mes"), "commenu.dungeon.dunmenu5.pak.allmenu");
    EXPECT_EQ(LocalizeMessageKey("commenu/a_eng/dungeon/dunmenu5.pak/allmenu.mes"), "commenu.dungeon.dunmenu5.pak.allmenu");
    EXPECT_EQ(LocalizeMessageKey("meswin/system14_2.mes"), "meswin.system14");
    // A file that names no language keeps its name.
    EXPECT_EQ(LocalizeMessageKey("noda_w/dun00_eng.mes"), "noda_w.dun00_eng");
    EXPECT_EQ(LocalizeMessageKey("commenu/emenu.pak/allmenu.mes"), "commenu.emenu.pak.allmenu");
}

TEST(Localize, RejectsBadJson) {
    LocalizeTable table;
    std::string   error;
    EXPECT_FALSE(LocalizeParse("{", table, error));
    EXPECT_FALSE(LocalizeParse("[]", table, error));
    EXPECT_FALSE(LocalizeParse(R"({"a.b.40000": "t"})", table, error));
    EXPECT_NE(error.find("not a message key"), std::string::npos);
    EXPECT_FALSE(LocalizeParse(R"({".1": "t"})", table, error));
    EXPECT_FALSE(LocalizeParse(R"({"a.b.1": 5})", table, error));
    EXPECT_FALSE(LocalizeParse(R"({"options.x.label": 5})", table, error));
    EXPECT_TRUE(table.files.empty());
    EXPECT_TRUE(table.strings.empty());
}

// A key that does not end in a message id is one of the port's own strings.
TEST(Localize, PortStringsAreKeysThatDoNotEndInAnId) {
    LocalizeTable table;
    std::string   error;
    ASSERT_TRUE(LocalizeParse(R"({"options.game.map.label": "Carte", "options.game.map.choice.3": "Non",
                                  "dun.script.d01.d01.1": "msg"})",
                              table, error))
        << error;
    EXPECT_EQ(table.strings.at("options.game.map.label"), "Carte");
    EXPECT_EQ(table.strings.count("options.game.map.choice.3"), 0U);
    EXPECT_EQ(table.files.at("options.game.map.choice").at(3), "Non");
    EXPECT_EQ(table.files.at("dun.script.d01.d01").at(1), "msg");
}

TEST(Localize, TextComesFromTheLanguageJsonOrTheEnglishGiven) {
    LangDir lang;
    Write(lang.dir / "fr_fr.json", R"({"options.game.map.label": "Carte du donjon", "options.game.map.choice.3": "Non"})");
    EXPECT_EQ(LocalizeText("options.game.map.label", "Dungeon Map"), "Carte du donjon");
    EXPECT_EQ(LocalizeText("options.game.map.choice.3", "Off"), "Non");
    EXPECT_EQ(LocalizeText("options.game.map.choice.2", "3"), "3") << "a key the file lacks gives the English";
    LanguageCode = 4;
    EXPECT_EQ(LocalizeText("options.game.map.label", "Dungeon Map"), "Dungeon Map") << "no file for German";
}

TEST(Localize, FormatFillsThePlaceholders) {
    EXPECT_EQ(LocalizeFormat("a %1 x %2\nwindow.", {"1280", "720"}), "a 1280 x 720\nwindow.");
    EXPECT_EQ(LocalizeFormat("%2 then %1, %1 again", {"x", "y"}), "y then x, x again");
    EXPECT_EQ(LocalizeFormat("100%", {"x"}), "100%");
}

TEST(Localize, ReplacesById) {
    std::vector<s16> retail = File({
        {1, "one"  },
        {2, "two"  },
        {3, "three"}
    });
    std::vector<s16> out;
    LocalizeStats    stats;
    ASSERT_TRUE(LocalizeMessages(retail.data(), retail.size(), {
                                                                   {2, "deux {-752}" },
                                                                   {9, "neuf\nlignes"}
    },
                                 out, stats));
    EXPECT_EQ(stats.replaced, 2);
    EXPECT_EQ(stats.missing, 0);
    EXPECT_EQ(stats.rejected, 0);
    EXPECT_EQ(Text(out, 1), "one");
    EXPECT_EQ(Text(out, 2), "deux {-752}");
    EXPECT_EQ(Text(out, 3), "three");
    EXPECT_EQ(Text(out, 9), "neuf\nlignes");
    EXPECT_EQ(Text(out, 4), "<none>");
}

TEST(Localize, CountsWhatTheFontLacks) {
    std::vector<s16> retail = File({
        {1, "one"}
    });
    std::vector<s16> out;
    LocalizeStats    stats;
    ASSERT_TRUE(LocalizeMessages(retail.data(), retail.size(), {
                                                                   {1, "a~b;"}
    },
                                 out, stats));
    EXPECT_EQ(stats.missing, 2);
    EXPECT_EQ(Text(out, 1), "a?b?");
}

TEST(Localize, KeepsRetailWhereATextDoesNotFit) {
    std::vector<s16> retail = File({
        {1, "one"},
        {2, "two"}
    });
    std::vector<s16> out;
    LocalizeStats    stats;
    // The first is past what a message file's s16 offsets reach, the last past its s16 ids.
    ASSERT_TRUE(LocalizeMessages(retail.data(), retail.size(),
                                 {
                                     {1, std::string(0x8000, 'A')},
                                     {2, "deux"},
                                     {40000, "no"}
    },
                                 out, stats));
    EXPECT_EQ(stats.rejected, 2);
    EXPECT_EQ(stats.replaced, 1);
    EXPECT_EQ(Text(out, 1), "one");
    EXPECT_EQ(Text(out, 2), "deux");

    EXPECT_FALSE(LocalizeMessages(retail.data(), retail.size(), {
                                                                    {1, std::string(0x8000, 'A')}
    },
                                  out, stats));
    EXPECT_EQ(stats.rejected, 1);
}

TEST(Localize, RetailRoundTrips) {
    std::vector<s16>           retail = File({
        {1, "Norune Village"   },
        {2, "{-1021}x\n{-1024}"},
        {3, "Tani\xC3\xA8re"   }
    });
    std::map<int, std::string> read;
    ASSERT_TRUE(LocalizeReadMessages(retail.data(), retail.size(), read));
    EXPECT_EQ(read, (std::map<int, std::string>{
                        {1, "Norune Village"   },
                        {2, "{-1021}x\n{-1024}"},
                        {3, "Tani\xC3\xA8re"   }
    }));

    std::vector<s16> out;
    LocalizeStats    stats;
    ASSERT_TRUE(LocalizeMessages(retail.data(), retail.size(), read, out, stats));
    EXPECT_EQ(out, retail);

    s16 junk[2] = {-1, 0};
    EXPECT_FALSE(LocalizeReadMessages(junk, 2, read));
}

// A message that never ends (as the last id of the PAL allmenu.mes does) is left out, not fatal.
TEST(Localize, SkipsAMessageWithoutAnEnd) {
    std::vector<s16> retail = File({
        {1, "one"},
        {2, "two"}
    });
    retail.pop_back();
    std::map<int, std::string> read;
    ASSERT_TRUE(LocalizeReadMessages(retail.data(), retail.size(), read));
    EXPECT_EQ(read.size(), 1U);
}

TEST(Localize, LoadedFileComesFromTheLanguageJson) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"},
        {2, "two"}
    });
    Write(lang.dir / "fr_fr.json", R"({"dun.script.d01.d01.2": "deux"})");

    Loaded loaded(retail);
    int    size = loaded.size;
    EXPECT_TRUE(LocalizeFile("cdrom0:\\DUN\\SCRIPT\\D01\\D01_3.MES", loaded.bytes, 2048, &size));
    std::vector<s16> got(reinterpret_cast<s16 *>(loaded.bytes), reinterpret_cast<s16 *>(loaded.bytes) + size / 2);
    EXPECT_EQ(Text(got, 1), "one");
    EXPECT_EQ(Text(got, 2), "deux");

    // Another file, and a name that is no message file, stay as they were.
    Loaded other(retail);
    size = other.size;
    EXPECT_FALSE(LocalizeFile("dun/script/d02/d02_3.mes", other.bytes, 2048, &size));
    EXPECT_FALSE(LocalizeFile("dun/script/d01/d01_3.bin", other.bytes, 2048, &size));
    EXPECT_EQ(size, other.size);

    // A language with no file of its own has nothing to say.
    LanguageCode = 4;
    EXPECT_FALSE(LocalizeFile("dun/script/d01/d01_4.mes", other.bytes, 2048, &size));
    EXPECT_EQ(size, other.size);
}

TEST(Localize, TheSaveFoldersFileLaysOverTheExecutables) {
    LangDir  lang;
    fs::path second = lang.dir / "second";
    LocalizeSetDirectories({lang.dir, second});
    std::vector<s16> retail = File({
        {1, "one"  },
        {2, "two"  },
        {3, "three"}
    });
    Write(lang.dir / "fr_fr.json", R"({"a.1": "exe", "a.2": "exe"})");
    Write(second / "fr_fr.json", R"({"a.2": "save"})");

    Loaded loaded(retail);
    int    size = loaded.size;
    ASSERT_TRUE(LocalizeFile("a_3.mes", loaded.bytes, 2048, &size));
    std::vector<s16> got(reinterpret_cast<s16 *>(loaded.bytes), reinterpret_cast<s16 *>(loaded.bytes) + size / 2);
    EXPECT_EQ(Text(got, 1), "exe");
    EXPECT_EQ(Text(got, 2), "save");
    EXPECT_EQ(Text(got, 3), "three");
}

TEST(Localize, BadJsonFallsBackToRetail) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"}
    });
    Write(lang.dir / "fr_fr.json", "{ not json");

    Loaded loaded(retail);
    int    size = loaded.size;
    testing::internal::CaptureStderr();
    EXPECT_FALSE(LocalizeFile("a.mes", loaded.bytes, 2048, &size));
    EXPECT_FALSE(LocalizeFile("a.mes", loaded.bytes, 2048, &size));
    std::string log = testing::internal::GetCapturedStderr();
    EXPECT_NE(log.find("fr_fr.json"), std::string::npos);
    // Told once, not on every load.
    EXPECT_EQ(log.find("fr_fr.json", log.find("fr_fr.json") + 1), std::string::npos);
    EXPECT_EQ(size, loaded.size);
}

TEST(Localize, AFileThatWouldNotFitStaysRetail) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"}
    });
    Write(lang.dir / "fr_fr.json", R"({"a.1": "a text far longer than the file it replaces"})");

    Loaded loaded(retail);
    int    size = loaded.size;
    testing::internal::CaptureStderr();
    EXPECT_FALSE(LocalizeFile("a.mes", loaded.bytes, static_cast<size_t>(size) + 4, &size));
    EXPECT_NE(testing::internal::GetCapturedStderr().find("retail text kept"), std::string::npos);
    EXPECT_EQ(size, loaded.size);
    EXPECT_TRUE(LocalizeFile("a.mes", loaded.bytes, 2048, &size));
    EXPECT_GT(size, loaded.size);
}

TEST(Localize, PackEntryComesFromTheJson) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"},
        {2, "two"}
    });
    Write(lang.dir / "fr_fr.json", R"({"commenu.emenu.pak.allmenu.1": "un"})");

    alignas(64) static unsigned char pack[4096];
    std::memcpy(pack, retail.data(), retail.size() * 2);
    int         size = static_cast<int>(retail.size() * 2);
    const auto *data = reinterpret_cast<const u_int *>(pack);

    // A pack nobody said where it came from is left alone.
    EXPECT_EQ(LocalizePack(reinterpret_cast<const u_int *>(pack), "allmenu.mes", data, &size), data);

    LocalizeLoaded("cdrom0:/COMMENU/A_FRE/EMENU.PAK;1", pack);
    LocalizeLoaded("commenu/a_fre/emenu.pak", pack);
    int          replaced = size;
    const u_int *got = LocalizePack(reinterpret_cast<const u_int *>(pack), "allmenu.mes", data, &replaced);
    ASSERT_NE(got, data);
    std::vector<s16> words(reinterpret_cast<const s16 *>(got), reinterpret_cast<const s16 *>(got) + replaced / 2);
    EXPECT_EQ(Text(words, 1), "un");
    EXPECT_EQ(Text(words, 2), "two");

    // The same entry read again is the same text, at the same place.
    int again = size;
    EXPECT_EQ(LocalizePack(reinterpret_cast<const u_int *>(pack), "allmenu.mes", data, &again), got);
    EXPECT_EQ(again, replaced);

    // An entry that is not a message file, and a buffer another file has taken, are not touched.
    int other = size;
    EXPECT_EQ(LocalizePack(reinterpret_cast<const u_int *>(pack), "option.img", data, &other), data);
    LocalizeLoaded("commenu/a_fre/other.img", pack);
    EXPECT_EQ(LocalizePack(reinterpret_cast<const u_int *>(pack), "allmenu.mes", data, &other), data);
}

// ---- Patches ----

namespace {

std::vector<s16> Patched(LangDir &lang, const std::vector<s16> &retail, const std::string &patches,
                         const std::string &files = "") {
    Write(lang.dir / "patches" / "a.json", patches);
    if (!files.empty()) {
        Write(lang.dir / "fr_fr.json", files);
    }
    Loaded loaded(retail);
    int    size = loaded.size;
    if (!LocalizeFile("a.mes", loaded.bytes, 2048, &size)) {
        return {};
    }
    return std::vector<s16>(reinterpret_cast<s16 *>(loaded.bytes), reinterpret_cast<s16 *>(loaded.bytes) + size / 2);
}

} // namespace

TEST(Localize, ParsesPatches) {
    LocalizeTable table;
    std::string   error;
    ASSERT_TRUE(LocalizeParsePatches(R"({"patches": [
        {"file": "A.MES", "id": 3, "replace": [["teh", "the"], ["recieve", "receive"]]},
        {"file": "a.mes", "id": 4, "original": "0123456789abcdef", "text": "new"}]})",
                                     table, error))
        << error;
    ASSERT_EQ(table.patches["a"].size(), 2U);
    EXPECT_EQ(table.patches["a"][0].replace.size(), 2U);
    EXPECT_EQ(table.patches["a"][1].original, "0123456789abcdef");
    EXPECT_EQ(*table.patches["a"][1].text, "new");

    for (const char *bad : {R"({"patches": 1})", R"({"patches": [{"id": 1, "text": "x"}]})",
                            R"({"patches": [{"file": "a", "id": 1}]})",
                            R"({"patches": [{"file": "a", "id": 1, "text": "x", "replace": [["a", "b"]]}]})",
                            R"({"patches": [{"file": "a", "id": 1, "replace": [["", "b"]]}]})",
                            R"({"patches": [{"file": "a", "id": 40000, "text": "x"}]})"}) {
        EXPECT_FALSE(LocalizeParsePatches(bad, table, error)) << bad;
    }
}

TEST(Localize, TextHashIsStable) {
    EXPECT_EQ(LocalizeTextHash(""), "cbf29ce484222325");
    EXPECT_EQ(LocalizeTextHash("a"), "af63dc4c8601ec8c");
    EXPECT_NE(LocalizeTextHash("one"), LocalizeTextHash("One"));
}

TEST(Localize, ReplacePatchEditsTheDiscsText) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"                },
        {2, "teh cat and teh dog"},
        {3, "three"              }
    });
    std::vector<s16> got = Patched(lang, retail, R"({"patches": [{"file": "a.mes", "id": 2, "replace": [["teh", "the"]]}]})");
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(Text(got, 1), "one");
    EXPECT_EQ(Text(got, 2), "the cat and the dog");
    EXPECT_EQ(Text(got, 3), "three");
}

TEST(Localize, APatchThatDoesNotMatchIsSkippedAndTold) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"},
        {2, "two"}
    });
    testing::internal::CaptureStderr();
    std::vector<s16> got = Patched(lang, retail, R"({"patches": [
        {"file": "a.mes", "id": 2, "replace": [["zzz", "y"]]},
        {"file": "a.mes", "id": 9, "text": "nine"},
        {"file": "a.mes", "id": 1, "original": "0000000000000000", "text": "uno"}]})");
    std::string      log = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(got.empty());
    EXPECT_NE(log.find("\"zzz\" is not in the disc's text"), std::string::npos);
    EXPECT_NE(log.find("message 9: not in the disc's file"), std::string::npos);
    EXPECT_NE(log.find("message 1: the disc's text is not what the patch was written for"), std::string::npos);
}

TEST(Localize, TextPatchWithTheRightHashApplies) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "one"},
        {2, "two"}
    });
    std::vector<s16> got = Patched(lang, retail,
                                   R"({"patches": [{"file": "a.mes", "id": 1, "original": ")" + LocalizeTextHash("one") +
                                       R"(", "text": "uno"}]})");
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(Text(got, 1), "uno");
    EXPECT_EQ(Text(got, 2), "two");
}

TEST(Localize, PatchesChainAndAFilesTextWins) {
    LangDir          lang;
    std::vector<s16> retail = File({
        {1, "aa"},
        {2, "bb"}
    });
    std::vector<s16> got = Patched(lang, retail,
                                   R"({"patches": [{"file": "a.mes", "id": 1, "replace": [["a", "b"]]},
                                                   {"file": "a.mes", "id": 1, "replace": [["bb", "c"]]},
                                                   {"file": "a.mes", "id": 2, "replace": [["b", "x"]]}]})",
                                   R"({"a.2": "mine"})");
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(Text(got, 1), "c");
    EXPECT_EQ(Text(got, 2), "mine");
}

// ---- game.language ----

#include "platform/config.hpp"

TEST(LocalizeLanguage, ConfigReadsAndWritesTheLanguage) {
    EXPECT_EQ(ConfigParse("").language, 0);
    EXPECT_EQ(ConfigParse(R"({"game": {"language": "francais"}})").language, 3);
    EXPECT_EQ(ConfigParse(R"({"game": {"language": "Espanol"}})").language, 6);
    EXPECT_EQ(ConfigParse(R"({"game": {"language": "english"}})").language, 2);
    EXPECT_EQ(ConfigParse(R"({"game": {"language": "ask"}})").language, 0);

    testing::internal::CaptureStderr();
    EXPECT_EQ(ConfigParse(R"({"game": {"language": "klingon"}})").language, 0);
    EXPECT_EQ(ConfigParse(R"({"game": {"language": 3}})").language, 0);
    testing::internal::GetCapturedStderr();

    Config config;
    config.language = 4;
    EXPECT_EQ(ConfigParse(ConfigSerialize(config)).language, 4);
    EXPECT_FALSE(ConfigAppliesOnRestart("game.language"));
}

// ---- the language files and the export ----

#include "platform/paths.hpp"

namespace {

void WriteMes(const fs::path &path, const std::vector<s16> &file) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size() * 2));
}

} // namespace

TEST(LocalizeLanguages, ExportWritesOneFlatFilePerLanguageWithTheSameKeys) {
    fs::path root = datafix::TempDir("langexport");
    WriteMes(root / "data/dun/script/d01/d01_2.mes", File({
                                                         {1, "Hello"},
                                                         {2, "Bye"  }
    }));
    WriteMes(root / "data/dun/script/d01/d01_3.mes", File({
                                                         {1, "Bonjour"                           },
                                                         {2, "Au revoir \xC3\xA0 la Cr\xC3\xA8me"}
    }));
    WriteMes(root / "data/commenu/a_ger/dunmenu.pak.mes", File({
                                                              {5, "Gegenstand"}
    }));
    WriteMes(root / "data/noda_w/dun00_eng.mes", File({
                                                     {9, "Only one"}
    }));
    PathsSetDataRoot(root / "data");

    ASSERT_GE(LocalizeExport(root / "out", true), 4);
    auto load = [&](const char *name) {
        std::ifstream stream(root / "out" / name);
        return nlohmann::ordered_json::parse(stream);
    };
    const auto en = load("en_gb.json");
    const auto fr = load("fr_fr.json");
    const auto de = load("de_de.json");
    // Flat "<file key>.<id>": text, the same keys for the same message in every language.
    EXPECT_EQ(en["dun.script.d01.d01.1"], "Hello");
    EXPECT_EQ(fr["dun.script.d01.d01.1"], "Bonjour");
    EXPECT_EQ(fr["dun.script.d01.d01.2"], "Au revoir \xC3\xA0 la Cr\xC3\xA8me");
    EXPECT_EQ(de["commenu.dunmenu.pak.5"], "Gegenstand");
    EXPECT_FALSE(en.contains("commenu.dunmenu.pak.5"));
    EXPECT_TRUE(en.contains("dun.script.d01.d01.2"));
    // A file that names no language is in every language's file.
    for (const auto *language : {&en, &fr, &de}) {
        EXPECT_EQ((*language)["noda_w.dun00_eng.9"], "Only one");
    }
    EXPECT_TRUE(fs::exists(root / "out" / "hashes" / "fr_fr.json"));
    EXPECT_EQ(load("hashes/fr_fr.json")["dun.script.d01.d01.1"], LocalizeTextHash("Bonjour"));

    // The file is UTF-8 as it stands, with no \u escapes standing in for letters.
    std::ifstream raw(root / "out" / "fr_fr.json", std::ios::binary);
    std::string   bytes((std::istreambuf_iterator<char>(raw)), std::istreambuf_iterator<char>());
    EXPECT_NE(bytes.find("Cr\xC3\xA8me"), std::string::npos);
    EXPECT_EQ(bytes.find("\\u00"), std::string::npos);
    raw.close();
    std::error_code removed;
    fs::remove_all(root, removed);
}

TEST(LocalizeLanguages, ExportSaysWhenTwoFilesShareAKey) {
    fs::path root = datafix::TempDir("langcollide");
    WriteMes(root / "data/x/m_3.mes", File({
                                          {1, "one"}
    }));
    WriteMes(root / "data/x/a_fre/m.mes", File({
                                              {1, "uno"}
    }));
    PathsSetDataRoot(root / "data");

    testing::internal::CaptureStderr();
    LocalizeExport(root / "out", false);
    EXPECT_NE(testing::internal::GetCapturedStderr().find("share a key"), std::string::npos);
    std::error_code removed;
    fs::remove_all(root, removed);
}

TEST(LocalizeLanguages, ChangingTheLanguageSettingWaitsForTheGameToSwitch) {
    LanguageCode = 2;
    Config before;
    Config after;
    after.language = 5;
    LocalizeConfigChanged(before, after);
    EXPECT_TRUE(LocalizeLanguagePending());
    EXPECT_EQ(LanguageCode, 2) << "nothing changes under a loaded area";
    LocalizeApplyPendingLanguage();
    EXPECT_EQ(LanguageCode, 5);
    EXPECT_FALSE(LocalizeLanguagePending());

    // Changing back before the game switched cancels it; "ask", the current language and a setting
    // that did not change leave nothing waiting.
    before = after;
    after.language = 3;
    LocalizeConfigChanged(before, after);
    EXPECT_TRUE(LocalizeLanguagePending());
    before = after;
    after.language = 5;
    LocalizeConfigChanged(before, after);
    EXPECT_FALSE(LocalizeLanguagePending());
    before = after;
    after.language = 0;
    LocalizeConfigChanged(before, after);
    EXPECT_FALSE(LocalizeLanguagePending());
    LocalizeApplyPendingLanguage();
    EXPECT_EQ(LanguageCode, 5);
}

// ---- the Options screen's own text ----

#include "menu_option.hpp"

// port/lang/ ships the Options screen's strings in each language: the same keys as the code has, nothing
// else, and the same placeholders and pad glyph codes as the English.
TEST(LocalizeOptions, ShippedTranslationsHaveExactlyTheCodesKeys) {
    const fs::path lang = fs::path(__FILE__).parent_path().parent_path().parent_path() / "lang";
    ASSERT_TRUE(fs::is_directory(lang)) << lang;

    std::map<std::string, std::string> english;
    for (const auto &[key, text] : OptionStrings()) {
        EXPECT_TRUE(english.emplace(key, text).second) << "two strings called " << key;
    }
    EXPECT_GT(english.size(), 100U);

    auto tokens = [](const std::string &text) {
        std::string out;
        for (size_t at = 0; at < text.size(); at++) {
            if ((text[at] == '%' && at + 1 < text.size() && isdigit(static_cast<unsigned char>(text[at + 1]))) ||
                (text[at] == '{' && at + 1 < text.size() && (text[at + 1] == '-' || isdigit(static_cast<unsigned char>(text[at + 1]))))) {
                out += text.substr(at, text.find_first_of("}", at) == std::string::npos ? 2 : text.find_first_of("} ", at) - at + 1);
            }
        }
        return out;
    };

    int files = 0;
    for (const char *name : {"fr_fr", "de_de", "it_it", "es_es"}) {
        std::ifstream stream(lang / (std::string(name) + ".json"));
        ASSERT_TRUE(stream) << name;
        const auto shipped = nlohmann::ordered_json::parse(stream);
        files++;
        for (const auto &[key, text] : english) {
            ASSERT_TRUE(shipped.contains(key)) << name << " lacks " << key;
            const std::string translated = shipped[key];
            EXPECT_FALSE(translated.empty()) << name << " " << key;
            EXPECT_EQ(tokens(translated), tokens(text)) << name << " " << key << ": placeholders and pad glyphs differ";
        }
        for (const auto &item : shipped.items()) {
            EXPECT_TRUE(english.count(item.key()) != 0) << name << " has " << item.key() << " that the code does not";
        }
    }
    EXPECT_EQ(files, 4);
}

TEST(LocalizeOptions, TheExportListsTheOptionStringsWithTheShippedText) {
    fs::path root = datafix::TempDir("langopt");
    WriteMes(root / "data/dun/script/d01/d01_3.mes", File({
                                                         {1, "Bonjour"}
    }));
    PathsSetDataRoot(root / "data");
    const fs::path lang = fs::path(__FILE__).parent_path().parent_path().parent_path() / "lang";
    LocalizeReset();
    LocalizeSetDirectories({lang});

    ASSERT_GE(LocalizeExport(root / "out", false), 1);
    auto load = [&](const char *name) {
        std::ifstream stream(root / "out" / name);
        return nlohmann::ordered_json::parse(stream);
    };
    const auto en = load("en_gb.json");
    const auto fr = load("fr_fr.json");
    EXPECT_EQ(en["options.game.map.label"], "Dungeon Map");
    EXPECT_EQ(fr["options.game.map.label"], "Carte du donjon");
    EXPECT_EQ(fr["dun.script.d01.d01.1"], "Bonjour");
    LocalizeSetDirectories({});
    LocalizeReset();
    std::error_code removed;
    fs::remove_all(root, removed);
}
