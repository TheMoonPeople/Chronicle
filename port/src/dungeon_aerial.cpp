#include "dungeon_aerial.hpp"

#include <SDL3/SDL.h>
#include "dungeon_aerial_cache.hpp"
#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "character.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "btactstatus.hpp"
#include "draw2d_port.hpp"
#include "dungeonparts.hpp"
#include "edit.hpp"
#include "frame.hpp"
#include "gfx/gfx.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "monstorunit.hpp"
#include "platform/config.hpp"
#include "rect.hpp"
#include "renderinfo.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "textwipe.hpp"
#include "userstatus.hpp"

// ---- Aerial map ------------------------------------------------------------------------------
//
// The dungeon map is the live floor seen from straight above. The near clip plane sits
// aerial_map_ceiling above the player's feet, so ceilings and upper walls are cut away. Only the
// cells the retail map shows are copied onto it (CDungeonMap::mask, or every cell dimmed while the
// map item is equipped); monsters, chests and atla balls appear as their own models under the rules
// retail's symbols follow, and the player as their model.

namespace {

constexpr float kCell = 160.0f;          // world units per grid cell
constexpr float kRetailScale = 0.1f;     // retail map pixels per world unit (16 per cell)
// Retail's map uses 16 pixels per grid cell at 380,64; game.aerial_map_size grows it from its
// top-right corner, moving up as far as the frame allows.
float kMapLeft = 380.0f;
float kMapTop = 64.0f;
float kMapSize = 320.0f;

void PlaceMap(float size) {
    kMapSize = 320.0f * size;
    kMapTop = std::max(8.0f, std::min(64.0f, 472.0f - kMapSize));
    kMapSize = std::min(kMapSize, 472.0f - kMapTop);
    kMapLeft = 632.0f - kMapSize;
}
constexpr float kFloorCentre = 9.5f * kCell;
constexpr float kAerialHeight = 4000.0f; // camera height above the player's feet
constexpr float kAerialDepth = 4000.0f;  // how far below the feet the floor is still drawn
constexpr float kTargetFill = 448.0f;    // target rows the map's span fills at zoom 1

bool g_drawn = false;

struct Aerial {
    float zoom = 1.0f;
    float map_x = 0.0f; // map pixel of world x = 0, z = 0
    float map_y = 0.0f;
    float u0 = 0.0f; // target texel of world x = 0, z = 0 on the floor
    float v0 = 0.0f;
    float du = 0.0f; // target texels per world unit along x, z
    float dv = 0.0f;
};

float Det3(const sceVu0FMATRIX m) {
    return m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) - m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2]) +
           m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]);
}

int CellOf(const float *pos) {
    int col = (int) ((80.0f + pos[0]) / kCell);
    int row = (int) ((80.0f + pos[2]) / kCell);

    if (col < 0 || row < 0 || col >= dungeon_grid::side || row >= dungeon_grid::side) {
        return -1;
    }

    return col + row * dungeon_grid::side;
}

// The aerial view, scaled by scale about pivot so a model draws larger where it stands without its
// own matrices changing.
void SetView(const sceVu0FMATRIX base, const float *eye, const float *pivot, float scale) {
    sceVu0FMATRIX view;
    sceVu0CopyMatrix(view, const_cast<float(*)[4]>(base));

    if (pivot != nullptr) {
        // view * T(pivot) * S(scale) * T(-pivot)
        for (int row = 0; row < 3; row++) {
            float moved = base[0][row] * pivot[0] + base[1][row] * pivot[1] + base[2][row] * pivot[2];

            for (int col = 0; col < 3; col++) {
                view[col][row] = base[col][row] * scale;
            }

            view[3][row] = base[3][row] + moved * (1.0f - scale);
        }
    }

    MGPortSetViewMatrixNoCut(view, const_cast<float *>(eye));
}

} // namespace

bool DungeonAerialMapDrawn() {
    return g_drawn;
}

