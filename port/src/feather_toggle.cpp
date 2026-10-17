#include "feather_toggle.hpp"

#include <algorithm>

#include "dun/gameloop.hpp"
#include "itemdata.hpp"
#include "mglib.hpp"
#include "platform/config.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "userstatus.hpp"

extern s32 gameTask;
extern s32 itemNowSel;

void FeatherToggleState::Sync(const FeatherToggleContext &context) {
    if (!context.enabled || !context.gameplay || !context.feather || context.slot < 1 || context.slot > 3 ||
        context.quantity <= 0 || context.duration <= 0 || context.character != context_.character ||
        context.slot != context_.slot) {
        on_ = false;
    }
    context_ = context;
}

bool FeatherToggleState::Use(bool held, bool pressed, bool allowed, bool moving) {
    // Other items and the opt-out path keep retail's held-button behavior.
    if (!context_.enabled || !context_.feather) return held;
    if (!context_.gameplay || !allowed || context_.quantity <= 0 || context_.duration <= 0 ||
        context_.slot < 1 || context_.slot > 3) return false;
    if (pressed) on_ = !on_;
    return on_ && moving;
}

bool FeatherToggleState::On(int slot) const {
    return on_ && context_.slot == slot;
}

namespace {
FeatherToggleState g_feather_toggle;
// Gold, red edging and pale highlights echo the quick-slot selector texture.
constexpr spRGBA kSelectorGold{128, 112, 0, 0x80};
constexpr spRGBA kSelectorEdge{112, 24, 8, 0x80};
constexpr spRGBA kSelectorHighlight{128, 128, 96, 0x80};
}

void FeatherToggleSync() {
    FeatherToggleContext context;
    context.enabled = ConfigGet().feather_toggle;
    if (UserStatus) {
        context.character = UserStatus->cur_chara;
        context.slot = itemNowSel;
        context.gameplay = gameTask == GAME_TASK_PLAY && BtEventMode == 0 && context.character >= 0 &&
                           context.character < 6 && UserStatus->hp[context.character] > 0;
        if (context.slot >= 1 && context.slot <= 3) {
            int slot = context.slot - 1;
            context.feather = UserStatus->item_pack.quick_item_slot[slot] == ITEM_DRAN_S_FEATHER;
            context.quantity = UserStatus->item_pack.quick_item_qty[slot];
            context.duration = UserStatus->active_item_vol[slot];
        }
    }
    g_feather_toggle.Sync(context);
}

bool FeatherUseRequested(bool held, bool pressed, bool allowed, bool moving) {
    // Slot navigation can have happened since the start of this tick.
    FeatherToggleSync();
    return g_feather_toggle.Use(held, pressed, allowed, moving);
}

bool FeatherToggleOn(int slot) {
    // Rendering must not show a stale badge after depletion or a menu transition in this tick.
    FeatherToggleSync();
    return g_feather_toggle.On(slot);
}

void DrawFeatherToggleIndicator(int x, int y) {
    // Texture-independent ON badge: it shares the quick bar's logical coordinates and HUD motion.
    auto box = [](int x, int y, int w, int h, spRGBA colour) {
        set2DSpriteC4(Vif1Packet, CRect_i_(x, y, w, h), &colour, &colour, &colour, &colour);
    };
    box(x, y, 22, 14, kSelectorEdge);
    box(x + 1, y + 1, 20, 12, kSelectorGold);
    constexpr unsigned letters[2][5] = {{7, 5, 5, 5, 7}, {9, 13, 11, 9, 9}};
    for (int letter = 0; letter < 2; ++letter) {
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < (letter == 0 ? 3 : 4); ++col) {
                if (letters[letter][row] & ((letter == 0 ? 4 : 8) >> col)) {
                    box(x + 3 + letter * 8 + col * 2, y + 2 + row * 2, 2, 2, kSelectorHighlight);
                }
            }
        }
    }
}

int FeatherDurationFill(int ticks) {
    if (ticks <= 0) return 0;
    // Round up so a feather with time left never appears empty.
    return (std::min(ticks, kFeatherDurationCapacity) * 34 + kFeatherDurationCapacity - 1) /
           kFeatherDurationCapacity;
}

int FeatherQuickSlotDuration(int slot) {
    if (!ConfigGet().feather_toggle || !UserStatus || slot < 1 || slot > 3) return -1;
    const auto &pack = UserStatus->item_pack;
    if (pack.quick_item_slot[slot - 1] != ITEM_DRAN_S_FEATHER || pack.quick_item_qty[slot - 1] <= 0) return -1;
    return std::max(0, UserStatus->active_item_vol[slot - 1]);
}

void DrawFeatherDurationBar(int x, int y, int ticks) {
    auto box = [](int x, int y, int w, int h, spRGBA colour) {
        set2DSpriteC4(Vif1Packet, CRect_i_(x, y, w, h), &colour, &colour, &colour, &colour);
    };
    box(x, y, 36, 6, kSelectorEdge);
    box(x + 1, y + 1, 34, 4, {28, 34, 38, 0x80});
    int fill = FeatherDurationFill(ticks);
    if (fill > 0) {
        box(x + 1, y + 1, fill, 4, kSelectorGold);
    }
}
