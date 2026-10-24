#include "gameutil.hpp"

#include <cstdio>
#include <cstring>

#include "audio/mixer.hpp"
#include "audio/trace.hpp"

// The EZMIDI RPC as the port's mixer answers it. CSound (port/src/sound.cpp) talks to the mixer
// directly and never comes here; this keeps the command table answerable for anything else.
//
// Command words: the low nibble is the MIDI port.
//   0x00+p play            0x20+p stop            0x30+p rewind to song n
//   0x40+p set sequence    0xA0+p port attribute  0xB0+p port volume (256 full)
//   0xC0   stereo (1) / monaural (0)
//   0x8010 allocate stream input buffer  0x8090+p is playing  0x80E0+p port volume
//   0x9050+p bind bank     0x9070 copy bank body to sound memory

PORT_OVERRIDE int ezMidiInit() {
    return 1;
}

PORT_OVERRIDE int ezMidi(int command, int argument) {
    audio::Mixer &mixer = audio::DefaultMixer();
    const int     port = command & 0x0F;
    audio::Trace("ezMidi 0x%X 0x%X", command, argument);
    switch (command & ~0x0F) {
        case 0x00:
            mixer.Play(port);
            return 0;
        case 0x20:
            mixer.Stop(port);
            return 0;
        case 0x30:
            mixer.Rewind(port, argument);
            return 0;
        case 0xA0:
            mixer.SetAttribute(port, argument);
            return 0;
        case 0xB0:
            mixer.SetVolume(port, argument);
            return 0;
        case 0xC0:
            mixer.SetStereo(argument != 0);
            return 0;
        case 0x8090:
            return mixer.IsPlaying(port) ? 1 : 0;
        case 0x80E0:
            return mixer.Volume(port);
        default:
            // The rest carry IOP addresses, which the port has no equivalent of.
            std::fprintf(stderr, "ezMidi: command 0x%X takes an IOP address; ignored\n", command);
            return 0;
    }
}

PORT_OVERRIDE int ezTransToIOP(void *iop_address, void *ee_address, int size) {
    std::memcpy(iop_address, ee_address, size);
    return 0;
}

#include <libvu0.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "framevu1.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "visualvu1.hpp"
#include "vu0_ops.hpp"

namespace {

// Retail's def_vrtx is file-static in gameutil.cpp, which only MotionProc2 touches.
std::unique_ptr<sceVu0FVECTOR[]> g_deformed;
int                              g_deformed_count = 0;

// base carries the inverted bind rotation with the uninverted bind translation in row 3, which is
// why the translation is subtracted before the transform and again after it.
void SkinVertex(float *out, float *deformed, const float *source, const float base[4][4], const float bone[4][4],
                const float inverse[4][4], float weight) {
    float local[4];
    for (int i = 0; i < 3; i++) {
        local[i] = source[i] - base[3][i];
    }
    local[3] = 1.0f;

    float accumulated[4];
    vu0::Apply(accumulated, base, local);
    for (int i = 0; i < 3; i++) {
        local[i] = accumulated[i] - base[3][i];
    }

    vu0::Apply(accumulated, bone, local);
    float moved[3];
    for (int i = 0; i < 3; i++) {
        moved[i] = accumulated[i] - source[i];
    }

    for (int i = 0; i < 3; i++) {
        deformed[i] = deformed[i] + moved[i] * weight;
    }

    float result[4];
    vu0::Apply(result, inverse, deformed);
    vu0::Copy(out, result);
}

} // namespace