namespace {

// What the map drew this frame, ringed in its colour over the aerial image so it reads at map size.
struct Marker {
    float  x;
    float  z;
    u_char r, g, b;
    bool   pulse;
};
std::vector<Marker> g_markers;

void AddMarker(const float *pos, u_char r, u_char g, u_char b, bool pulse = true) {
    g_markers.push_back({pos[0], pos[2], r, g, b, pulse});
}

bool CellShown(CDungeonMap *map, int cell, int show_all) {
    return cell >= 0 && map->cells[cell].parts_no != MAP_PARTS_NONE && (show_all || map->mask[cell] != 0);
}

// The overhead camera's height, in 80-unit steps: walking over bumpy ground must not change the
// cached floor image (its key includes this), or the whole floor is redrawn on most ticks.
float AerialEyeHeight(float feet) { return 80.0f * std::round(feet / 80.0f); }

void DrawModels(CDungeonMap *map, const sceVu0FMATRIX base, const float *eye, const float *player, float scale, int crystal,
                int show_all, int pass) {
    float lid[4];

    // Chests: retail's symbol shows a closed box on a revealed cell, or all of them with the crystal.
    if (pass == 0 && map->box_lid_model != NULL && map->box_body_model != NULL && map->chest_body_model != NULL &&
        map->chest_lid_model != NULL) {
        TexManager.ReloadTexture(Vif1Packet, 3);

        for (int i = 0; i < dungeon_grid::boxes; i++) {
            TREASURE_BOX *box = &map->boxes[i];
            int           cell = CellOf(box->pos);

            if (box->used == 0 || box->closed == 0 || cell < 0 || map->cells[cell].parts_no == MAP_PARTS_NONE ||
                !(crystal || map->mask[cell] != 0)) {
                continue;
            }

            if (box->kind != TREASURE_BOX_LARGE && box->kind != TREASURE_BOX_SMALL) {
                continue;
            }

            const bool large = box->kind == TREASURE_BOX_LARGE;
            CFrame    *body = large ? map->box_body_model : map->chest_body_model;
            CFrame    *top = large ? map->box_lid_model : map->chest_lid_model;
            SetView(base, eye, box->pos, scale);
            body->SetPosition(box->pos);
            sceVu0CopyVector(lid, body->position);
            lid[1] += large ? 8.0f : 3.0f;
            lid[2] -= large ? 5.0f : 3.0f;
            top->SetPosition(lid);
            top->SetRotation((PI * box->lid_angle) / 180.0f, 0.0f, 0.0f);
            MGDraw(body);
            MGDraw(top);
            AddMarker(box->pos, 0xFF, 0xC0, 0x20);
        }
    }

    // Atla balls: as their symbols, on a revealed cell or with the crystal.
    if (pass == 0 && map->atra_model != NULL && atraGetStatus == 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x16);

        for (int i = 0; i < 8; i++) {
            int cell = CellOf(map->atra[i].pos);

            if (map->atra[i].used == 0 || cell < 0 || map->cells[cell].parts_no == MAP_PARTS_NONE ||
                !(crystal || map->mask[cell] != 0)) {
                continue;
            }

            SetView(base, eye, map->atra[i].pos, scale);
            map->atra_model->SetPosition(map->atra[i].pos);
            MGDraw(map->atra_model);
            AddMarker(map->atra[i].pos, 0x40, 0xC0, 0xFF);
        }
    }

    // Monsters: CMonstorUnit::DrawMapSymbol's test, an awake monster or every one with the crystal.
    CMonstorUnit *unit = NowMonstorUnit;

    if (unit != NULL && CMonUnitHyde == 0 && BtEventMode == 0) {
        float          pos[4];
        sceVu0FVECTOR  ambient;
        const int      saved_current = unit->current_monster;

        MGGetAmbient(ambient);
        TexManager.ReloadTexture(Vif1Packet, 42);

        for (int i = 0; i < std::extent_v<decltype(CMonstorUnit::monster)>; i++) {
            MONSTOR *monster = &unit->monster[i];

            if (monster->state == -1 || monster->revealed == 0 || !(crystal || monster->state == 2)) {
                continue;
            }
            if ((monster->state == 2) != (pass == 1)) {
                continue;
            }

            CCharacter *chara = &unit->chara[i][0];
            chara->GetPosition(pos);

            if (!CellShown(map, CellOf(pos), show_all || crystal)) {
                continue;
            }

            unit->current_monster = i;
            unit->PalletSet();
            SetView(base, eye, pos, scale);
            chara->Draw();
            MGSetAmbient(ambient);
            AddMarker(pos, 0xFF, 0x20, 0x20);
        }

        unit->current_monster = saved_current;
    }

