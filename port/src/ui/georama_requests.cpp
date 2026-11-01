#include "ui/georama_requests.hpp"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "clsmes.hpp"
#include "dialog/npc.hpp"
#include "edit.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editpartsinfo.hpp"
#include "gameutil.hpp"
#include "journal/heard.hpp"
#include "localize/localize.hpp"
#include "main.hpp"
#include "memcard.hpp"
#include "savedata.hpp"
#include "text/gametext.hpp"
#include "ui/framework.hpp"

namespace dc::ui::georama_requests {

using namespace dc::game;
using namespace dc::localize;

namespace {

// The parts CEditGround::RequestCheck asks about in each town: a request is kept under the part it
// is about (request[part]), and the villagers living in a part voice its request.
constexpr std::array kNoruneParts = {0, 1, 2, 3, 4, 5, 6, 7};
constexpr std::array kMatatakiParts = {0, 1, 2, 3, 4, 5, 6, 7, 14, 16};
constexpr std::array kQueensParts = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr std::array kMuskaParts = {0, 1, 2, 3, 4, 5, 6, 7, 10};
constexpr std::array kYellowDropsParts = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

// A villager is Atla element 80 + its number in the town's villager list, which names it.
constexpr int kVillagerElement = 80;
constexpr int kVillagers = 16;

constexpr const char *kControls = "{up} / {down} Select   {left} / {right} Page   {circle} Back";

enum class State {
    Unknown,
    Unfinished,
    Unmet,
    Met
};

ui::Catalogue s_catalogue;
int           s_town = -1;

std::span<const int> RequestParts(int town) {
    switch (town) {
        case TOWN_NORUNE:
            return kNoruneParts;
        case TOWN_MATATAKI:
            return kMatatakiParts;
        case TOWN_QUEENS:
            return kQueensParts;
        case TOWN_MUSKA_LACKA:
            return kMuskaParts;
        case TOWN_YELLOW_DROPS:
            return kYellowDropsParts;
        default:
            return {};
    }
}

// A message of the game's system messages, which hold the names of every town's parts and villagers.
std::string SystemText(int message) {
    if (SystemMes == nullptr) {
        return {};
    }
    // As ClsMes::GetTextLineDataTop reads a message file: the count, each message's number and
    // where its text starts, then the texts.
    const int    count = SystemMes[0];
    const short *body = &SystemMes[1];
    for (int i = 0; i < count; i++) {
        if (body[i * 2 + 1] == message) {
            return text::GameTextDecode(&body[count + body[i * 2 + 2]]);
        }
    }
    return {};
}

State PartState(int part) {
    CEditPartsInfo *parts = CommonMenuAtoraInfo;
    EDITPARTS_INFO *info = parts != nullptr ? parts->GetPartsInfo(part) : nullptr;
    if (info == nullptr || info->obtained == 0) {
        return State::Unknown;
    }
    // RequestCheck clears the request of a part that is not complete.
    if (parts->CheckComplete(part) == 0) {
        return State::Unfinished;
    }
    return parts->GetRequest(part) != 0 ? State::Met : State::Unmet;
}

int PartAt(int index) {
    const std::span<const int> parts = RequestParts(s_town);
    return index >= 0 && std::cmp_less(index, parts.size()) ? parts[index] : -1;
}

ui::CatalogueEntry Entry(int index) {
    const int part = PartAt(index);
    switch (PartState(part)) {
        case State::Unknown:
            return {.label = LocalizeText("requests.unknown", "???"), .color = ColorGrey};
        case State::Unfinished:
            return {.label = SystemText(GetAtraMsgNo(s_town, part)), .color = ColorGrey};
        case State::Unmet:
            return {.label = SystemText(GetAtraMsgNo(s_town, part)), .color = ColorWhite};
        case State::Met:
            return {.label = SystemText(GetAtraMsgNo(s_town, part)), .color = ColorGreen};
    }
    return {};
}

// A party member's name as the message window fills it in for {nameN}: the player may rename them.
std::string CharaName(int chara) {
    std::vector<s16> codes;
    const s16       *name = SaveData != nullptr ? SaveData->GetCharaName(chara) : nullptr;
    for (int i = 0; name != nullptr && i < 11 && name[i] != 0; i++) {
        codes.push_back(static_cast<s16>(NameRegistCodeJtoE(static_cast<u16>(name[i]) - 1)));
    }
    codes.push_back(MES_CODE_END);
    return text::GameTextDecode(codes.data());
}

// A villager's line as the journal shows it, a page for each of the message window's: names filled in,
// the window's own line breaks and codes (colours, waits) left to the panel's wrapping.
std::vector<std::string> LinePages(std::string_view line) {
    std::vector<std::string> pages(1);
    for (std::size_t at = 0; at < line.size(); at++) {
        if (line[at] == '\n') {
            pages.back() += ' ';
        } else if (line[at] == '{') {
            const std::size_t close = line.find('}', at);
            if (close == std::string_view::npos) {
                break;
            }
            const std::string_view token = line.substr(at + 1, close - at - 1);
            if (token == "page") {
                pages.emplace_back();
            } else if (token.size() == 5 && token.starts_with("name") && token[4] >= '1' && token[4] <= '6') {
                pages.back() += CharaName(token[4] - '1');
            }
            at = close;
        } else {
            pages.back() += line[at];
        }
    }
    for (std::string &page : pages) {
        std::string tidy;
        for (char ch : page) {
            if (ch != ' ' || (!tidy.empty() && tidy.back() != ' ')) {
                tidy += ch;
            }
        }
        while (!tidy.empty() && tidy.back() == ' ') {
            tidy.pop_back();
        }
        page = std::move(tidy);
    }
    std::erase_if(pages, [](const std::string &page) { return page.empty(); });
    return pages;
}

// The pages of a building's residents: each villager whose home it is and who is back in it (the house
// holds their element), with their request once they have said it.
std::vector<ui::CataloguePage> ResidentPages(int part) {
    std::vector<ui::CataloguePage> pages;
    CEditPartsInfo                *parts = CommonMenuAtoraInfo;
    EDITPARTS_INFO                *info = parts != nullptr ? parts->GetPartsInfo(part) : nullptr;
    if (info == nullptr || EdEventInfo.villagers == nullptr) {
        return pages;
    }
    for (int villager = 0; villager < kVillagers; villager++) {
        const VILLAGER_INFO &record = EdEventInfo.villagers[villager];
        if (record.character_no != part || record.model_no < 0 || std::cmp_greater_equal(record.model_no, std::size(info->elements)) ||
            info->elements[record.model_no].enabled == 0) {
            continue;
        }
        const std::string name = SystemText(GetAtraMsgNo(s_town, kVillagerElement + villager));
        if (!journal::RequestHeard(s_town, villager)) {
            pages.push_back({.title = name,
                             .text = LocalizeText("requests.not_heard", "Talk to them to hear their request."),
                             .color = ColorGrey});
            continue;
        }
        for (std::string &page : LinePages(dialog::DialogNpcTalkLine(journal::RequestMessage(villager)))) {
            pages.push_back({.title = name, .text = std::move(page)});
        }
    }
    return pages;
}

ui::CatalogueDetail Detail(int index) {
    const int   part = PartAt(index);
    const State state = PartState(part);
    if (state == State::Unknown) {
        return {.heading = LocalizeText("requests.unknown", "???"),
                .status = LocalizeText("requests.not_found", "Not found yet."),
                .status_color = ColorGrey};
    }

    ui::CatalogueDetail detail{.heading = SystemText(GetAtraMsgNo(s_town, part))};
    switch (state) {
        case State::Unfinished:
            detail.status = LocalizeText("requests.unfinished", "Unfinished. Its request counts once it is complete.");
            detail.status_color = ColorGrey;
            break;
        case State::Unmet:
            detail.status = LocalizeText("requests.unmet", "Request not met.");
            detail.status_color = ColorYellow;
            break;
        default:
            detail.status = LocalizeText("requests.met", "Request met.");
            detail.status_color = ColorGreen;
            break;
    }
    // Its residents only once it stands in the town whole, so a building still to come names no one.
    EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(part);
    if (state != State::Unfinished && info != nullptr && info->placed > 0) {
        detail.pages = ResidentPages(part);
    }
    return detail;
}

ui::TrackerTally Summary() {
    const std::span<const int> parts = RequestParts(s_town);
    ui::TrackerTally           tally{.done = 0, .total = static_cast<int>(parts.size())};
    for (int part : parts) {
        if (PartState(part) == State::Met) {
            tally.done++;
        }
    }
    return tally;
}

} // namespace

void Open(int texture_block) {
    s_town = NowEditMap;
    TrackerWindow::LoadStoneBanner(texture_block);
    s_catalogue.Open({.title = LocalizeText("requests.title", "GEORAMA: REQUESTS"),
                      .list_heading = LocalizeText("requests.list", "BUILDINGS"),
                      .summary_label = LocalizeText("requests.summary", "Requests Met"),
                      .controls = LocalizeText("requests.controls", kControls),
                      .count = [] { return static_cast<int>(RequestParts(s_town).size()); },
                      .entry = Entry,
                      .detail = Detail,
                      .summary = Summary});
}

bool HandleKey() {
    if (!s_catalogue.HandleKey()) {
        return false;
    }
    TrackerWindow::ReleaseStoneBanner();
    return true;
}

void Draw() {
    s_catalogue.Draw();
}

int PageArrowAt(float x, float y) {
    if (s_catalogue.PageArrowRect(-1).Contains(x, y)) {
        return -1;
    }
    return s_catalogue.PageArrowRect(1).Contains(x, y) ? 1 : 0;
}

void AppendStrings(std::vector<std::pair<std::string, std::string>> &out) {
    const std::pair<std::string, std::string> strings[] = {
        {"requests.title",      "GEORAMA: REQUESTS"                                  },
        {"requests.list",       "BUILDINGS"                                          },
        {"requests.summary",    "Requests Met"                                       },
        {"requests.controls",   kControls                                            },
        {"requests.analysis",   "{square} Requests   {circle} Back"                  },
        {"requests.unknown",    "???"                                                },
        {"requests.not_found",  "Not found yet."                                     },
        {"requests.unfinished", "Unfinished. Its request counts once it is complete."},
        {"requests.unmet",      "Request not met."                                   },
        {"requests.met",        "Request met."                                       },
        {"requests.not_heard",  "Talk to them to hear their request."                },
    };
    for (const auto &entry : strings) {
        out.push_back(entry);
    }
}

} // namespace dc::ui::georama_requests
