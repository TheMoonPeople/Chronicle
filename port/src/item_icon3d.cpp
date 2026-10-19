#include "item_icon3d.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <vector>

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "battlemenu.hpp"
#include "dngstatusdata.hpp"
#include "btitem.hpp"
#include "collision.hpp"
#include "btmisc.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gfx/gfx.hpp"
#include "itemdata.hpp"
#include "memcard.hpp"
#include "mds.hpp"
#include "menu_draw.hpp"
#include "normal_smoothing.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "texture_port.hpp"

namespace {

constexpr int         kIconSize = 32;
constexpr int         kCrop = 128;  // scratch pixels turned into one icon (4x4 each)
constexpr float       kFill = 120.0f; // pixels across the model inside the crop
constexpr int         kBlock = 0x47; // a texture block nothing else loads into
constexpr std::size_t kModelBytes = 8u << 20;
constexpr std::size_t kImageBytes = 4u << 20;
constexpr std::size_t kArenaBytes = 8u << 20;
constexpr const char *kScratch = "item icon 3d scratch";

bool EnvOn(const char *name) {
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != 0 && value[0] != '0';
}

bool Debug() {
    static const bool on = EnvOn("DC_ITEM_ICONS_3D_DEBUG");
    return on;
}

struct Icon {
    std::unique_ptr<CTexture> texture;
    bool                      failed = false;
};

std::map<int, Icon> g_icons;

u_long Tex0(unsigned tbp, int tbw, int psm, int tw, int th) {
    return (u_long) tbp | ((u_long) tbw << 14) | ((u_long) psm << 20) | ((u_long) tw << 26) | ((u_long) th << 30) |
           ((u_long) 1 << 34);
}

// Buffers the files are read into and the model is built in; kept for every icon, since only one is drawn at a time.
struct Work {
    std::vector<uint8_t> storage;
    uint8_t             *model = nullptr;
    uint8_t             *image = nullptr;
    uint8_t             *arena_base = nullptr;

    Work() {
        storage.resize(kModelBytes + kImageBytes + kArenaBytes + 192);
        auto align = [](uint8_t *p) { return reinterpret_cast<uint8_t *>((reinterpret_cast<uintptr_t>(p) + 63) & ~uintptr_t{63}); };
        model = align(storage.data());
        image = align(model + kModelBytes);
        arena_base = align(image + kImageBytes);
    }
};

Work &GetWork() {
    static Work work;
    return work;
}

CFrame *g_frame = nullptr; // the model
CFrame *g_root = nullptr;  // the frame the picture is turned, scaled and placed by, with the model its child
std::string g_last_name;

// The icon's frame is drawn into a target of its own, so the screen-bound test (made against the visible extent) is
// switched off for every frame of it.
void NoCull(CFrame *frame, int &count) {
    for (; frame != nullptr; frame = frame->brother) {
        frame->attr.cull_enable = 0;
        count++;
        NoCull(frame->child, count);
    }
}

// 0: the lit model; 1: the same model unlit, which is black and makes the outline and shadow.
int g_pass = 0;

float EnvFloat(const char *name, float fallback) {
    const char *text = std::getenv(name);
    return text != nullptr ? static_cast<float>(std::atof(text)) : fallback;
}

// Drawn by MenuPolygonDraw, which has set the menu camera's position and the lights (and puts them back after).
void DrawModel() {
    MenuWorldTrans(&MenuCamera);
    if (g_pass == 1) {
        float none_dir[4][4] = {};
        float none_colour[4][4] = {};
        float dark[4] = {0.0f, 0.0f, 0.0f, 128.0f};
        MGSetPLight(none_dir, none_colour);
        MGSetAmbient(dark);
    } else {
        // A key light from the upper left, a weak fill from the other side, and enough ambient that the shade side
        // stays a colour: the retail icons are painted with a highlight on the top left and shade bottom right.
        float key[3] = {-0.45f, 0.75f, 0.5f};
        float fill[3] = {0.6f, -0.2f, 0.4f};
        float key_length = std::sqrt(key[0] * key[0] + key[1] * key[1] + key[2] * key[2]);
        float fill_length = std::sqrt(fill[0] * fill[0] + fill[1] * fill[1] + fill[2] * fill[2]);
        float dir[4][4] = {};
        float colour[4][4] = {};
        for (int i = 0; i < 3; i++) {
            dir[i][0] = key[i] / key_length;
            dir[i][1] = fill[i] / fill_length;
        }
        const float key_strength = EnvFloat("DC_ITEM_ICONS_3D_KEY", 120.0f);
        const float fill_strength = EnvFloat("DC_ITEM_ICONS_3D_FILL_LIGHT", 36.0f);
        colour[0][0] = colour[0][1] = colour[0][2] = key_strength;
        colour[1][0] = colour[1][1] = colour[1][2] = fill_strength;
        float ambient[4] = {EnvFloat("DC_ITEM_ICONS_3D_AMBIENT", 70.0f), 0.0f, 0.0f, 128.0f};
        ambient[1] = ambient[2] = ambient[0];
        MGSetPLight(dir, colour);
        MGSetAmbient(ambient);
    }
    MGDraw(g_root);
}

// The model's file names, or false where the item has none of its own (an empty slot of the table falls back to the
// water bottle in retail, and that is not this item's picture).
bool ModelPaths(int item_no, char *model_path, char *texture_path) {
    if (item_no < ITEM_ATTACH_START) {
        item_no = TransWepNo(item_no);
    }
    if (item_no < ITEM_ATTACH_START) {
        return false;
    }
    if (item_no < ITEM_WEAPON_START) {
        const char *name = ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START];
        if (name == nullptr || name[0] == 0) {
            return false;
        }
    }
    BtGetItemNamePath(model_path, texture_path, item_no);
    return true;
}

