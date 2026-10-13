#pragma once

#include "common.h"

#include "menu_draw.hpp"
#include "shop.hpp"

/**
 * Shop UI bookkeeping shared by the charge shop and item shop screens: their
 * cursor, phase and animation state. Only the handful of fields read outside
 * this unit's own functions are named.
 */
struct ShopMenuWork {
    s16            shop_no;  /**< Shop being run. */
    s16            side;     /**< Board the cursor is on. @see ShopSide */
    s16            mode;     /**< How the shop was opened. */
    s16            key_used; /**< Set once this frame's confirm or sell press has been handled, so the frame's cancel press is ignored. */
    PERSONAL_BOARD board;    /**< The player's side of the shop: the personal board and the records it holds. */
    s32            unk_168;
    float          stock_y;          /**< Screen Y the stock board draws at, eased toward its top row. */
    float          stock_scroll;     /**< Scroll bar position of the stock board. */
    s16            stock_row_offset; /**< Cleared with the stock board's top row whenever a shop opens. */
    u8             stock_top_row;    /**< Row the stock board shows first. */
    u8             unk_177;
    float          cursor_x;  /**< Screen X of the cursor, eased toward its cell. */
    float          cursor_y;  /**< Screen Y of the cursor, eased toward its cell. */
    s16            talk_mode; /**< What the shopkeeper is saying or waiting on. @see ShopTalkMode */
    s16            unk_182;
    s32            step_count; /**< Frames the shop has spent in its talk mode. */
    s16            ready;      /**< Nonzero once the shop's textures are entered. */
    s16            tex_block;  /**< Texture block the shop's textures are entered into. */
    s16            unk_18C;
    s16            alpha; /**< Opacity the shop draws with. */
    s16            lang;  /**< Language the shop's plates are laid out for. */
    s16            unk_192;
    s16            person_state;     /**< Shopkeeper model: 1 while it is read, 2 once it is built, 0 when there is none. */
    s16            person_tex_block; /**< Texture block the shopkeeper's textures are entered into. */
    s16            unk_198;
    s16            msg_mode;   /**< How the shopkeeper's message is shown. @see ShopMsgMode */
    s16            motion;     /**< Motion the shopkeeper's model is playing: 0 idle, 3 talking. */
    s16            msg_no;     /**< Shopkeeper line the speech window was last made for. */
    s32            draw_delay; /**< Frames the shopkeeper has been stepped; the model and its speech are drawn once it reaches four. */
    s32            idle_count; /**< Frames without input; at 320 the shopkeeper starts talking. */
};

STATIC_ASSERT(sizeof(ShopMenuWork) == 0x1A8);

/** Shared UI state for the charge shop and item shop screens. */
extern ShopMenuWork ShopMenu;

/** Which shop is open. @see ShopKind */
extern s16 ChargeOrShopFlag;