    // The player.
    if (pass == 1 && BtActStatus.player_visible != 0 && EdEventInfo.player_draw != 0) {
        // Larger than the rest and fully lit, so the player reads at a glance from straight above.
        sceVu0FVECTOR saved_ambient;
        sceVu0FVECTOR bright = {128.0f, 128.0f, 128.0f, 128.0f};
        MGGetAmbient(saved_ambient);
        MGSetAmbient(bright);
        SetView(base, eye, player, scale * 1.8f);
        Draw_MainUnit();
        MGSetAmbient(saved_ambient);
    }
}

// The things the map marks with a dot, by retail's symbol rules: closed chests and Atla on a
// revealed cell (every one with the crystal), awake monsters (every one with the crystal).
void CollectMarkers(CDungeonMap *map, int crystal, int show_all) {
    g_markers.clear();
    for (int i = 0; i < dungeon_grid::boxes; i++) {
        const TREASURE_BOX *box = &map->boxes[i];
        const int cell = CellOf(box->pos);
        if (box->used == 0 || box->closed == 0 || cell < 0 || map->cells[cell].parts_no == MAP_PARTS_NONE ||
            !(crystal || map->mask[cell] != 0) || (box->kind != TREASURE_BOX_LARGE && box->kind != TREASURE_BOX_SMALL)) {
            continue;
        }
        AddMarker(box->pos, 0xFF, 0xC0, 0x20);
    }
    if (atraGetStatus == 0) {
        for (int i = 0; i < 8; i++) {
            const int cell = CellOf(map->atra[i].pos);
            if (map->atra[i].used == 0 || cell < 0 || map->cells[cell].parts_no == MAP_PARTS_NONE ||
                !(crystal || map->mask[cell] != 0)) {
                continue;
            }
            AddMarker(map->atra[i].pos, 0x40, 0xC0, 0xFF);
        }
    }
    CMonstorUnit *unit = NowMonstorUnit;
    if (unit != NULL && CMonUnitHyde == 0 && BtEventMode == 0) {
        float pos[4];
        for (int i = 0; i < std::extent_v<decltype(CMonstorUnit::monster)>; i++) {
            const MONSTOR *monster = &unit->monster[i];
            if (monster->state == -1 || monster->hp <= 0 || monster->revealed == 0 || !(crystal || monster->state == 2)) continue;
            unit->chara[i][0].GetPosition(pos);
            if (!CellShown(map, CellOf(pos), show_all || crystal)) continue;
            AddMarker(pos, 0xFF, 0x30, 0x30);
        }
    }
}

