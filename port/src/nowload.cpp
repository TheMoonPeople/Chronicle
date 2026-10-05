#include "language.h"

#include <cstdint>
#include <cstdio>
#include <memory>

#include "dataread.hpp"
#include "gameloop.hpp"
#include "gfx/gfx.hpp"
#include "mainselect.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "nowload.hpp"
#include "platform/clock.hpp"
#include "rect.hpp"
#include "texture_port.hpp"
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
    sceGsTex0 *tex0 = (sceGsTex0 *) &texture->tex0;
    unsigned   tbp = 0;
    unsigned   cbp = 0;
    if (texture->image[0] != 0) {
        const u_char      *levels[1] = {(u_char *) texture->image[0]};
        PortDecodedTexture decoded;
        if (PortDecodeTexture(texture->bpp, texture->width, texture->height, levels, 1, (u_char *) texture->clut, 0,
                              false, decoded)) {
            tbp = PortCreateTexture(decoded, PortTextureOwner::Loading, &cbp);
        }
    }
    tex0->TBP0 = tbp;
    tex0->bits.tcc = 1;
    tex0->CBP = cbp;
}

// set2DSprite's alpha form: a GS sprite in 12.4 coordinates, MODULATE with 0x80 grey and the fade
// in alpha, blended (Cs - Cd) * As + Cd over the cleared frame.
void DrawSprite() {
    PortTextureRef ref = PortTextureFromCTexture(sprite.texture);
    if (!ref.valid) {
        return;
    }
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
    gfx::Draw2D(gfx::Primitive::Quads, quad, ref.binding, state);
}

// Retail draws from the VSync interrupt into its own double buffer, whatever the game is doing.
// Here a pump presents: never inside a frame the game has open, since a mode's Init can be
// between MGBeginFrame and MGEndFrame when it waits.
void PresentLoadingFrame() {
    if (end_flag != 0 || gfx::InFrame()) {
        return;
    }
    if (gfx::BeginFrame()) {
        const uint8_t black[4] = {0, 0, 0, 0x80};
        gfx::Clear(true, black, true, 0.0f);
        if (sprite.visible) {
            DrawSprite();
        }
    }
    gfx::EndFrame();
}

} // namespace

int check_now_loading() {
    if (end_flag == 0) {
        ClockSyncV();
    }
    return end_flag;
}

void wait_now_loading_vsync() {
    if (end_flag == 0) {
        clear_now_loading_vsync_end();

        do {
            ClockSyncV();
        } while (check_now_loading_vsync_end() == 0 && ClockGetTickCallback() == VSyncCallBack_Load);
    }
}

void now_loading_off() {
    now_loding_off = 1;
}

void init_now_loading(int title_number) {
    end_flag = 1;

    if (now_loding_off != 0) {
        now_loding_off = 0;
        return;
    }

    PortReleaseOwner(PortTextureOwner::Loading);
    nl_tex.Initialize();
    nl_tex2.Initialize();
    sprite = LoadingSprite{};

    u_char *archive = Archive();
    int     archive_size;
    now_loding_flag = 0;

    char image_directory[64] = "img";

    if (LanguageCode > LANG_JAPANESE) {
        std::snprintf(image_directory, sizeof image_directory, "img_%d", LanguageCode);
    }

    char path[64] = "";

    if (title_number < 5) {
        std::snprintf(path, sizeof path, "%s/mt0%d.tm2", image_directory, title_number + 1);
    } else if (title_number < 100) {
        std::snprintf(path, sizeof path, "%s/mt%d.tm2", image_directory, title_number);
    } else {
        std::snprintf(path, sizeof path, "%s/mt%d.tm2", image_directory, title_number - 99);
    }

    if (title_number == 0x321) {
        std::snprintf(path, sizeof path, "%s/title.img", image_directory);

        if (LoadFile2(path, archive, &archive_size, 0) == 0) {
            return;
        }

        char sce_logo[] = "SCElogo";
        char l5_logo[] = "L5logo";
        LoadTexture(sce_logo, archive, &nl_tex, 8000, 10000);
        LoadTexture(l5_logo, archive, &nl_tex2, 9500, 10100);
    } else {
        if (path[0] == '\0') {
            return;
        }

        if (LoadFile2(path, archive, &archive_size, 0) == 0) {
            return;
        }

        LoadTexture((TM2_head *) archive, &nl_tex, 8000, 9000);
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

int VSyncCallBack_Load(int field) {
    if (end_flag) {
        now_loading_vsync_end = 1;
        return 0;
    }

    if (nl_start_cnt == 0) {
        sprite.visible = false;

        if (map_title_no == 0x321) {
            if (logo_count == 0) {
                // Languages past the first two show a full-screen logo image instead.
                if (LanguageCode >= LANG_ENGLISH_UK) {
                    Show(&nl_tex, CRect_i_(0, 0x10, 0x280, 0x1C0), CRect_i_(0, 0, 0x280, 0x1C0), (u_char) (int) col_cnt);
                } else {
                    Show(&nl_tex, CRect_i_(0x60, 0xC0, 0x1C0, 0x40), CRect_i_(0, 0, 0x1C0, 0x40), (u_char) (int) col_cnt);
                }
            }

            if (logo_count == 1) {
                Show(&nl_tex2, CRect_i_(0x100, 0xA0, 0x80, 0x80), CRect_i_(0, 0, 0x80, 0x80), (u_char) (int) col_cnt);
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
            Show(&nl_tex, CRect_i_(0x80, 0xA0, 0x180, 0x80), CRect_i_(0, 0, 0x180, 0x80), (u_char) (int) col_cnt);

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

void LoadTexture(char *name, u_char *archive, CTexture *texture, int image_address, int palette_address) {
    SetTextureInfo(texture, name, archive);
    Upload(texture);
}

void LoadTexture(TM2_head *image, CTexture *texture, int image_address, int palette_address) {
    char name[] = "maptitle";
    SetTextureInfo(texture, name, image);
    Upload(texture);
}