Mot_List *MotionProc2(CFrame *frame, tagMOTION_TYPE *motion, tagFRAME_INF *frame_info, Mot_List *list) {
    static sceVu0FMATRIX  Bone_Matrix;
    static sceVu0FMATRIX  Bone_Matrix_inv;
    static sceVu0FMATRIX  Bone_Matrix_Base;
    static sceVu0FVECTOR *vert;
    CFrameVu1            *target;

    if (list->type == MOTION_KEY_SKIP) {
        return list->next;
    }

    target = &(static_cast<CFrameVu1 *>(frame))[list->target];

    if (list->target == static_cast<u32>(frame_info[list->frame].parent_frame)) {
        CFrameVu1  *owner = &(static_cast<CFrameVu1 *>(frame))[list->frame];
        MDT_HEADER *model = reinterpret_cast<MDT_HEADER *>(owner->GetVisual()->GetMDTDataAddress());

        vert = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<u_char *>(model) + model->vertex_ofs);

        const int vertex_count = frame_info[list->frame].vertex_count;
        if (vertex_count < 0) {
            return list->next;
        }
        if (vertex_count != g_deformed_count) {
            g_deformed = std::make_unique<sceVu0FVECTOR[]>(static_cast<size_t>(vertex_count));
            g_deformed_count = vertex_count;
        }

        if (vertex_count > 0 && frame_info[list->frame].base_vertices == nullptr) {
            return list->next;
        }
        if (vertex_count > 0) {
            std::memcpy(g_deformed.get(), frame_info[list->frame].base_vertices,
                        static_cast<size_t>(vertex_count) * sizeof(sceVu0FVECTOR));
        }
        sceVu0UnitMatrix(Bone_Matrix);
        sceVu0UnitMatrix(Bone_Matrix_Base);
        sceVu0InversMatrix(Bone_Matrix_inv, frame_info[list->frame].matrix);
        owner->attr.remake_pending = 1;
        sceVu0UnitMatrix(frame_info[list->target].bone_base_matrix);
        sceVu0UnitMatrix(frame_info[list->target].bone_matrix);
    } else {
        sceVu0FMATRIX local;
        sceVu0FVECTOR translation;

        sceVu0CopyMatrix(local, target->local);
        MulMatrix(Bone_Matrix, frame_info[frame_info[list->target].parent_frame].bone_matrix, local);
        MulMatrix(Bone_Matrix_Base, frame_info[frame_info[list->target].parent_frame].bone_base_matrix,
                  motion->base_matrices[list->target]);
        sceVu0CopyMatrix(frame_info[list->target].bone_base_matrix, Bone_Matrix_Base);
        sceVu0CopyMatrix(frame_info[list->target].bone_matrix, Bone_Matrix);
        sceVu0CopyVector(translation, Bone_Matrix_Base[3]);
        sceVu0InversMatrix(Bone_Matrix_Base, Bone_Matrix_Base);
        sceVu0CopyVector(Bone_Matrix_Base[3], translation);
    }

    for (u_int i = 0; i < list->key_count; i++) {
        u32 vertex = list->keys[i].frame;
        if (vertex >= static_cast<u32>(g_deformed_count) || vert == nullptr) {
            continue;
        }
        SkinVertex(vert[vertex], g_deformed[vertex], frame_info[list->frame].base_vertices[vertex], Bone_Matrix_Base,
                   Bone_Matrix, Bone_Matrix_inv, 0.01f * list->keys[i].value[0]);
    }

    return list->next;
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "draw2d_port.hpp"
#include "gameutil.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "texture_port.hpp"

namespace {

// Retail's sprite batches filter with a file-static flag nothing ever sets.
constexpr u_long kBatchTex1 = 0x41;

bool                       g_batch_open = false;
u_long                     g_batch_tex0 = 0;
std::vector<gfx::Vertex2D> g_batch;

} // namespace

// The batch draws when it closes, which is where retail's packet reaches the GS: nothing else may
// write to the packet between Start and End.
PORT_OVERRIDE void set2DSprite_Start(sceVif1Packet *packet, CTexture *texture) {
    g_batch.clear();
    g_batch_open = texture != nullptr;
    if (texture != nullptr) {
        g_batch_tex0 = texture->tex0;
    }
}

PORT_OVERRIDE void set2DSprite_Core(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                                    u8 red, u8 green, u8 blue, u8 alpha) {
    if (texture == nullptr || !g_batch_open) {
        return;
    }

    // DC_SPRITE_TRACE=1: each distinct (texture, texel rect) a sprite draws, once, for finding the art
    // a draw site uses (button symbols baked into textures).
    static const bool                                            trace = std::getenv("DC_SPRITE_TRACE") != nullptr;
    static std::set<std::tuple<std::string, int, int, int, int>> seen;
    if (trace && seen.emplace(texture->name, texel.x, texel.y, texel.width, texel.height).second) {
        std::fprintf(stderr, "sprite %s texel %d %d %d %d screen %d %d %d %d\n", texture->name, texel.x, texel.y,
                     texel.width, texel.height, screen.x, screen.y, screen.width, screen.height);
    }

    const float x0 = static_cast<float>(screen.x);
    const float y0 = static_cast<float>(screen.y);
    const float x1 = static_cast<float>(screen.x + screen.width);
    const float y1 = static_cast<float>(screen.y + screen.height);
    const float u0 = static_cast<float>(texel.x);
    const float v0 = static_cast<float>(texel.y);
    const float u1 = static_cast<float>(texel.x + texel.width);
    const float v1 = static_cast<float>(texel.y + texel.height);

    g_batch.push_back(draw2d::Vertex(x0, y0, 0.0f, u0, v0, red, green, blue, alpha));
    g_batch.push_back(draw2d::Vertex(x1, y0, 0.0f, u1, v0, red, green, blue, alpha));
    g_batch.push_back(draw2d::Vertex(x1, y1, 0.0f, u1, v1, red, green, blue, alpha));
    g_batch.push_back(draw2d::Vertex(x0, y1, 0.0f, u0, v1, red, green, blue, alpha));
}

PORT_OVERRIDE void set2DSprite_End(sceVif1Packet *packet, CTexture *texture) {
    if (!g_batch_open) {
        return;
    }

    g_batch_open = false;
    draw2d::DrawTextured(gfx::Primitive::Quads, g_batch, g_batch_tex0, kBatchTex1, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    g_batch.clear();
}

PORT_OVERRIDE void SetClut(sceVif1Packet *packet, CTexture *texture, i *clut) {
    if (texture == nullptr || clut == nullptr) {
        return;
    }

    const sceGsTex0 tex0 = *reinterpret_cast<const sceGsTex0 *>(&texture->tex0);
    PortLoadClut(static_cast<unsigned>(tex0.CBP), reinterpret_cast<const u_int *>(clut));
}
