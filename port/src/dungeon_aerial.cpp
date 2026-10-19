#include "dungeon_aerial.hpp"

#include <SDL3/SDL.h>
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
// Retail's map is 20 x 20 cells of 16 pixels at 380,64; game.aerial_map_size grows it from its
// top-right corner, moving up as far as the frame allows.
float kMapLeft = 380.0f;
float kMapTop = 64.0f;
float kMapSize = 320.0f;

void PlaceMap(float size) {
    kMapSize = 320.0f * size;
    kMapTop = std::max(8.0f, std::min(64.0f, 472.0f - kMapSize));
    kMapSize = std::min(kMapSize, 472.0f - kMapTop);
    kMapLeft = 700.0f - kMapSize;
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

    if (col < 0 || row < 0 || col >= 20 || row >= 20) {
        return -1;
    }

    return col + row * 20;
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
};
std::vector<Marker> g_markers;

void AddMarker(const float *pos, u_char r, u_char g, u_char b) {
    g_markers.push_back({pos[0], pos[2], r, g, b});
}

bool CellShown(CDungeonMap *map, int cell, int show_all) {
    return cell >= 0 && map->cells[cell].parts_no != MAP_PARTS_NONE && (show_all || map->mask[cell] != 0);
}

void DrawModels(CDungeonMap *map, const sceVu0FMATRIX base, const float *eye, const float *player, float scale, int crystal,
                int show_all) {
    float lid[4];

    // Chests: retail's symbol shows a closed box on a revealed cell, or all of them with the crystal.
    if (map->box_lid_model != NULL && map->box_body_model != NULL && map->chest_body_model != NULL &&
        map->chest_lid_model != NULL) {
        TexManager.ReloadTexture(Vif1Packet, 3);

        for (int i = 0; i < 24; i++) {
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
    if (map->atra_model != NULL && atraGetStatus == 0) {
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

        for (int i = 0; i < 16; i++) {
            MONSTOR *monster = &unit->monster[i];

            if (monster->state == -1 || monster->revealed == 0 || !(crystal || monster->state == 2)) {
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
    if (BtActStatus.player_visible != 0 && EdEventInfo.player_draw != 0) {
        SetView(base, eye, player, scale);
        Draw_MainUnit();
        AddMarker(player, 0x30, 0xFF, 0x30);
    }
}

// Renders the floor from above into the "aerial_map" target; false when it cannot.
bool RenderAerial(CDungeonMap *map, const float *player, Aerial &aerial, int show_all, int crystal) {
    if (!gfx::InFrame() && !gfx::Recording()) {
        return false;
    }

    gfx::TextureHandle target = gfx::NamedRenderTarget("aerial_map", 640, 480, true);

    if (target == gfx::kNullTexture) {
        return false;
    }

    const Config &config = ConfigGet();
    aerial.zoom = config.aerial_map_zoom * kMapSize / 320.0f;

    // Up to zoom 1 the whole floor shows, centred on the map as retail's; past it the map follows
    // the player.
    float centre[2] = {kFloorCentre, kFloorCentre};

    if (config.aerial_map_zoom > 1.0f) {
        centre[0] = player[0];
        centre[1] = player[2];
    }

    const float map_scale = kRetailScale * aerial.zoom;
    aerial.map_x = kMapLeft + kMapSize / 2.0f - map_scale * centre[0];
    aerial.map_y = kMapTop + kMapSize / 2.0f - map_scale * centre[1];

    const RenderInfo         saved_info = mgRenderInfo;
    const gfx::TextureHandle previous = gfx::CurrentRenderTarget();

    // Texels per world unit at the feet: the map's span fills kTargetFill rows.
    const float target_scale = kTargetFill / (kMapSize / map_scale);
    const float near_dist = kAerialHeight - config.aerial_map_ceiling;
    MGSetRenderInfo(target_scale * kAerialHeight, near_dist > 1.0f ? near_dist : 1.0f, kAerialHeight + kAerialDepth);
    mgRenderInfo.fog_a = mgRenderInfo.fog_near;
    mgRenderInfo.fog_b = 0.0f;
    mgRenderInfo.fog_far = mgRenderInfo.fog_near;

    // Straight down: eye x along world x, eye y (screen down) along world z, depth down the height.
    // The game's own view sets the handedness, so faces cull as they do in the main view.
    const float   side = Det3(saved_info.view) < 0.0f ? -1.0f : 1.0f;
    const float   eye[4] = {centre[0], player[1] + kAerialHeight, centre[1], 1.0f};
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
    aerial.u0 = 320.0f - aerial.du * centre[0];
    aerial.v0 = 240.0f - aerial.dv * centre[1];

    gfx::SetRenderTarget(target);
    const uint8_t black[4] = {0, 0, 0, 0x80};
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

    for (int cell = 0; cell < 400; cell++) {
        if (!CellShown(map, cell, show_all)) {
            continue;
        }

        CDungeonParts *part = &map->parts[map->cells[cell].parts_no];
        part->direction = map->cells[cell].direction;
        part->pos[0] = kCell * (float) (cell % 20);
        part->pos[1] = 0.0f;
        part->pos[2] = kCell * (float) (cell / 20);
        part->pos[3] = 1.0f;
        part->Draw();
    }

    for (int i = 0; i < 72; i++) {
        sceVu0CopyVector(map->parts[i].pos, saved_parts[i].pos);
        map->parts[i].direction = saved_parts[i].direction;
    }

    DrawModels(map, base, eye, player, config.aerial_map_model_scale, crystal, show_all);

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

    for (int cell = 0; cell < 400; cell++) {
        if (!CellShown(map, cell, show_all)) {
            continue;
        }

        const float x = kCell * (float) (cell % 20) - kCell / 2.0f;
        const float z = kCell * (float) (cell / 20) - kCell / 2.0f;
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

// A ring in each marker's colour around where it stands on the map.
void DrawMarkers(const Aerial &aerial) {
    gfx::DrawState state = draw2d::SpriteState();
    state.scissor = true;
    state.scissor_rect = {kMapLeft, kMapTop, kMapSize, kMapSize};

    const float scale = kRetailScale * aerial.zoom;
    // Big enough to frame a scaled model, never smaller than retail's 8 pixel symbol.
    const float radius = std::max(6.0f, 0.12f * kCell * scale * ConfigGet().aerial_map_model_scale / 2.5f);
    const float width = std::max(1.5f, radius * 0.25f);
    constexpr int kSegments = 20;
    std::vector<gfx::Vertex2D> quads;

    for (const Marker &marker : g_markers) {
        const float cx = aerial.map_x + scale * marker.x;
        const float cy = aerial.map_y + scale * marker.z;

        for (int pass = 0; pass < 2; pass++) {
            // A dark outline under the coloured ring keeps it readable on any floor.
            const float  inner = pass == 0 ? radius - width - 1.0f : radius - width;
            const float  outer = pass == 0 ? radius + 1.0f : radius;
            const u_char r = pass == 0 ? 0 : marker.r;
            const u_char g = pass == 0 ? 0 : marker.g;
            const u_char b = pass == 0 ? 0 : marker.b;

            for (int i = 0; i < kSegments; i++) {
                const float a0 = 6.2831853f * i / kSegments;
                const float a1 = 6.2831853f * (i + 1) / kSegments;
                quads.push_back(draw2d::Vertex(cx + inner * std::cos(a0), cy + inner * std::sin(a0), 0.0f, 0, 0, r, g, b, 0x80));
                quads.push_back(draw2d::Vertex(cx + outer * std::cos(a0), cy + outer * std::sin(a0), 0.0f, 0, 0, r, g, b, 0x80));
                quads.push_back(draw2d::Vertex(cx + outer * std::cos(a1), cy + outer * std::sin(a1), 0.0f, 0, 0, r, g, b, 0x80));
                quads.push_back(draw2d::Vertex(cx + inner * std::cos(a1), cy + inner * std::sin(a1), 0.0f, 0, 0, r, g, b, 0x80));
            }
        }
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
    int status = UserStatus->minimap_status;
    g_drawn = false;

    if (status == 3) {
        return;
    }

    unsigned char red_table[3] = {0x20, 0x40, 0x60};
    unsigned char green_table[3] = {0x60, 0x80, 0xA2};
    unsigned char blue_table[3] = {0x40, 0x60, 0xA2};
    unsigned char red = red_table[status];
    unsigned char green = green_table[status];
    unsigned char blue = blue_table[status];
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
        g_markers.clear();
        PlaceMap(ConfigGet().aerial_map_size);

        if (RenderAerial(this, player, aerial, show_all, crystal)) {
            CompositeAerial(this, aerial, show_all, green, red, blue);
            DrawTrapSymbols(this, aerial, blue);

            if (ConfigGet().aerial_map_markers) {
                DrawMarkers(aerial);
            }
            g_drawn = true;
            BtEquipMap = saved_map;
            BtEquipMasuisyou = saved_crystal;
            return;
        }
    }

    DrawRetailMiniMap(this, pos, angle, red, green, blue);
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

    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 20; col++) {
            int           cell_no = col + row * 20;
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
        int cell = (int) ((80.0f + symbol.pos[0]) / 160.0f) + (int) ((80.0f + symbol.pos[2]) / 160.0f) * 20;
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
