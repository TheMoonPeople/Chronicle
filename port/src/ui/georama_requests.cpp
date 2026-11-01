#include "ui/georama_requests.hpp"

#include <array>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "edit.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editpartsinfo.hpp"
#include "localize/localize.hpp"
#include "main.hpp"
#include "memcard.hpp"
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

// The villagers living in part who are back in the town: their house holds their element.
std::vector<std::string> Residents(int part) {
    std::vector<std::string> names;
    CEditPartsInfo          *parts = CommonMenuAtoraInfo;
    EDITPARTS_INFO          *info = parts != nullptr ? parts->GetPartsInfo(part) : nullptr;
    if (info == nullptr || EdEventInfo.villagers == nullptr) {
        return names;
    }
    for (int villager = 0; villager < kVillagers; villager++) {
        const VILLAGER_INFO &record = EdEventInfo.villagers[villager];
        if (record.character_no != part || record.model_no < 0 || std::cmp_greater_equal(record.model_no, std::size(info->elements))) {
            continue;
        }
        if (info->elements[record.model_no].enabled != 0) {
            names.push_back(SystemText(GetAtraMsgNo(s_town, kVillagerElement + villager)));
        }
    }
    return names;
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
    detail.body_heading = LocalizeText("requests.residents", "Residents");
    detail.body = Residents(part);
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
                      .progress = LocalizeText("requests.progress", "Progress: %1%"),
                      .controls = LocalizeText("requests.controls", "{up} / {down} Select   {circle} Back"),
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

void AppendStrings(std::vector<std::pair<std::string, std::string>> &out) {
    const std::pair<std::string, std::string> strings[] = {
        {"requests.title",      "GEORAMA: REQUESTS"                                  },
        {"requests.list",       "BUILDINGS"                                          },
        {"requests.summary",    "Requests Met"                                       },
        {"requests.progress",   "Progress: %1%"                                      },
        {"requests.controls",   "{up} / {down} Select   {circle} Back"               },
        {"requests.analysis",   "{square} Requests   {circle} Back"                   },
        {"requests.unknown",    "???"                                                },
        {"requests.not_found",  "Not found yet."                                     },
        {"requests.unfinished", "Unfinished. Its request counts once it is complete."},
        {"requests.unmet",      "Request not met."                                   },
        {"requests.met",        "Request met."                                       },
        {"requests.residents",  "Residents"                                          },
    };
    for (const auto &entry : strings) {
        out.push_back(entry);
    }
}

} // namespace dc::ui::georama_requests