bool Render(int item_no, CTexture &out) {
    char model_path[64];
    char texture_path[76];
    if (!ModelPaths(item_no, model_path, texture_path)) {
        if (Debug()) {
            std::fprintf(stderr, "item icon 3d: item %d has no model\n", item_no);
        }
        return false;
    }
    Work &work = GetWork();
    int   size = 0;
    if (Debug()) {
        std::fprintf(stderr, "item icon 3d: item %d reads %s and %s\n", item_no, model_path, texture_path);
    }
    if (!LoadFile2(model_path, work.model, &size, 0) || size <= 0 || static_cast<std::size_t>(size) + 2048 > kModelBytes) {
        return false;
    }
    if (!LoadFile2(texture_path, work.image, &size, 0) || size <= 0 || static_cast<std::size_t>(size) + 2048 > kImageBytes) {
        return false;
    }

    char name[48];
    std::snprintf(name, sizeof name, "item icon 3d %d", item_no);
    unsigned key = PortCreatePlaceholder(name, kIconSize, kIconSize, 4, PortTextureOwner::Other, nullptr);
    gfx::TextureHandle target = gfx::FindNamedRenderTarget(name);
    gfx::TextureHandle scratch = gfx::NamedRenderTarget(kScratch, 640, 480, true);
    if (key == 0 || target == gfx::kNullTexture || scratch == gfx::kNullTexture) {
        if (Debug()) {
            std::fprintf(stderr, "item icon 3d: item %d: no target (key %u)\n", item_no, key);
        }
        return false;
    }

    // Smooth the model's normals while it is built and drawn here, so a rounded thing is lit as a curve (see
    // normal_smoothing.hpp); DC_ITEM_ICONS_3D_SMOOTH is the crease angle in degrees, 0 for retail's flat shading.
    struct SmoothScope {
        float previous;
        SmoothScope() : previous(PortSetNormalSmoothing(EnvFloat("DC_ITEM_ICONS_3D_SMOOTH", 60.0f))) {}
        ~SmoothScope() { PortSetNormalSmoothing(previous); }
    } smooth_scope;
    SetTempTexture(kBlock, reinterpret_cast<char *>(work.image));
    CDataAlloc2<1> arena(-1);
    arena.base = work.arena_base;
    arena.limit = static_cast<int>(kArenaBytes >> 4);
    arena.Reset();
    g_frame = reinterpret_cast<CFrame *>(LoadMDSFile(reinterpret_cast<u_int *>(work.model), &arena, 0, nullptr, nullptr));
    if (g_frame == nullptr) {
        TexManager.DeleteTextureBlock(kBlock);
        return false;
    }
    // Frame the model: centre its bound on the origin, turn it so its long side runs corner to corner, and scale it
    // so the bound's sphere fills kFill pixels of the scratch target.
    CBoxVu0 box;
    g_frame->GetBoundBox(&box, 1);
    float ext[3], mid[4];
    for (int i = 0; i < 3; i++) {
        ext[i] = box.max[i] - box.min[i];
        mid[i] = (box.max[i] + box.min[i]) * 0.5f;
    }
    mid[3] = 1.0f;
    float radius = 0.5f * std::sqrt(ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2]);
    if (!(radius > 0.0001f)) {
        radius = 1.0f;
    }
    // The pose: the one the treasure chest presents the item in, which depends on who the weapon belongs to
    // (BtGetTreasureboxBig_Init); everything else is shown as the chest shows a consumable. PORT_ICON_POSE=shape
    // keeps the older rule that turns the long side onto a diagonal.
    float rx = 1.5707964f, ry = 0.0f, rz = 1.5707964f;
    const bool shape_pose = EnvOn("DC_ITEM_ICONS_3D_POSE_SHAPE");
    if (shape_pose) {
        rx = 0.35f;
        ry = 0.0f;
        rz = 0.785f;
        if (ext[2] > ext[0] && ext[2] > ext[1]) {
            rx = 1.2f;
        }
    } else if (BtlMenuStatusPt != nullptr) {
        switch (reinterpret_cast<CDngStatusData *>(BtlMenuStatusPt)->CheckWeaponUser(item_no)) {
            case CHARA_XIAO:
                rx = -1.5707964f;
                ry = -1.5707964f;
                rz = 0.0f;
                break;
            case CHARA_OSMOND:
                rx = -1.5707964f;
                ry = 3.1415927f;
                rz = 0.0f;
                break;
            case CHARA_RUBY:
                rx = 0.0f;
                ry = 1.5707964f;
                rz = 0.0f;
                break;
            default:
                break;
        }
    }
    if (Debug()) {
        std::fprintf(stderr, "item icon 3d: item %d pose %.2f %.2f %.2f\n", item_no, rx, ry, rz);
    }
    g_frame->SetRotation(rx, ry, rz);
    // The pose is the chest's, seen by the chest's camera. The picture's own turns sit on top of it, one frame per
    // turn so each is about the axis it names: stand it up, turn it about the vertical, tip it to look from a little
    // above, and last roll it in the picture. Scale and place are on the outermost.
    static CFrameVu1 stand, yaw, elev, root;
    CFrameVu1       *chain[4] = {&stand, &yaw, &elev, &root};
    for (CFrameVu1 *frame : chain) {
        frame->visual = nullptr;
        frame->SetChild(nullptr);
    }
    g_frame->parent = nullptr;
    root.SetChild(&elev);
    elev.SetChild(&yaw);
    yaw.SetChild(&stand);
    stand.SetChild(g_frame);
    g_root = &root;
    const bool weapon = item_no >= ITEM_WEAPON_START;
    float      stand_x = weapon ? -1.15f : -1.5707964f;
    float      tip = weapon ? 0.0f : 0.35f;
    if (const char *text = std::getenv("DC_ITEM_ICONS_3D_TIP")) {
        tip = static_cast<float>(std::atof(text));
    }
    float turn_now = 0.5f, roll = 0.0f;
    auto  apply = [&]() {
        stand.SetRotation(stand_x, 0.0f, 0.0f);
        yaw.SetRotation(0.0f, turn_now, 0.0f);
        elev.SetRotation(tip, 0.0f, 0.0f);
        root.SetRotation(0.0f, 0.0f, roll);
    };
    apply();
    root.SetScale(1.0f, 1.0f, 1.0f);
    float origin[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    root.SetPosition(origin);
    // The picture's extent. A box's eight corners overstate a rounded thing (a bottle or fruit is nearer an ellipsoid
    // in its box), so the extent is the mean of the box's and the ellipsoid's reach along each picture axis.
    float rotated[4] = {0, 0, 0, 1};
    float lo[3], hi[3];
    auto  measure = [&]() {
        float o[4] = {0, 0, 0, 1}, wo[4], we[3][4];
        g_frame->GetWorldPosition(wo, o);
        for (int i = 0; i < 3; i++) {
            float e[4] = {i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f, 1.0f};
            g_frame->GetWorldPosition(we[i], e);
        }
        float mid_local[4] = {mid[0], mid[1], mid[2], 1.0f}, centre_world[4];
        g_frame->GetWorldPosition(centre_world, mid_local);
        for (int k = 0; k < 3; k++) {
            float sum_box = 0.0f, sum_sq = 0.0f;
            for (int i = 0; i < 3; i++) {
                const float reach = 0.5f * ext[i] * (we[i][k] - wo[k]);
                sum_box += std::fabs(reach);
                sum_sq += reach * reach;
            }
            const float half = k == 2 ? 0.0f : 0.5f * (sum_box + std::sqrt(sum_sq));
            lo[k] = centre_world[k] - half;
            hi[k] = centre_world[k] + half;
        }
    };
    measure();
    // A plate (a medallion, a scroll, a tablet): its broad face should face us, whichever way the chest pose lays it.
    float sorted_ext[3] = {ext[0], ext[1], ext[2]};
    std::sort(sorted_ext, sorted_ext + 3);
    const bool plate = !weapon && sorted_ext[2] > 0.0001f && sorted_ext[0] < 0.6f * sorted_ext[1] && sorted_ext[1] > 0.5f * sorted_ext[2];
    if (plate && !shape_pose) {
        float best = -1.0f, best_stand = stand_x, best_turn = turn_now;
        for (int quarter = -2; quarter <= 2; quarter++) {
            for (int step = -6; step <= 6; step++) {
                stand_x = quarter * 1.5707964f;
                turn_now = step * 0.2618f;
                apply();
                measure();
                const float area = (hi[0] - lo[0]) * (hi[1] - lo[1]);
                const float score = area * (1.0f - 0.01f * std::fabs(turn_now) - 0.01f * std::fabs(stand_x + 1.5707964f));
                if (score > best) {
                    best = score;
                    best_stand = stand_x;
                    best_turn = turn_now;
                }
            }
        }
        stand_x = best_stand;
        turn_now = best_turn;
        tip = 0.0f;
        apply();
        measure();
    }
    // The turn about the vertical: the chest pose keeps the item upright but not facing us, so a flat thing can be edge-on
    // here. Try a spread of turns and keep the one whose box shows the most (the widest face), leaning to the usual
    // three-quarter turn when two are as good.
    if (!shape_pose && !plate && !EnvOn("DC_ITEM_ICONS_3D_NO_TURN_SEARCH")) {
        float best_score = -1.0f, best_turn = turn_now;
        for (int step = -6; step <= 6; step++) {
            turn_now = step * 0.2618f; // 15 degrees
            apply();
            measure();
            const float area = (hi[0] - lo[0]) * (hi[1] - lo[1]);
            const float score = area * (1.0f - 0.04f * std::fabs(turn_now - 0.5f));
            if (score > best_score) {
                best_score = score;
                best_turn = turn_now;
            }
        }
        turn_now = best_turn;
        // Everything but weapons reads better turned a little more to the left than the widest face has it.
        if (!weapon) {
            float offset = -0.2618f;
            if (const char *text = std::getenv("DC_ITEM_ICONS_3D_ITEM_TURN")) {
                offset = static_cast<float>(std::atof(text));
            }
            turn_now += offset;
        }
        apply();
        measure();
    }
    // Long and thin as seen (weapons, keys, rods): lay it along the diagonal, which is how the retail icons draw them,
    // and which fills a square cell the best.
    const float width = hi[0] - lo[0], height = hi[1] - lo[1];
    if (!shape_pose && (width > 2.0f * height || height > 2.0f * width)) {
        roll += width > height ? 0.785f : -0.785f;
        apply();
        measure();
    }
    for (int i = 0; i < 3; i++) {
        rotated[i] = (lo[i] + hi[i]) * 0.5f;
    }
    radius = 0.5f * std::max(hi[0] - lo[0], hi[1] - lo[1]);
    if (!(radius > 0.0001f)) {
        radius = 1.0f;
    }

    RenderInfo saved = mgRenderInfo;
    const gfx::TextureHandle previous = gfx::CurrentRenderTarget();
    gfx::SetRenderTarget(scratch);
    const uint8_t none[4] = {};

    // Pixels per world unit at the origin, from one trial draw, which also puts the camera in place for the projection.
    static float pixels_per_unit = 0.0f;
    if (pixels_per_unit <= 0.0f) {
        gfx::Clear(true, none, true, 0.0f);
        MenuPolygonDraw(0x80, DrawModel);
        float a[4] = {0, 0, 0, 1}, c[4] = {10.0f, 0, 0, 1};
        int   pa[4], pc[4];
        MGRotTransPers2D(pa, a, 0);
        MGRotTransPers2D(pc, c, 0);
        pixels_per_unit = std::fabs(static_cast<float>(pc[0] - pa[0])) / 10.0f;
        if (pixels_per_unit <= 0.0f) {
            pixels_per_unit = 3.0f;
        }
    }
    int frames = 0;
    NoCull(g_frame, frames);
    if (Debug()) {
        std::fprintf(stderr, "item icon 3d: %d frames, draw_on %d, max %g,%g,%g\n", frames, g_frame->attr.draw_on, g_frame->max[0], g_frame->max[1], g_frame->max[2]);
    }
    const float scale = (kFill * 0.5f) / (pixels_per_unit * radius);
    root.SetScale(scale, scale, scale);
    float position[4] = {-rotated[0] * scale, -rotated[1] * scale, -rotated[2] * scale, 1.0f};
    root.SetPosition(position);
    const uint8_t red[4] = {0x80, 0, 0, 0x80};
    gfx::Clear(true, EnvOn("DC_ITEM_ICONS_3D_RED") ? red : none, true, 0.0f);
    if (!EnvOn("DC_ITEM_ICONS_3D_NO_OUTLINE")) {
        // The outline and shadow: the unlit (black) model drawn shifted about the picture, a pixel of the icon (4 of
        // the scratch's) each way and a little more down and right for the shadow, then the lit model on top.
        const float step = 4.0f / pixels_per_unit;
        const float shifts[9][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-0.75f, -0.75f}, {0.75f, -0.75f},
                                    {-0.75f, 0.75f}, {0.75f, 0.75f}, {1.4f, -1.6f}};
        g_pass = 1;
        for (const auto &shift : shifts) {
            float moved[4] = {position[0] + shift[0] * step, position[1] + shift[1] * step, position[2], 1.0f};
            root.SetPosition(moved);
            MenuPolygonDraw(0x80, DrawModel);
        }
        g_pass = 0;
        root.SetPosition(position);
        gfx::Clear(false, none, true, 0.0f);
    }
    MenuPolygonDraw(0x80, DrawModel);
    int centre[2] = {320, 240};
    Get3DPosTo2DPos(g_root, centre);
    // Supersample down in exact halves: kCrop -> kCrop/2 -> the icon.
    gfx::TextureHandle half = gfx::NamedRenderTarget("item icon 3d half", kCrop / 2, kCrop / 2, true);
    const gfx::Rect from = {320 - kCrop / 2, 240 - kCrop / 2, kCrop, kCrop};
    if (half != gfx::kNullTexture) {
        gfx::SetRenderTarget(previous);
        gfx::BlitTexture(scratch, from, half, {0, 0, kCrop / 2, kCrop / 2}, gfx::Filter::Linear);
        gfx::BlitTexture(half, {0, 0, kCrop / 2, kCrop / 2}, target, {0, 0, kIconSize, kIconSize}, gfx::Filter::Linear);
    } else {
        gfx::SetRenderTarget(previous);
        gfx::BlitTexture(scratch, from, target, {0, 0, kIconSize, kIconSize}, gfx::Filter::Linear);
    }
    mgRenderInfo = saved;
    TexManager.DeleteTextureBlock(kBlock);
    root.SetChild(nullptr);
    g_frame = nullptr;
    g_root = nullptr;
    g_last_name = name;
    if (Debug()) {
        std::fprintf(stderr, "item icon 3d: item %d %s radius %.2f scale %.3f ppu %.2f centre %d,%d\n", item_no, model_path,
                     radius, scale, pixels_per_unit, centre[0], centre[1]);
    }

    out.block = -1;
    out.width = kIconSize;
    out.height = kIconSize;
    out.bpp = 4;
    std::snprintf(out.name, sizeof out.name, "%s", name);
    out.tex0 = Tex0(key, kIconSize / 64 + 1, SCE_GS_PSMCT32, 5, 5);
    return true;
}

} // namespace

