// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "editloop3.hpp"
#include "fish.hpp"
#include "fishing/framework.hpp"
#include "fishing/host.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "platform/datapacks.hpp"
#include "shop.hpp"
#include "tests/platform_fixture.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::platform;

namespace {

namespace fs = std::filesystem;

// What the documents fill, as bytes: each kind's row, model and name, and the prizes.
std::vector<unsigned char> Tables() {
    std::vector<unsigned char> bytes;
    for (int kind = -1; kind <= 18; kind++) {
        const FishInfo *info = fishing::Info(kind);
        const auto     *begin = reinterpret_cast<const unsigned char *>(info);
        if (info != nullptr) {
            bytes.insert(bytes.end(), begin, begin + sizeof(FishInfo));
        }
        const char *model = GetFishFileName(kind);
        if (model != nullptr) {
            bytes.insert(bytes.end(), model, model + std::strlen(model) + 1);
        }
        const int name = GetFishMsgNo(kind);
        bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&name), reinterpret_cast<const unsigned char *>(&name + 1));
        bytes.push_back(fishing::CountsAsMardanGarayan(kind) ? 1 : 0);
    }
    const auto *prizes = reinterpret_cast<const unsigned char *>(exitemlst);
    bytes.insert(bytes.end(), prizes, prizes + sizeof(exitemlst));
    return bytes;
}

nlohmann::json &Row(nlohmann::json &document, const char *rows, const char *key, int value) {
    for (auto &row : document[rows]) {
        if (row[key] == value) {
            return row;
        }
    }
    throw std::runtime_error("no such row");
}

// The share of each kind among the fish a place spawns at a time of day, over many seeded draws.
std::map<int, double> Shares(int spot, fishing::TimeOfDay time) {
    const fishing::Place *place = fishing::PlaceAtSpot(spot);
    std::mt19937          engine(1234);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    const fishing::SpawnContext            context{.time = time, .random = [&] { return uniform(engine); }};
    std::map<int, double>                  shares;
    int                                    total = 0;
    for (int run = 0; run < 50000; run++) {
        for (const fishing::Species *species : place->spawn->Spawn(*place, context)) {
            shares[species->kind]++;
            total++;
        }
    }
    for (auto &[kind, share] : shares) {
        share /= total;
    }
    return shares;
}

// A data folder with a datapacks folder beside it, and the exported documents to start packs from.
class FishingDocuments : public ::testing::Test {
protected:
    fs::path directory = fs::canonical(fs::temp_directory_path()) / std::format("dc_fishing_{}", ProcessId());
    fs::path data = directory / "data";

    void SetUp() override {
        fs::remove_all(directory);
        fs::create_directories(data);
        ASSERT_TRUE(fishing::Export(directory / "exported"));
        datapacks::Clear();
        item::Load();
        fishing::Load();
    }

    void TearDown() override {
        datapacks::Clear();
        item::Load();
        fishing::Load();
        fs::remove_all(directory);
    }

    nlohmann::json Document(const std::string &name) const {
        return nlohmann::json::parse(std::ifstream(directory / "exported/gameplay/fishing" / name));
    }

    void Pack(const std::string &pack, const std::string &name, const nlohmann::json &document) const {
        const fs::path file = directory / "datapacks" / pack / "gameplay/fishing" / name;
        fs::create_directories(file.parent_path());
        std::ofstream(file) << document.dump();
    }

    void Order(const std::string &packs) const {
        fs::create_directories(directory / "datapacks");
        std::ofstream(directory / "datapacks/datapacks.json") << R"({"schema_version":1,"load_order":[)" + packs + "]}";
        datapacks::Load(data);
        item::Load();
        fishing::Load();
    }
};

