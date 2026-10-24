#include "language.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#include "area_card.hpp"
#include "dataread.hpp"
#include "gameloop.hpp"
#include "gfx/gfx.hpp"
#include "localize_texture.hpp"
#include "mainselect.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "nowload.hpp"
#include "platform/clock.hpp"
#include "rect.hpp"
#include "texture_port.hpp"
#include "textwipe.hpp"
#include "tim2.hpp"

namespace {

// What retail's 320000-byte stack array holds, plus the sector-rounded tail LoadFile2 writes.
constexpr std::size_t kArchiveBytes = 320000 + 2048;

int count;

// The sprite the last vertical sync would have sent; the idle hook draws it.
struct LoadingSprite {
    bool      visible = false;
    CTexture *texture = nullptr;
    CRect_i_  screen;
    CRect_i_  texel;
    u_char    alpha = 0;
};

LoadingSprite sprite;

// The area name card's file name without its extension (mt01...), which is what the localization's picture is called:
// the card is one texture called "maptitle" whatever the area.
std::string card_name;
// Original card texture for comparison with the runtime lettering.
unsigned painted_tbp = 0;
unsigned painted_cbp = 0;

u_char *Archive() {
    static std::unique_ptr<u_char[]> buffer(new u_char[kArchiveBytes + 64]);
    std::uintptr_t                   address = reinterpret_cast<std::uintptr_t>(buffer.get());
    return buffer.get() + ((64 - address % 64) % 64);
}

void Show(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, u_char alpha) {
    sprite.visible = true;
    sprite.texture = texture;
    sprite.screen = screen;
    sprite.texel = texel;
    sprite.alpha = alpha;
}

void Upload(CTexture *texture) {
    sceGsTex0 *tex0 = reinterpret_cast<sceGsTex0 *>(&texture->tex0);
    unsigned   tbp = 0;
    unsigned   cbp = 0;
    if (texture->image[0] != nullptr) {
        const u_char      *levels[1] = {reinterpret_cast<const u_char *>(texture->image[0])};
        PortDecodedTexture decoded;
        if (PortDecodeTexture(texture->bpp, texture->width, texture->height, levels, 1,
                              reinterpret_cast<const u_char *>(texture->clut), 0,
                              false, decoded)) {
            if (texture == &nl_tex && !card_name.empty()) {
                painted_tbp = painted_cbp = 0;
                PortDecodedTexture painted = decoded;
                if (!areacard::Prepare(card_name, decoded)) {
                    LocalizeTexture(card_name.c_str(), decoded);
                    painted.levels.clear();
                } else {
                    LocalizeTexture(card_name.c_str(), painted);
                }
                if (!painted.levels.empty()) {
                    painted_tbp = PortCreateTexture(painted, PortTextureOwner::Loading, &painted_cbp);
                }
            }
            tbp = PortCreateTexture(decoded, PortTextureOwner::Loading, &cbp);
        }
    }
    tex0->TBP0 = tbp;
    tex0->bits.tcc = 1;
    tex0->CBP = cbp;
}

// set2DSprite's alpha form: a GS sprite in 12.4 coordinates, MODULATE with 0x80 grey and the fade
// in alpha, blended (Cs - Cd) * As + Cd over the cleared frame.
void DrawCardSprite(PortTextureRef ref, bool with_lettering) {
    // TEX1 MMAG is setbilinear's switch, which is on unless a mode turns it off.
    ref.binding.filter = gfx::Filter::Linear;
    const CRect_i_ &screen = sprite.screen;
    const CRect_i_ &texel = sprite.texel;
    float           x0 = MGPortLogicalX((screen.x << 4) + 27648);
    float           x1 = MGPortLogicalX(((screen.x + screen.width) << 4) + 27647);
    float           y0 = MGPortLogicalY((screen.y << 3) + GS_Y_OFFSET);
    float           y1 = MGPortLogicalY(((screen.y + screen.height) << 3) + GS_Y_OFFSET);
    float           u0 = static_cast<float>(texel.x);
    float           u1 = static_cast<float>(texel.x + texel.width);
    float           v0 = static_cast<float>(texel.y);
    float           v1 = static_cast<float>(texel.y + texel.height);
    gfx::Vertex2D   quad[4] = {
        {x0, y0, 0.0f, u0, v0, {0x80, 0x80, 0x80, sprite.alpha}},
        {x1, y0, 0.0f, u1, v0, {0x80, 0x80, 0x80, sprite.alpha}},
        {x1, y1, 0.0f, u1, v1, {0x80, 0x80, 0x80, sprite.alpha}},
        {x0, y1, 0.0f, u0, v1, {0x80, 0x80, 0x80, sprite.alpha}},
    };
    gfx::DrawState state;
    state.blend = true;
    state.alpha = gfx::GsBlend{0, 1, 0, 1, 0};
    textwipe::Draw2D(gfx::Primitive::Quads, quad, ref.binding, state);
    if (with_lettering && sprite.texture == &nl_tex && texel.x == 0 && texel.y == 0 &&
        texel.width == static_cast<int>(sprite.texture->width) && texel.height == static_cast<int>(sprite.texture->height)) {
        areacard::Draw(x0, y0, x1, y1, static_cast<float>(sprite.alpha) / 128.0f);
    }
}

void DrawSprite() {
    PortTextureRef ref = PortTextureFromCTexture(sprite.texture);
    if (!ref.valid) {
        return;
    }
    const bool full_card = sprite.texture == &nl_tex && sprite.texel.x == 0 && sprite.texel.y == 0 &&
                           sprite.texel.width == static_cast<int>(sprite.texture->width) &&
                           sprite.texel.height == static_cast<int>(sprite.texture->height);
    if (textwipe::Active() && full_card && areacard::Prepared() && painted_tbp != 0 && PortKeyLive(painted_tbp)) {
        const PortTextureRef painted = PortTextureFromTex0(static_cast<u_long>(painted_tbp) | (static_cast<u_long>(painted_cbp) << 37), 0x20);
        textwipe::Both([&](bool runtime) { DrawCardSprite(runtime || !painted.valid ? ref : painted, runtime); });
        return;
    }
    DrawCardSprite(ref, true);
}

// Retail draws from the VSync interrupt into its own double buffer, whatever the game is doing.
// Here a pump presents: never inside a frame the game has open, since a mode's Init can be
// between MGBeginFrame and MGEndFrame when it waits.
void PresentLoadingFrame() {
    if (end_flag != 0 || gfx::InFrame()) {
        return;
    }
    textwipe::Update();
    if (gfx::BeginFrame()) {
        const uint8_t black[4] = {0, 0, 0, 0x80};
        gfx::Clear(true, black, true, 0.0f);
        if (sprite.visible) {
            DrawSprite();
        }
        textwipe::DrawDivider();
    }
    gfx::EndFrame();
}

} // namespace