// Renders the floor from above into the "aerial_map" target; false when it cannot.
bool RenderAerial(CDungeonMap *map, const float *player, Aerial &aerial, int show_all, int crystal, bool render) {
    if (!gfx::InFrame() && !gfx::Recording()) {
        return false;
    }
    MGPortOrthographicScope overhead(kAerialHeight);

    // Two images: "aerial_floor" holds the floor and everything that stands still (chests, Atla,
    // sleeping monsters) and is redrawn only when that changes; "aerial_map" is a copy of it with the
    // awake monsters and the player drawn on top each frame.
    gfx::TextureHandle floor_target = gfx::NamedRenderTarget("aerial_floor", 640, 480, true);
    gfx::TextureHandle target = gfx::NamedRenderTarget("aerial_map", 640, 480, true);

    if (target == gfx::kNullTexture || floor_target == gfx::kNullTexture) {
        return false;
    }

    const Config &config = ConfigGet();
    aerial.zoom = config.aerial_map_zoom * kMapSize / 320.0f;

    // Centre on the floor's built cells. When they do not fit (large generated floors, or zoom
    // past 1) the map follows the player, clamped so it never scrolls past the floor's edge.
    const float map_scale = kRetailScale * aerial.zoom;
    int lo_x = dungeon_grid::side, lo_z = dungeon_grid::side, hi_x = -1, hi_z = -1;
    for (int cell = 0; cell < dungeon_grid::cells; cell++) {
        if (map->cells[cell].parts_no == MAP_PARTS_NONE) continue;
        lo_x = std::min(lo_x, cell % dungeon_grid::side); hi_x = std::max(hi_x, cell % dungeon_grid::side);
        lo_z = std::min(lo_z, cell / dungeon_grid::side); hi_z = std::max(hi_z, cell / dungeon_grid::side);
    }
    float centre[2] = {kFloorCentre, kFloorCentre};
    float render_centre[2] = {kFloorCentre, kFloorCentre};
    float render_span = 20.0f * kCell;
    if (hi_x >= lo_x) {
        const float visible = kMapSize / map_scale;
        const float min_x = kCell * lo_x - kCell / 2.0f, max_x = kCell * hi_x + kCell / 2.0f;
        const float min_z = kCell * lo_z - kCell / 2.0f, max_z = kCell * hi_z + kCell / 2.0f;
        centre[0] = (min_x + max_x) / 2.0f;
        centre[1] = (min_z + max_z) / 2.0f;
        render_centre[0] = centre[0];
        render_centre[1] = centre[1];
        render_span = std::max(render_span, std::max(max_x - min_x, max_z - min_z));
        if (max_x - min_x > visible) centre[0] = std::clamp(player[0], min_x + visible / 2.0f, max_x - visible / 2.0f);
        if (max_z - min_z > visible) centre[1] = std::clamp(player[2], min_z + visible / 2.0f, max_z - visible / 2.0f);
    }

    aerial.map_x = kMapLeft + kMapSize / 2.0f - map_scale * centre[0];
    aerial.map_y = kMapTop + kMapSize / 2.0f - map_scale * centre[1];

    const RenderInfo         saved_info = mgRenderInfo;
    const gfx::TextureHandle previous = gfx::CurrentRenderTarget();

    // Texels per world unit: the whole floor fills kTargetFill rows, so the image never depends on
    // where the map is scrolled and can be reused until new cells are revealed.
    const float target_scale = kTargetFill / render_span;
    const float near_dist = kAerialHeight - config.aerial_map_ceiling;
    MGSetRenderInfo(target_scale * kAerialHeight, near_dist > 1.0f ? near_dist : 1.0f, kAerialHeight + kAerialDepth);
    mgRenderInfo.fog_a = mgRenderInfo.fog_near;
    mgRenderInfo.fog_b = 0.0f;
    mgRenderInfo.fog_far = mgRenderInfo.fog_near;

    // Straight down: eye x along world x, eye y (screen down) along world z, depth down the height.
    // The game's own view sets the handedness, so faces cull as they do in the main view.
    const float   side = Det3(saved_info.view) < 0.0f ? -1.0f : 1.0f;
    const float   eye[4] = {render_centre[0], AerialEyeHeight(player[1]) + kAerialHeight, render_centre[1], 1.0f};
    sceVu0FMATRIX base;
    std::memset(base, 0, sizeof base);
    base[0][0] = side;
    base[3][0] = -side * eye[0];
    base[2][1] = 1.0f;
    base[3][1] = -eye[2];
    base[1][2] = -1.0f;
    base[3][2] = eye[1];
    base[3][3] = 1.0f;
    SetView(base, eye, nullptr, 1.0f);

    // Where the floor at the feet lands in the target: MGRotTransPers2D's logical pixels, centre
    // (320, 240), target_scale texels per world unit at the camera's height above it.
    aerial.du = side * target_scale;
    aerial.dv = target_scale;
    aerial.u0 = 320.0f - aerial.du * render_centre[0];
    aerial.v0 = 240.0f - aerial.dv * render_centre[1];
    const uint8_t black[4] = {0, 0, 0, 0x80};
    if (render) {
        gfx::SetRenderTarget(floor_target);
        gfx::Clear(true, black, true, 0.0f);

        // The cells, as CDungeonMap::DrawMap places them; each part is shared by every cell using it.
        struct PartPlace {
            float pos[4];
            int   direction;
        };
        PartPlace saved_parts[72];

        for (int i = 0; i < 72; i++) {
            sceVu0CopyVector(saved_parts[i].pos, map->parts[i].pos);
            saved_parts[i].direction = map->parts[i].direction;
        }

        TexManager.ReloadTexture(Vif1Packet, 3);

        for (int cell = 0; cell < dungeon_grid::cells; cell++) {
            if (!CellShown(map, cell, show_all)) {
                continue;
            }

            CDungeonParts *part = &map->parts[map->cells[cell].parts_no];
            part->direction = map->cells[cell].direction;
            part->pos[0] = kCell * (float) (cell % dungeon_grid::side);
            part->pos[1] = 0.0f;
            part->pos[2] = kCell * (float) (cell / dungeon_grid::side);
            part->pos[3] = 1.0f;
            part->Draw();
        }

        for (int i = 0; i < 72; i++) {
            sceVu0CopyVector(map->parts[i].pos, saved_parts[i].pos);
            map->parts[i].direction = saved_parts[i].direction;
        }

        DrawModels(map, base, eye, player, config.aerial_map_model_scale, crystal, show_all, 0);
    }

    // The live layer: the still image, then the few things that move.
    gfx::CopyTexture(floor_target, {0, 0, 640, 480}, target, 0, 0);
    gfx::SetRenderTarget(target);
    gfx::Clear(false, black, true, 0.0f);
    SetView(base, eye, nullptr, 1.0f);
    DrawModels(map, base, eye, player, config.aerial_map_model_scale, crystal, show_all, 1);

    gfx::SetRenderTarget(previous);
    mgRenderInfo = saved_info;
    TexManager.ReloadTexture(Vif1Packet, 0x1F);
    return true;
}