TEST_F(FishingDocuments, TheCompiledTablesAreRetails) {
    const FishInfo *mardan = fishing::Info(5);
    ASSERT_NE(mardan, nullptr);
    EXPECT_EQ(mardan->min_fp, 200);
    EXPECT_EQ(mardan->max_fp, 400);
    EXPECT_EQ(mardan->food_affinity[6], 1.0f); // the Poisonous Apple
    EXPECT_EQ(fishing::Info(3)->model_size, 35.4f);
    EXPECT_EQ(fishing::Info(18), nullptr);
    EXPECT_STREQ(GetFishFileName(5), "chara/f06a.chr");
    EXPECT_EQ(GetFishFileName(18), nullptr);
    EXPECT_EQ(GetFishMsgNo(5), FishMsg[5] + 30);
    EXPECT_EQ(GetFishMsgNo(-1), 0);
    EXPECT_TRUE(fishing::CountsAsMardanGarayan(5) && fishing::CountsAsMardanGarayan(17));
    EXPECT_FALSE(fishing::CountsAsMardanGarayan(4));
    EXPECT_EQ(exitemlst[0].item_no, ITEM_ANTIDOTE_DRINK);
    EXPECT_EQ(exitemlst[32].price, 2500);
    EXPECT_EQ(fishing::SpawnAt(3, ED_TIME_DAY).kinds.size(), 5u);
    EXPECT_FALSE(fishing::SpawnAt(3, ED_TIME_DAY).draw_under_water);
    EXPECT_EQ(fishing::SpawnAt(9, ED_TIME_DAY).kinds, std::vector<int>(5, -1));
}

TEST_F(FishingDocuments, TheTrackedDocumentsHoldTheCompiledTables) {
    // port/gameplay/fishing, which the build seals into its gameplay seed and dcdata writes into the data.
    const fs::path        tracked = fs::path(__FILE__).parent_path().parent_path().parent_path() / "gameplay/fishing";
    std::set<std::string> files;
    for (const fs::directory_entry &entry : fs::directory_iterator(tracked)) {
        files.insert(entry.path().filename().string());
    }
    EXPECT_EQ(files, (std::set<std::string>{"fish.json", "places.json", "prizes.json"}));
    for (const std::string &name : files) {
        EXPECT_EQ(nlohmann::json::parse(std::ifstream(tracked / name)), Document(name)) << name;
    }
}

TEST_F(FishingDocuments, TheExportLoadsBackAsCompiled) {
    const std::vector<unsigned char> compiled = Tables();
    fs::create_directories(directory / "datapacks");
    fs::rename(directory / "exported", directory / "datapacks/exported");
    Order(R"("exported")");
    EXPECT_EQ(Tables(), compiled);
}

TEST_F(FishingDocuments, APacksChangeReachesTheGameAndLeavesWithIt) {
    const std::vector<unsigned char> compiled = Tables();
    auto                             fish = Document("fish.json");
    Row(fish, "fish", "fish_kind", 4)["max_fp"] = 90;
    Row(fish, "fish", "fish_kind", 4)["mardan_garayan"] = true;
    Row(fish, "fish", "fish_kind", 4)["fight"] = {{"pull", 1.5}};
    Row(fish, "fish", "fish_kind", 4)["food_affinity"] = nlohmann::json::array({
        {{"bait", "vanilla:poisonous_apple"}, {"affinity", 0.75}}
    });
    auto places = Document("places.json");
    Row(places, "places", "spot", 0)["fish_count"] = 6;
    Row(places, "places", "spot", 0)["bite"]["lose_bait_early"] = 1.0;
    Row(places, "places", "spot", 0)["spawn"]["fish"] = nlohmann::json::array({
        {{"fish", "vanilla:fish_8"}, {"weight", 1}}
    });
    auto prizes = Document("prizes.json");
    prizes["prizes"][0] = {
        {"item",  "vanilla:bread"},
        {"price", 7              }
    };
    Pack("fishy", "fish.json", fish);
    Pack("fishy", "places.json", places);
    Pack("fishy", "prizes.json", prizes);
    Order(R"("fishy")");

    EXPECT_EQ(fishing::Info(4)->max_fp, 90);
    EXPECT_EQ(fishing::Info(4)->food_affinity[6], 0.75f);
    EXPECT_EQ(fishing::Info(4)->food_affinity[1], 0.0f);
    EXPECT_TRUE(fishing::CountsAsMardanGarayan(4));
    EXPECT_EQ(fishing::SpeciesOfKind(4)->pull, 1.5f);
    EXPECT_EQ(fishing::SpeciesOfKind(3)->pull, 0.5f); // left out: the default
    EXPECT_EQ(fishing::SpawnAt(0, ED_TIME_DAY).kinds, std::vector<int>(6, 8));
    const fishing::BiteContext always{.random = [] { return 0.99; }};
    EXPECT_TRUE(fishing::PlaceAtSpot(0)->bite->LosesBait(fishing::LossCause::PressedEarly, always));
    EXPECT_FALSE(fishing::PlaceAtSpot(1)->bite->LosesBait(fishing::LossCause::PressedEarly, always));
    EXPECT_EQ(exitemlst[0].item_no, ITEM_BREAD);
    EXPECT_EQ(exitemlst[0].price, 7);

    Order("");
    EXPECT_EQ(Tables(), compiled);
}