PORT_OVERRIDE int check_now_loading() {
    if (end_flag == 0) {
        ClockSyncV();
    }
    return end_flag;
}

PORT_OVERRIDE void wait_now_loading_vsync() {
    if (end_flag == 0) {
        clear_now_loading_vsync_end();

        do {
            ClockSyncV();
        } while (check_now_loading_vsync_end() == 0 && ClockGetTickCallback() == VSyncCallBack_Load);
    }
}

PORT_OVERRIDE void now_loading_off() {
    now_loding_off = 1;
}

PORT_OVERRIDE void init_now_loading(int title_number) {
    end_flag = 1;

    if (now_loding_off != 0) {
        now_loding_off = 0;
        return;
    }

    PortReleaseOwner(PortTextureOwner::Loading);
    painted_tbp = painted_cbp = 0;
    areacard::Clear();
    nl_tex.Initialize();
    nl_tex2.Initialize();
    sprite = LoadingSprite{};

    u_char *archive = Archive();
    int     archive_size;
    now_loding_flag = 0;

    std::string image_directory = "img";
    if (LanguageCode > LANG_JAPANESE) {
        image_directory += "_" + std::to_string(LanguageCode);
    }

    std::string path;
    card_name.clear();

    if (title_number < 5) {
        path = image_directory + "/mt0" + std::to_string(title_number + 1) + ".tm2";
    } else if (title_number < 100) {
        path = image_directory + "/mt" + std::to_string(title_number) + ".tm2";
    } else {
        path = image_directory + "/mt" + std::to_string(title_number - 99) + ".tm2";
    }

    if (title_number == 0x321) {
        path = image_directory + "/title.img";

        if (LoadFile2(path.data(), archive, &archive_size, 0) == 0) {
            return;
        }

        char sce_logo[] = "SCElogo";
        char l5_logo[] = "L5logo";
        LoadTexture(sce_logo, archive, &nl_tex, 8000, 10000);
        LoadTexture(l5_logo, archive, &nl_tex2, 9500, 10100);
    } else {
        if (path.empty()) {
            return;
        }

        if (LoadFile2(path.data(), archive, &archive_size, 0) == 0) {
            return;
        }

        card_name = std::filesystem::path(path).stem().string();
        LoadTexture(reinterpret_cast<TM2_head *>(archive), &nl_tex, 8000, 9000);
    }

    map_title_no = title_number;
    nl_start_cnt = GameFastLoad() ? 1 : 20;
    col_cnt = 0.0f;
    col_add = GameFastLoad() ? 64.0f : 1.0f;
    count = 0;
    logo_count = 0;
    end_flag = 0;
    now_loading_vsync_end = 1;
    MGInitVSyncCallBack(VSyncCallBack_Load);
    ClockSetTickCallback(VSyncCallBack_Load);
    ClockSetIdleHook(PresentLoadingFrame);
}

