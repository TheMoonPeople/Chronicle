// Names the PS2 link supplies and the port's link otherwise lacks (ps2/config/pal/object_fixups.json
// and the linker script). MWCC pools string literals and the PS2 build binds a unit's extern name
// to another unit's pooled literal or to a differently typed copy of a function; clang has no such
// pool, so each name gets its own definition here. Weak, so a replacement unit that takes one of
// these over needs no change here. Pure aliases of existing storage (draw_rect, WorkBuffer__2)
// are linker aliases in port/CMakeLists.txt instead.

#include <algorithm>
#include <cstring>

#include "btitem.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "clothread.hpp"
#include "dranmapfield.hpp"
#include "dungeonmap.hpp"
#include "eastking.hpp"
#include "editpartsdata.hpp"
#include "gameutil.hpp"
#include "hitmark.hpp"
#include "main.hpp"
#include "memcard.hpp"
#include "menu_misc.hpp"
#include "mglib.hpp"
#include "snd.hpp"
#include "water.hpp"

#define PORT_LINK_NAME __attribute__((weak))

#ifdef _WIN32
// COFF has no --defsym; these unresolved names must share storage, never a copied table.
#pragma comment(linker, "/alternatename:draw_rect=draw_rect_store")
#pragma comment(linker, "/alternatename:WorkBuffer__2=WorkBuffer")
#endif

// The literals, as the PAL executable has them.
PORT_LINK_NAME char       BtAtraShortCharaFile[] = "dun/mainchara/c01d_ex00.chr";
PORT_LINK_NAME char       BtEffectInfoFile[] = "info.cfg";
PORT_LINK_NAME char       MdsExtension[] = ".mds";
PORT_LINK_NAME char       OverMessage[] = " ************* over!!\n";
PORT_LINK_NAME char       HealEffectTextureName[] = "basefx00";
PORT_LINK_NAME char       gamemode_empty_string[] = "";
PORT_LINK_NAME char       manual_frame_image[] = "#frame_image#640#480#4";
PORT_LINK_NAME char       allmenu_mes[] = "allmenu.mes";
PORT_LINK_NAME const char CharaFileExtension[5] = ".chr";
PORT_LINK_NAME const char FrameImageTexture[] = "#frame_image#640#480#4";

// The title overlay's units declare their own rectangle template rather than rect.hpp's CRect_i_
// (x, y, width, height in the same four words), and MWCC mangled both to the one name.
template <class T>
class CRect {
public:
    T x;
    T y;
    T w;
    T h;
};

namespace {

CRect_i_ Rect(const CRect<int> &rect) {
    return CRect_i_(rect.x, rect.y, rect.w, rect.h);
}

} // namespace

PORT_LINK_NAME void MGFillBox(const CRect<int> &rect, unsigned char r, unsigned char g, unsigned char b,
                              unsigned char a) {
    MGFillBox(Rect(rect), r, g, b, a);
}

PORT_LINK_NAME void MGMoveImage(sceGsTex0 *src, const CRect<int> &rect, sceGsTex0 *dst, int dst_x, int dst_y,
                                int direction) {
    MGMoveImage(src, Rect(rect), dst, dst_x, dst_y, direction);
}

PORT_LINK_NAME void MGStretchMoveImage(sceGsTex0 *src, const CRect<int> &src_rect, sceGsTex0 *dst,
                                       const CRect<int> &dst_rect) {
    MGStretchMoveImage(src, Rect(src_rect), dst, Rect(dst_rect));
}

PORT_LINK_NAME void MoveImageTest(sceVif1Packet *packet, int src_base, int src_width, int src_format,
                                  const CRect<int> &rect, int dst_base, int dst_width, int dst_format, int dst_x,
                                  int dst_y, int direction) {
    MoveImageTest(packet, src_base, src_width, src_format, Rect(rect), dst_base, dst_width, dst_format, dst_x, dst_y,
                  direction);
}

PORT_LINK_NAME void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen,
                                const CRect<int> &texel, unsigned char alpha) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), alpha);
}

PORT_LINK_NAME void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen,
                                const CRect<int> &texel, unsigned char red, unsigned char green, unsigned char blue,
                                unsigned char alpha) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), red, green, blue, alpha);
}

PORT_LINK_NAME void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen,
                                const CRect<int> &texel, int pivot_x, int pivot_y, float angle) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), pivot_x, pivot_y, angle);
}

// op_c.cpp types CWater itself and calls DrawVu1 with a u_long128 * last; the member's mangled
// name is defined as a function taking the object first, which is how the member is called.
PORT_LINK_NAME void OpeningWaterDrawVu1(CWater *water, RenderInfo *info, sceVif1Packet *packet,
                                        u_long128 *parent) asm(PORT_ASM_NAME(_ZN6CWater7DrawVu1EP10RenderInfoP13sceVif1PacketPo));

void OpeningWaterDrawVu1(CWater *water, RenderInfo *info, sceVif1Packet *packet, u_long128 *parent) {
    DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(water, info, packet, parent);
}

// main.cpp defines this as GeneratedNpcModel's implicit assignment, which the PS2 link renames: every
// member but the tail padding, the character through its own assignment.
PORT_LINK_NAME MAP_NPC_MODEL &MAP_NPC_MODEL::operator=(const MAP_NPC_MODEL &other) {
    chara = other.chara;
    for (int i = 0; i < 4; i++) {
        pos[i] = other.pos[i];
        rot[i] = other.rot[i];
    }
    parts_no = other.parts_no;
    used = other.used;
    visible = other.visible;
    motion_no = other.motion_no;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 4; j++) {
            draw_pos[i][j] = other.draw_pos[i][j];
        }
        draw_param[i] = other.draw_param[i];
    }
    draw_num = other.draw_num;
    return *this;
}

// The linker script names EditGaijiTbl inside EditPartsData, and clsmes.cpp indexes it with codes -0x300
// and up, which on the PS2 lands on the last word of each GaijiDataTbl entry: EditGaijiTbl is
// GaijiDataTbl + 0x601C. ld64's -alias takes no offset, so instead of a linker alias on one platform and
// not the other, the port gives the table its own storage, copied from GaijiDataTbl before main (which
// nothing writes), and names its end with an assembler alias. Mach-O marks the alias an alt entry, so it
// stays in the storage's atom.
constexpr int kEditGaijiCodes = 0x300;

alignas(16) EDIT_GAIJI PortEditGaijiStorage[kEditGaijiCodes + 1];

static_assert(kEditGaijiCodes * sizeof(EDIT_GAIJI) == 0x6000);
#define PORT_EDIT_GAIJI_TBL PORT_ASM_NAME(EditGaijiTbl)
asm(".globl " PORT_EDIT_GAIJI_TBL "\n"
    ".set " PORT_EDIT_GAIJI_TBL ", " PORT_ASM_NAME(PortEditGaijiStorage) " + 0x6000\n");

namespace {

[[gnu::constructor]] void CopyEditGaijiTable() {
    const unsigned char  *table = reinterpret_cast<const unsigned char *>(GaijiDataTbl);
    constexpr std::size_t kFirstWord = 0x1C;
    for (std::size_t i = 0; i < kEditGaijiCodes; i++) {
        std::size_t start = kFirstWord + i * sizeof(EDIT_GAIJI);
        if (start >= sizeof(GaijiDataTbl)) {
            break;
        }
        std::size_t bytes = std::min(sizeof(EDIT_GAIJI), sizeof(GaijiDataTbl) - start);
        std::memcpy(&PortEditGaijiStorage[i], table + start, bytes);
    }
}

} // namespace
