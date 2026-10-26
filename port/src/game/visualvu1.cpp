#include "stubs/visualvu1.hpp"

#include <cstring>

#include "dataalloc.hpp"
#include "dataset.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "tim2.hpp"
#include "visual.hpp"

void SetTextureInfo(CTexture *tex, char *name, TM2_head *head) {
    TM2_picture *picture = (TM2_picture *) ((u_char *) head + 16);
    int          width = head->image_width;
    int          height = head->image_height;
    int          bpp;
    int          psm = SCE_GS_PSMCT32;
    u_char      *clut;
    u_char      *image;
    int          tw;
    int          th;
    int          pow2;
    int          k;
    int          tbw;

    switch (head->image_type) {
        case TIM2_RGB16:
            psm = SCE_GS_PSMCT16;
            bpp = 2;
            break;
        case TIM2_RGB24:
            psm = SCE_GS_PSMCT24;
            bpp = 3;
            break;
        case TIM2_RGB32:
            bpp = 4;
            break;
        case TIM2_IDTEX4:
            psm = SCE_GS_PSMT4;
            bpp = 0;
            break;
        case TIM2_IDTEX8:
            psm = SCE_GS_PSMT8;
            bpp = 1;
            break;
    }

    clut = 0;
    image = (u_char *) picture + picture->header_size;

    if (bpp < 2) {
        clut = image + picture->image_size;
    }

    u_char *mip[4] = {0, 0, 0, 0};

    if (picture->mipmap_count > 1) {
        u_char *next = image + picture->mipmap_size[0];

        for (int m = 1; m < picture->mipmap_count; m++) {
            mip[m] = next;
            next += picture->mipmap_size[m];
        }
    }

    strcpy(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = bpp;
    tex->image[0] = (u_int *) image;

    for (int m = 1; m < 4; m++) {
        tex->image[m] = (u_int *) mip[m];
    }

    tex->clut = (u_int *) clut;

    tw = 0;
    th = 0;
    pow2 = width;

    while (pow2 >= 2) {
        pow2 >>= 1;
        tw++;
    }

    for (pow2 = 1, k = 0; k < tw; k++) {
        pow2 <<= 1;
    }

    if (width != pow2) {
        tw++;
    }

    pow2 = height;

    while (pow2 >= 2) {
        pow2 >>= 1;
        th++;
    }

    for (pow2 = 1, k = 0; k < th; k++) {
        pow2 <<= 1;
    }

    if (height != pow2) {
        th++;
    }

    tbw = width >> 6;

    if (tbw <= 0) {
        tbw = 1;
    }

    if (tex->bpp > 1) {
        tex->tex0 = SCE_GS_SET_TEX0(0, tbw, psm, tw, th, 1, 0, 0, 0, 0, 0, 0);
    } else {
        tex->tex0 = SCE_GS_SET_TEX0(0, tbw, psm, tw, th, 1, 0, 0, 0, 0, 0, 1);
    }
}

void SetTextureInfo(CTexture *tex, char *name, u_char *buffer) {
    u_int      i;
    IMG_head  *head = (IMG_head *) buffer;
    IMG_entry *entry = (IMG_entry *) (head + 1);

    for (i = 0; i < head->pictures; i++, entry++) {
        if (name != 0 && name[0] != 0) {
            if (strcmp(entry->name, name) != 0) {
                continue;
            }
        }

        SetTextureInfo(tex, entry->name, (TM2_head *) ((u_char *) head + entry->offset));
    }
}

/**
 * Clears the visual's draw flags.
 *
 * @mangled Initialize__7CVisualFv
 * @address 0x134B50
 * @size 0x10
 */
void CVisual::Initialize() {
    unk_00 = 0;
    unk_04 = 0;
}

/**
 * Constructs a visual and clears it.
 *
 * @mangled __ct__7CVisualFv
 * @address 0x134B60
 * @size 0x44
 */
CVisual::CVisual() {
    Initialize();
}

int CVisualVu1::RemakeData(unsigned int *data) {
    return 0;
}
/**
 * Whether a render flag is clear.
 */
static inline bool IsOff(int flag) {
    return !flag;
}

/**
 * Draws the visual into a VIF packet.
 *
 * @mangled DrawVu1__10CVisualVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
 * @address 0x134BC0
 * @size 0xC4
 */

/**
 * Writes the shadow draw packet and its transform matrix.
 */
static int SetShadowData(u_int *packet, float (*matrix)[4]) {
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0x6C080029;
    packet[4] = 0x8001;
    packet[5] = 0x102E8000;
    packet[6] = 0xE;
    packet[7] = 0;
    packet[8] = 0x68;
    packet[9] = 0x80;
    packet[10] = 0x42;
    packet[11] = 0;
    packet[12] = 0x8001;
    packet[13] = 0x102E8000;
    packet[14] = 0xE;
    packet[15] = 0;
    packet[16] = 0x62;
    packet[17] = 0x80;
    packet[18] = 0x42;
    packet[19] = 0;
    ((u_long128 *) packet)[5] = ((u_long128 *) matrix)[0];
    ((u_long128 *) packet)[6] = ((u_long128 *) matrix)[1];
    ((u_long128 *) packet)[7] = ((u_long128 *) matrix)[2];
    ((u_long128 *) packet)[8] = ((u_long128 *) matrix)[3];
    return 0x24;
}

int SetMaterial(u_int *packet, MDT_MATERIAL *material) {
    if (material == NULL) {
        return 0x14;
    }

    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0x6C040025;
    u_int last_row[4] = {3, 0, 0, 0};
    ((u_long128 *) packet)[1] = *(u_long128 *) material->diffuse;
    ((u_long128 *) packet)[2] = *(u_long128 *) material->ambient;
    ((u_long128 *) packet)[3] = *(u_long128 *) material->specular;
    ((u_long128 *) packet)[4] = *(u_long128 *) last_row;
    return 0x14;
}

int SetTEX0(u_int *packet, u_long tex0, u_long tex1) {
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;

    if (tex1 == 0) {
        tex1 = *(u_long *) &mgTEX1Env;
    }

    if (tex1 != 0) {
        packet[3] = 0x50000003;
        packet[4] = 0x8002;
        packet[5] = 0x10000000;
        packet[6] = 0xE;
        packet[7] = 0;
        packet += 8;
    } else {
        packet[3] = 0x50000002;
        packet[4] = 0x8001;
        packet[5] = 0x10000000;
        packet[6] = 0xE;
        packet[7] = 0;
        packet += 8;
    }

    packet[0] = (u_int) tex0;
    packet[1] = (u_int) ((tex0 >> 32) & 0xFFFFFFFF);
    packet[2] = SCE_GS_TEX0_1;
    packet[3] = 0;

    if (tex1 != 0) {
        packet[4] = (u_int) tex1;
        packet[5] = (u_int) ((tex1 >> 32) & 0xFFFFFFFF);
        packet[6] = SCE_GS_TEX1_1;
        packet[7] = 0;
        return 0x10;
    }

    return 0xC;
}

/**
 * Clears the vector-unit visual's packet pointers and sizes.
 *
 * @mangled Initialize__10CVisualVu1Fv
 * @address 0x134EC0
 * @size 0x3C
 */
void CVisualVu1::Initialize() {
    CVisual::Initialize();
    vu_data = NULL;
    vu_size = 0;
    unk_00 = 0;
    unk_04 = 0;
}

/**
 * Constructs a vector-unit visual and clears it.
 *
 * @mangled __ct__10CVisualVu1Fv
 * @address 0x134F00
 * @size 0x50
 */
CVisualVu1::CVisualVu1() {
    Initialize();
}

/**
 * Points the visual at the model data it draws from.
 *
 * @mangled SetMDTDataAddress__13CVisualMDTVu1FPUi
 * @address 0x134F50
 * @size 0xC
 */
void CVisualMDTVu1::SetMDTDataAddress(u_int *data) {
    this->data = data;
}

/**
 * Gives the model data the visual draws from.
 *
 * @mangled GetMDTDataAddress__13CVisualMDTVu1Fv
 * @address 0x134F60
 * @size 0xC
 */
u_int *CVisualMDTVu1::GetMDTDataAddress() {
    return data;
}

/**
 * Clears the model visual's data pointer and packet.
 *
 * @mangled Initialize__13CVisualMDTVu1Fv
 * @address 0x134F70
 * @size 0x3C
 */
void CVisualMDTVu1::Initialize() {
    CVisualVu1::Initialize();
    copy_on_draw = false;
    data = NULL;
    vu_data_buffer[1] = NULL;
    vu_data_buffer[0] = NULL;
}

/**
 * Constructs a model visual and clears it.
 *
 * @mangled __ct__13CVisualMDTVu1Fv
 * @address 0x134FB0
 * @size 0x48
 */
CVisualMDTVu1::CVisualMDTVu1() {
    CVisualMDTVu1::Initialize();
}

/**
 * Draws the visual into a packet through the vector unit.
 *
 * @mangled DrawVu1__10CVisualVu1FPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
 * @address 0x135000
 * @size 0x964
 */

/**
 * Writes one vector-unit upload header and gives back its length in words.
 *
 * @mangled SetVuData__FiP1PUiP1P1P1P1i
 * @address 0x135970
 * @size 0x130
 */
static int SetVuData(int count, u_long128 *block, u_int *index, u_long128 *vertex, u_long128 *normal, u_long128 *uv, u_long128 *colour, int prim) {
    u_int     *header = (u_int *) block;
    u_int     *cursor = (u_int *) block;
    u_long128 *vertex_out;
    u_long128 *normal_out;
    u_long128 *uv_out;
    u_long128 *colour_out;

    header[0] = count | 0x8000;

    if (prim != 4) {
        cursor[1] = 0x302DC000;
        cursor += 2;
    } else {
        cursor[1] = 0x302E4000;
        cursor += 2;
    }

    cursor[0] = 0x412;
    cursor[1] = 0;
    cursor[2] = count;
    cursor[3] = prim;

    if (colour != NULL) {
        cursor[4] = 0x100;
    } else {
        cursor[4] = 0;
    }

    vertex_out = &block[2];
    normal_out = &vertex_out[count];
    uv_out = &normal_out[count];
    colour_out = &uv_out[count];

    for (int i = count; i > 0; i--) {
        *vertex_out++ = vertex[index[0]];
        *normal_out++ = normal[index[1]];
        *uv_out++ = uv[index[2]];
        index += 3;

        if (colour != NULL) {
            *colour_out++ = colour[*index];
            index++;
        }
    }

    int size = count * 3 + 2;

    if (colour != NULL) {
        size = count * 4 + 2;
    }

    return size;
}

/**
 * Builds a model's VU data block and returns its size in quadwords.
 *
 * @mangled CreateVUdataFromMDT__10CVisualVu1FPUiPUiii
 * @address 0x135AA0
 * @size 0x3A8
 */

/**
 * Draws the model, from a copy of this frame's block when the visual asks for one.
 *
 * @mangled DrawVu1__13CVisualMDTVu1FPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
 * @address 0x1360E0
 * @size 0x120
 */

/**
 * Draws the model into a VIF packet, choosing the buffer the frame is using.
 *
 * @mangled DrawVu1__13CVisualMDTVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
 * @address 0x136200
 * @size 0x34
 */