namespace {

// --fast-load, a test hook: the holds take two ticks.
int Hold(int ticks) {
    return GameFastLoad() ? 2 : ticks;
}

} // namespace

PORT_OVERRIDE int VSyncCallBack_Load(int field) {
    if (end_flag) {
        now_loading_vsync_end = 1;
        return 0;
    }

    if (nl_start_cnt == 0) {
        sprite.visible = false;

        if (map_title_no == 0x321) {
            if (logo_count == 0) {
                if (nl_tex.width >= 640) {
                    Show(&nl_tex, CRect_i_(0, 0x10, 0x280, 0x1C0), CRect_i_(0, 0, 0x280, 0x1C0),
                         static_cast<u_char>(static_cast<int>(col_cnt)));
                } else {
                    Show(&nl_tex, CRect_i_(0x60, 0xC0, 0x1C0, 0x40), CRect_i_(0, 0, 0x1C0, 0x40),
                         static_cast<u_char>(static_cast<int>(col_cnt)));
                }
            }

            if (logo_count == 1) {
                Show(&nl_tex2, CRect_i_(0x100, 0xA0, 0x80, 0x80), CRect_i_(0, 0, 0x80, 0x80),
                     static_cast<u_char>(static_cast<int>(col_cnt)));
            }

            if (count == 0) {
                col_cnt += col_add * 2.0f;
            }

            if (col_cnt > 128.0f) {
                count = Hold(220);
                col_cnt = 128.0f;
                col_add *= -1.0f;
            }

            if (col_cnt < 0.0f) {
                count = Hold(100);
                col_cnt = 0.0f;
                col_add *= -1.0f;

                if (logo_count == 1) {
                    end_flag = 1;
                }

                logo_count++;
            }

            count--;

            if (count < 0) {
                count = 0;
            }
        } else {
            Show(&nl_tex, CRect_i_(0x80, 0xA0, 0x180, 0x80), CRect_i_(0, 0, 0x180, 0x80),
                 static_cast<u_char>(static_cast<int>(col_cnt)));

            if (count == 0) {
                col_cnt += col_add;
            }

            if (col_cnt > 128.0f) {
                count = Hold(120);
                col_cnt = 128.0f;
                col_add *= -1.0f;
            }

            if (col_cnt < 0.0f) {
                end_flag = 1;
                col_cnt = 0.0f;
            }

            count--;

            if (count < 0) {
                count = 0;
            }
        }
    }

    nl_start_cnt--;

    if (nl_start_cnt < 0) {
        nl_start_cnt = 0;
    }

    DBuffID = !DBuffID;
    now_loading_vsync_end = 1;

    if (end_flag) {
        ClockSetIdleHook(nullptr);
    }

    return 0;
}

PORT_OVERRIDE void LoadTexture(char *name, u_char *archive, CTexture *texture, int image_address, int palette_address) {
    SetTextureInfo(texture, name, archive);
    Upload(texture);
}

PORT_OVERRIDE void LoadTexture(TM2_head *image, CTexture *texture, int image_address, int palette_address) {
    char name[] = "maptitle";
    SetTextureInfo(texture, name, image);
    Upload(texture);
}