// Copies each shown cell of the aerial image onto the map at retail's place for it.
void CompositeAerial(CDungeonMap *map, const Aerial &aerial, int show_all, u_char bright, u_char dim, u_char alpha) {
    gfx::TextureBinding binding;
    binding.texture = gfx::FindNamedRenderTarget("aerial_map");
    binding.filter = gfx::Filter::Linear;
    binding.ignore_alpha = true;

    gfx::DrawState state = draw2d::SpriteState();
    state.scissor = true;
    state.scissor_rect = {kMapLeft, kMapTop, kMapSize, kMapSize};

    const float                tile = kCell * kRetailScale * aerial.zoom;
    std::vector<gfx::Vertex2D> quads;

    for (int cell = 0; cell < dungeon_grid::cells; cell++) {
        if (!CellShown(map, cell, show_all)) {
            continue;
        }

        const float x = kCell * (float) (cell % dungeon_grid::side) - kCell / 2.0f;
        const float z = kCell * (float) (cell / dungeon_grid::side) - kCell / 2.0f;
        const float dx = aerial.map_x + kRetailScale * aerial.zoom * x;
        const float dy = aerial.map_y + kRetailScale * aerial.zoom * z;

        if (dx > kMapLeft + kMapSize || dy > kMapTop + kMapSize || dx + tile < kMapLeft || dy + tile < kMapTop) {
            continue;
        }

        const u_char shade = map->mask[cell] != 0 ? bright : dim;
        const float  u = aerial.u0 + aerial.du * x;
        const float  v = aerial.v0 + aerial.dv * z;
        const float  uw = aerial.du * kCell;
        const float  vh = aerial.dv * kCell;

        const gfx::Vertex2D corners[4] = {
            draw2d::Vertex(dx, dy, 0.0f, u, v, shade, shade, shade, alpha),
            draw2d::Vertex(dx + tile, dy, 0.0f, u + uw, v, shade, shade, shade, alpha),
            draw2d::Vertex(dx + tile, dy + tile, 0.0f, u + uw, v + vh, shade, shade, shade, alpha),
            draw2d::Vertex(dx, dy + tile, 0.0f, u, v + vh, shade, shade, shade, alpha),
        };
        quads.insert(quads.end(), corners, corners + 4);
    }

    if (!quads.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, quads, binding, state);
    }
}