TEST_F(FishingDocuments, ADocumentThatCannotBeUsedFallsBackWhole) {
    const std::vector<unsigned char> compiled = Tables();
    auto                             fish = Document("fish.json");
    Row(fish, "fish", "fish_kind", 4)["max_fp"] = 90;
    fish["fish"].erase(0); // every kind must be there
    auto places = Document("places.json");
    Row(places, "places", "spot", 1)["spawn"]["stage"] = "nonesuch";
    Row(places, "places", "spot", 2)["bite"]["hook_ticks"] = nlohmann::json::array({60, 30});
    auto prizes = Document("prizes.json");
    prizes["prizes"][0]["item"] = "vanilla:no_such_item";
    auto liking = Document("fish.json");
    Row(liking, "fish", "fish_kind", 4)["food_affinity"][0]["bait"] = "vanilla:bread"; // not a bait
    Pack("bad", "fish.json", fish);
    Pack("bad", "places.json", places);
    Pack("bad", "prizes.json", prizes);
    Pack("worse", "fish.json", liking);
    Order(R"("bad", "worse")");
    EXPECT_EQ(Tables(), compiled);
}

// A custom species, as a pack's document gives it.
nlohmann::json Species(const std::string &id) {
    return {
        {"schema_version", 2                                                                },
        {"kind",           "fish_species"                                                   },
        {"id",             id                                                               },
        {"model",          "chara/f06a.chr"                                                 },
        {"model_size",     21.0                                                             },
        {"min_size",       12.0                                                             },
        {"max_size",       24.0                                                             },
        {"min_fp",         50                                                               },
        {"max_fp",         90                                                               },
        {"food_affinity",  {{{"bait", "vanilla:carrot"}, {"affinity", 1.0}}}                },
        {"text",           {{"en_gb", {{"name", "Moon Trout"}}}, {"fr_fr", {{"name", "Truite"}}}}}
    };
}

TEST_F(FishingDocuments, ACustomSpeciesComesFromItsOwnDocumentWithAKindOfItsOwn) {
    Pack("trout", "species/chronicle.moon_trout.json", Species("chronicle:moon_trout"));
    Pack("trout", "species/chronicle.char.json", Species("chronicle:char"));
    auto places = Document("places.json");
    Row(places, "places", "spot", 0)["spawn"]["fish"] = nlohmann::json::array({
        {{"fish", "chronicle:moon_trout"}, {"weight", 1}}
    });
    Pack("trout", "places.json", places);
    Order(R"("trout")");
    const fishing::Species *trout = fishing::FindSpecies("chronicle:moon_trout");
    ASSERT_NE(trout, nullptr);
    EXPECT_EQ(fishing::FindSpecies("chronicle:char")->kind, fishing::kCustomKindFirst); // ID order
    EXPECT_EQ(trout->kind, fishing::kCustomKindFirst + 1);
    EXPECT_TRUE(trout->IsCustom());
    EXPECT_EQ(fishing::Info(trout->kind)->max_fp, 90);
    EXPECT_STREQ(GetFishFileName(trout->kind), "chara/f06a.chr");
    EXPECT_EQ(fishing::SpeciesName(*trout, "fr_fr"), "Truite");
    EXPECT_EQ(fishing::SpeciesName(*trout, "de_de"), "Moon Trout");
    EXPECT_FALSE(fishing::CountsAsMardanGarayan(trout->kind));
    EXPECT_EQ(fishing::SpawnAt(0, ED_TIME_DAY).kinds, std::vector<int>(4, trout->kind));
}