bool ItemIcons3DEnabled() {
    static const bool on = EnvOn("DC_ITEM_ICONS_3D");
    return on;
}

CTexture *ItemIcon3DTexture(int item_no) {
    if (!ItemIcons3DEnabled()) {
        return nullptr;
    }
    if (!gfx::InFrame()) {
        if (Debug()) {
            static int said = 0;
            if (said++ < 5) {
                std::fprintf(stderr, "item icon 3d: item %d asked outside a frame, sheet icon for now\n", item_no);
            }
        }
        return nullptr; // drawing is dropped outside a frame; ask again next time
    }
    Icon &icon = g_icons[item_no];
    if (icon.failed) {
        return nullptr;
    }
    if (!icon.texture) {
        auto texture = std::make_unique<CTexture>();
        if (!Render(item_no, *texture)) {
            icon.failed = true;
            return nullptr;
        }
        icon.texture = std::move(texture);
    }
    return icon.texture.get();
}

void ItemIcons3DDrawSheet() {
    const char *page = std::getenv("DC_ITEM_ICONS_3D_SHEET");
    if (page == nullptr || !ItemIcons3DEnabled() || !gfx::InFrame()) {
        return;
    }
    static std::set<int> built;
    const int            index = std::atoi(page);
    if (!built.insert(index).second) {
        return;
    }
    // A contact sheet of the page's icons in a target of its own, for the debug dump to write out.
    gfx::TextureHandle sheet = gfx::NamedRenderTarget("item icon 3d sheet", 640, 448, true);
    if (sheet == gfx::kNullTexture) {
        return;
    }
    const int first = ITEM_ATTACH_START + index * 280;
    for (int i = 0; i < 280; i++) {
        if (ItemIcon3DTexture(first + i) == nullptr) {
            continue;
        }
        char name[48];
        std::snprintf(name, sizeof name, "item icon 3d %d", first + i);
        gfx::TextureHandle icon = gfx::FindNamedRenderTarget(name);
        if (icon != gfx::kNullTexture) {
            gfx::BlitTexture(icon, {0, 0, kIconSize, kIconSize}, sheet,
                             {(i % 20) * kIconSize, (i / 20) * kIconSize, kIconSize, kIconSize}, gfx::Filter::Nearest);
        }
    }
}