// A small pulsing dot for each marker, with a dark edge so it reads on any floor. Things within two
// cells of the player are left out: they are in view already. The player shows as their model (DrawModels).
void DrawMarkers(const Aerial &aerial, const float *player) {
    gfx::DrawState state = draw2d::SpriteState();
    state.scissor = true;
    state.scissor_rect = {kMapLeft, kMapTop, kMapSize, kMapSize};

    const float scale = kRetailScale * aerial.zoom;
    const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(SDL_GetTicks()) * 0.006f);
    std::vector<gfx::Vertex2D> quads;
    auto dot = [&](float cx, float cy, float half, u_char r, u_char g, u_char b, u_char a) {
        quads.push_back(draw2d::Vertex(cx - half, cy - half, 0.0f, 0, 0, r, g, b, a));
        quads.push_back(draw2d::Vertex(cx + half, cy - half, 0.0f, 0, 0, r, g, b, a));
        quads.push_back(draw2d::Vertex(cx + half, cy + half, 0.0f, 0, 0, r, g, b, a));
        quads.push_back(draw2d::Vertex(cx - half, cy + half, 0.0f, 0, 0, r, g, b, a));
    };
    const float near = 2.0f * kCell;
    for (const Marker &marker : g_markers) {
        const float dx = marker.x - player[0], dz = marker.z - player[2];
        if (dx * dx + dz * dz < near * near) continue;
        const float cx = aerial.map_x + scale * marker.x;
        const float cy = aerial.map_y + scale * marker.z;
        const u_char a = static_cast<u_char>(0x50 + 0x30 * wave);
        dot(cx, cy, 3.0f, 0, 0, 0, a);
        dot(cx, cy, 2.0f, marker.r, marker.g, marker.b, a);
    }

    if (!quads.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, quads, {}, state);
    }
}

// Retail's trap circle symbols, which stay icons on the aerial map.
void DrawTrapSymbols(CDungeonMap *map, const Aerial &aerial, u_char alpha) {
    CTexture *symbol_texture = TexManager.GetTexture(const_cast<char *>("itempack"), -1);

    for (int i = 0; i < 3; i++) {
        if (map->trap_circle[i].state == 0) {
            continue;
        }

        int cell = CellOf(map->trap_circle[i].pos);

        if (cell < 0 || map->cells[cell].parts_no == MAP_PARTS_NONE ||
            !(BtEquipMasuisyou == 1 || map->mask[cell] != 0)) {
            continue;
        }

        const float scale = kRetailScale * aerial.zoom;
        int         x = (int) (aerial.map_x + scale * map->trap_circle[i].pos[0]) - 4;
        int         y = (int) (aerial.map_y + scale * map->trap_circle[i].pos[2]) - 4;
        set2DSprite(Vif1Packet, symbol_texture, CRect_i_(x, y, 8, 8), CRect_i_(88, 112, 8, 8), alpha);
    }
}

void DrawRetailMiniMap(CDungeonMap *map, float *pos, float angle, u_char red, u_char green, u_char blue);