TEST_F(FishingDocuments, ACustomSpeciesThatCannotBeReadIsLeftOutAndTold) {
    Pack("bad", "species/chronicle.wrong_name.json", Species("chronicle:moon_trout"));
    Pack("bad", "species/vanilla.fish_3.json", Species("vanilla:fish_3"));
    auto nameless = Species("chronicle:nameless");
    nameless.erase("text");
    Pack("bad", "species/chronicle.nameless.json", nameless);
    auto odd = Species("chronicle:odd");
    odd["colour"] = "silver";
    Pack("bad", "species/chronicle.odd.json", odd);
    testing::internal::CaptureStderr();
    Order(R"("bad")");
    const std::string told = testing::internal::GetCapturedStderr();
    EXPECT_EQ(fishing::FindSpecies("chronicle:moon_trout"), nullptr);
    EXPECT_EQ(fishing::FindSpecies("chronicle:nameless"), nullptr);
    EXPECT_NE(fishing::FindSpecies("chronicle:odd"), nullptr);
    EXPECT_NE(told.find("chronicle.wrong_name.json"), std::string::npos) << told;
    EXPECT_NE(told.find("vanilla namespace"), std::string::npos) << told;
    EXPECT_NE(told.find("fields this version does not read: colour"), std::string::npos) << told;
}

TEST_F(FishingDocuments, TheIncludedEnhancedFishingPackLoadsWhole) {
    // port/datapacks/enhanced_fishing, which testers copy into their datapacks.
    const fs::path pack = fs::path(__FILE__).parent_path().parent_path().parent_path() / "datapacks/enhanced_fishing";
    fs::create_directories(directory / "datapacks");
    fs::copy(pack, directory / "datapacks/enhanced_fishing", fs::copy_options::recursive);
    testing::internal::CaptureStderr();
    Order(R"("enhanced_fishing")");
    const std::string told = testing::internal::GetCapturedStderr();
    EXPECT_EQ(told.find("config:"), std::string::npos) << told;

    for (int spot = 0; spot < 5; spot++) {
        EXPECT_TRUE(fishing::PlaceAtSpot(spot)->bite->ChoosesForTheCast()) << spot; // brisk
        EXPECT_TRUE(fishing::PlaceAtSpot(spot)->fight->ReadsRods()) << spot;         // tension
    }
    EXPECT_EQ(fishing::SpeciesOfKind(17)->pull, 2.0f);
    // The Baron's pull with each of Ruty's rods: barely, catchable, and the first test fight's feel.
    const auto baron_with = [](std::string_view rod) { return 2.0f / fishing::EffectOf(*item::Find(rod)->Get<item::Rod>()).pull_divisor; };
    EXPECT_NEAR(baron_with("chronicle:sea_rod"), 1.71f, 0.02f);
    EXPECT_NEAR(baron_with("chronicle:garayan_rod"), 1.33f, 0.02f);
    EXPECT_NEAR(baron_with("chronicle:baron_rod"), 0.78f, 0.02f);
}

TEST_F(FishingDocuments, EachPlaceSpawnsWithRetailsOdds) {
    using enum fishing::TimeOfDay;
    const auto expect = [](int spot, fishing::TimeOfDay time, const std::map<int, double> &odds) {
        const std::map<int, double> shares = Shares(spot, time);
        ASSERT_EQ(shares.size(), odds.size()) << "spot " << spot;
        for (const auto &[kind, chance] : odds) {
            EXPECT_NEAR(shares.at(kind), chance, 0.005) << "spot " << spot << " kind " << kind;
        }
    };
    expect(0, Day, {{1, 0.25}, {2, 0.25}, {6, 0.25}, {7, 0.25}});
    expect(1, Day, {{1, 0.35}, {4, 0.35}, {9, 0.10}, {10, 0.20}});
    expect(3, Night, {{0, 0.2}, {3, 0.2}, {11, 0.2}, {12, 0.2}, {13, 0.2}});
    // The rare fish come one time in the time of day's interval, kind 17 one time in five of those.
    for (const auto &[time, interval] : std::map<fishing::TimeOfDay, double>{{Day, 50}, {Evening, 20}, {Night, 35}, {Morning, 25}}) {
        const double rare = 1.0 / interval;
        expect(2, time, {{17, rare / 5}, {5, rare * 4 / 5}, {2, (1 - rare) / 3}, {4, (1 - rare) / 3}, {6, (1 - rare) / 3}});
        expect(4, time, {{17, rare / 5}, {5, rare * 4 / 5}, {14, (1 - rare) * 0.4}, {15, (1 - rare) * 0.3}, {16, (1 - rare) * 0.3}});
    }
}

} // namespace

} // namespace dc::test