// Retail's lookup of the sheet cell an item's icon is in, with the model's own icon in front of it.
PORT_OVERRIDE CTexture *RetCTex(short item_no, int &u, int &v) {
    if (Debug()) {
        static bool said = false;
        if (!said) {
            said = true;
            std::fprintf(stderr, "item icon 3d: RetCTex first asked for item %d, enabled=%d\n", item_no, ItemIcons3DEnabled());
        }
    }
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (EnvOn("DC_ITEM_ICONS_3D_FILL") && BtlMenuStatusPt != nullptr) {
        // Debug: put a spread of items in the bag once, so the menu's grid has icons to show.
        static bool filled = false;
        if (!filled) {
            filled = true;
            CDngStatusData *status = reinterpret_cast<CDngStatusData *>(BtlMenuStatusPt);
            for (int id = 132; id < 160; id++) {
                status->GetItem(id, 1);
            }
            for (int id = 81; id < 100; id++) {
                status->GetItem(id, 1);
            }
        }
    }

    if (info == nullptr) {
        return nullptr;
    }

    int icon = info->icon_index;

    if (icon < 0) {
        return nullptr;
    }

    if (std::getenv("DC_ITEM_ICONS_3D_SHEET") != nullptr) {
        ItemIcons3DDrawSheet(); // every ask, so the last one is on top of the menu panels
    }

    if (CTexture *model = ItemIcon3DTexture(item_no)) {
        u = 0;
        v = 0;
        return model;
    }

    u = ((icon + 8) % 8) << 5;
    v = (icon >> 3) << 5;

    switch (info->kind) {
        case ITEMKIND_ITEM:
            return ItemIcon;
        case ITEMKIND_ATTACH:
        case ITEMKIND_WEAPON:
            return WepIcon;
    }

    return nullptr;
}

// Debug: with DC_ITEM_ICONS_3D_DUMP=<dir>, the scratch render and the newest icon are written there. Called outside a frame.
void ItemIcons3DDumpDebug() {
    const char *dir = std::getenv("DC_ITEM_ICONS_3D_DUMP");
    if (dir == nullptr) {
        return;
    }
    auto write = [&](const char *target_name, const char *file) {
        gfx::TextureHandle texture = gfx::FindNamedRenderTarget(target_name);
        std::vector<uint8_t> rgba;
        uint32_t width = 0, height = 0;
        if (texture != gfx::kNullTexture && gfx::ReadbackTexture(texture, rgba, width, height)) {
            gfx::WritePng(std::filesystem::path(dir) / file, rgba.data(), width, height);
        }
    };
    write("item icon 3d sheet", "sheet.png");
    write(kScratch, "scratch.png");
    write("item icon 3d half", "half.png");
    if (!g_last_name.empty()) {
        write(g_last_name.c_str(), "icon.png");
    }
}