// Live tuning while the map shows, on function keys the port leaves free: F5/F6 model size
// (x0.8/x1.25), F1/F2 map size (-/+0.1), F7 zoom +0.25 (Shift+F7 -0.25), Shift+F5/F6 ceiling cut
// -/+10. Each press is saved to config.json at once.
void TuneAerial() {
    struct Key {
        SDL_Scancode code;
        bool         shift;
        int          field;
        float        step;
    };
    static const Key keys[] = {
        {SDL_SCANCODE_F6, false, 0, 1.25f}, {SDL_SCANCODE_F5, false, 0, 0.8f},
        {SDL_SCANCODE_F2, false, 1, 0.1f},  {SDL_SCANCODE_F1, false, 1, -0.1f},
        {SDL_SCANCODE_F7, false, 2, 0.25f}, {SDL_SCANCODE_F7, true, 2, -0.25f},
        {SDL_SCANCODE_F6, true, 3, 10.0f},  {SDL_SCANCODE_F5, true, 3, -10.0f},
    };
    static bool held[sizeof keys / sizeof keys[0]] = {};
    int         count = 0;
    const bool *state = SDL_GetKeyboardState(&count);

    if (state == nullptr) {
        return;
    }

    const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    Config     config = ConfigGet();
    bool       changed = false;

    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        const bool down = keys[i].shift == shift && keys[i].code < count && state[keys[i].code];

        if (down && !held[i]) {
            const float step = keys[i].step;
            switch (keys[i].field) {
                case 0: config.aerial_map_model_scale = std::clamp(config.aerial_map_model_scale * step, 0.5f, 40.0f); break;
                case 1: config.aerial_map_size = std::clamp(config.aerial_map_size + step, 0.5f, 1.5f); break;
                case 2: config.aerial_map_zoom = std::clamp(config.aerial_map_zoom + step, 0.5f, 8.0f); break;
                default: config.aerial_map_ceiling = std::clamp(config.aerial_map_ceiling + step, 0.0f, 5000.0f); break;
            }
            changed = true;
        }

        held[i] = down;
    }

    if (changed) {
        std::fprintf(stderr, "aerial map: model_scale %g size %g zoom %g ceiling %g\n", config.aerial_map_model_scale,
                     config.aerial_map_size, config.aerial_map_zoom, config.aerial_map_ceiling);
        ConfigChange(config);
    }
}

} // namespace

PORT_OVERRIDE void CDungeonMap::DrawMiniMap(float *pos, float angle) {
    CDungeonMap *self = this;
    CDungeonMap *map_self = self;
    int status = UserStatus->minimap_status;
    g_drawn = false;

    if (status == 3) {
        return;
    }

    // The Dungeon Map option picks the map's size (large, medium, small) at one fixed brightness.
    static const float sizes[3] = {1.5f, 1.15f, 0.85f};
    unsigned char red = 0x58;    // unrevealed cells shown by the map item
    unsigned char green = 0x80;  // revealed cells
    unsigned char blue = 0x80;   // opaque
    int           saved_map = BtEquipMap;
    int           saved_crystal = BtEquipMasuisyou;

    if (DebugStatus[3] != 0) {
        BtEquipMap = 1;
        BtEquipMasuisyou = 1;
    }

    if (ConfigGet().aerial_map) {
        Aerial aerial;
        int    show_all = BtEquipMap != 0;
        int    crystal = BtEquipMasuisyou == 1;
        float  player[4];
        sceVu0CopyVector(player, pos);

        TuneAerial();
        PlaceMap(sizes[status] * ConfigGet().aerial_map_size / 1.4f);

        // The floor image is drawn only when it changes: a new floor, the map item or crystal, or
        // newly revealed cells. Between those the map reuses it and just moves the view and dots.
        static std::uint64_t last_key = 0;
        static bool cached = false;
        const int  flags = show_all | (crystal << 1);
        std::uint64_t key = DungeonAerialFloorKey(*map_self, flags, ConfigGet().aerial_map_ceiling,
                                                 ConfigGet().aerial_map_model_scale, AerialEyeHeight(player[1]));
        const auto mix = [&](std::uint64_t value) { key = (key ^ value) * 1099511628211ull; };
        mix(selectMapNo); mix(UserStatus->cur_georama);
        mix(gfx::FindNamedRenderTarget("aerial_floor"));
        mix(gfx::FindNamedRenderTarget("aerial_map"));
        // Sleeping monsters are fixed. Waking/sleeping invalidates the cache;
        // animation offsets must not make it redraw the floor every tick.
        if (NowMonstorUnit != nullptr) {
            for (int i = 0; i < std::extent_v<decltype(CMonstorUnit::monster)>; i++) {
                const MONSTOR &m = NowMonstorUnit->monster[i];
                if (m.state != -1 && m.state != 2 && m.hp > 0) {
                    mix(i + 1); mix(m.revealed);
                }
            }
        }
        const bool stale = !cached || key != last_key;
        bool       drawn = RenderAerial(map_self, player, aerial, show_all, crystal, stale);
        if (drawn && stale) {
            last_key = key;
            cached = true;
        }
        CollectMarkers(map_self, crystal, show_all);

        if (drawn) {
            CompositeAerial(map_self, aerial, show_all, green, red, blue);
            DrawTrapSymbols(map_self, aerial, blue);
            if (ConfigGet().aerial_map_markers) DrawMarkers(aerial, player);
            g_drawn = true;
            BtEquipMap = saved_map;
            BtEquipMasuisyou = saved_crystal;
            return;
        }
    }

    // Retain retail's density/opacity choices when aerial rendering is disabled or unavailable.
    constexpr u_char retail_red[] = {0x20, 0x40, 0x60};
    constexpr u_char retail_green[] = {0x60, 0x80, 0xA2};
    constexpr u_char retail_blue[] = {0x40, 0x60, 0xA2};
    DrawRetailMiniMap(map_self, pos, angle, retail_red[status], retail_green[status], retail_blue[status]);
    BtEquipMap = saved_map;
    BtEquipMasuisyou = saved_crystal;
}

namespace {

// Retail's CDungeonMap::DrawMiniMap after its density colours and the debug override.
void DrawRetailMiniMap(CDungeonMap *map, float *pos, float angle, u_char red, u_char green, u_char blue) {
    int        visible = BtEquipMap ? 0 : 1;
    CTexture  *map_texture = TexManager.GetTexture(const_cast<char *>("minimap1"), -1);
    CTexture  *symbol_texture = TexManager.GetTexture(const_cast<char *>("itempack"), -1);
    int        map_x;

    for (int row = 0; row < dungeon_grid::side; row++) {
        for (int col = 0; col < dungeon_grid::side; col++) {
            int           cell_no = col + row * dungeon_grid::side;
            int           masked = map->mask[cell_no];
            int           show = masked;
            unsigned char alpha;

            if (visible == 0 && masked == 0) {
                show = 1;
                alpha = red;
            } else {
                alpha = green;
            }

            if (map->cells[cell_no].parts_no != MAP_PARTS_NONE && show == 1) {
                int direction = map->cells[cell_no].direction + map->parts[map->cells[cell_no].parts_no].direction_offset;
                set2DSprite(Vif1Packet, map_texture, CRect_i_(col * 16 + 380, row * 16 + 64, 16, 16),
                            CRect_i_((direction % 8) * 16, (direction / 8) * 16, 16, 16), alpha, alpha, alpha, blue);
            }
        }
    }

    pos[0] *= 0.1f;
    pos[2] *= 0.1f;
    map_x = (int) (pos[2] + 64.0f + 8.0f);
    float player_screen_x = pos[0] + 380.0f + 8.0f;
    set2DSprite(Vif1Packet, symbol_texture, CRect_i_((int) player_screen_x, map_x, 12, 12), CRect_i_(88, 96, 12, 12), 7, 7,
                angle);

    struct Symbol {
        const float *pos;
        int          u;
        int          v;
    };
    std::vector<Symbol> symbols;

    for (int i = 0; i < 3; i++) {
        if (map->trap_circle[i].state != 0) {
            symbols.push_back({map->trap_circle[i].pos, 88, 112});
        }
    }

    for (int i = 0; i < 24; i++) {
        if (map->boxes[i].used != 0 && map->boxes[i].closed != 0) {
            symbols.push_back({map->boxes[i].pos, 80, 96});
        }
    }

    for (int i = 0; i < 8; i++) {
        if (map->atra[i].used != 0) {
            symbols.push_back({map->atra[i].pos, 72, 104});
        }
    }

    for (const Symbol &symbol : symbols) {
        int x = (int) (symbol.pos[0] * 0.1f);
        int y = (int) (symbol.pos[2] * 0.1f);
        int cell = (int) ((80.0f + symbol.pos[0]) / 160.0f) + (int) ((80.0f + symbol.pos[2]) / 160.0f) * dungeon_grid::side;
        int show = map->mask[cell];

        if (BtEquipMasuisyou == 1) {
            show = 1;
        }

        if (map->cells[cell].parts_no != MAP_PARTS_NONE && show == 1) {
            set2DSprite(Vif1Packet, symbol_texture, CRect_i_(x + 384, y + 68, 8, 8), CRect_i_(symbol.u, symbol.v, 8, 8), blue);
        }
    }
}

} // namespace
