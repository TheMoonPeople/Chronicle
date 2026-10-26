#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 199

#include "dataset.hpp"

#include <eekernel.h>
#include <libvu0.h>

#include <cstdio>
#include <cstring>

#include "boxvu0.hpp"
#include "collisionmdt.hpp"
#include "dataalloc.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "visual.hpp"

void CCollisionMDT::Initialize() {
    model = 0;
    mesh = 0;
    mesh_count = 0;
}

u_int          *read_buffer;
CDataAlloc2<1> *WorkBuffer;
CDataAlloc2<1> *ActiveData;

CDataAlloc2<1>         VisualData(-1);
CDataAlloc2<1>         MotionData(-1);
CDataAlloc2<1>         TextureData(-1);
CDataAlloc2<1>         WaterData(-1);
CDataAlloc2<1>         VariousData(-1);
CDataAlloc2<1>         ActiveData0(-1);
CDataAlloc2<1>         ActiveData1(-1);
CDataAlloc<1, 1690000> GlobalDataBuffer;
CDataAlloc2<1>         workbuffer(-1);
void SetDataBuffer(CDataAlloc2<1> *arena, int quads);

void SetPacketReadBuffer(int packet_quads, int read_quads);

static int htoi(char *text) {
    char *cursor;
    int   len;
    int   value;
    int   i;
    int   place;
    int   ch;
    int   digit;

    cursor = text;
    len = 0;
    value = 0;

    while (*cursor++ != '\0') {
        len++;
    }

    place = 1;

    for (i = 0; i < len; i++) {
        ch = (unsigned char) text[len - i - 1];
        digit = 0;

        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        }

        if (ch >= 'a' && ch <= 'f') {
            digit = ch - 'a' + 10;
        }

        if (ch >= 'A' && ch <= 'F') {
            digit = ch - 'A' + 10;
        }

        value += digit * place;
        place <<= 4;
    }

    return value;
}

void SetFrameAttr(CFrame *frame, int recurse) {
    int     has_codes;
    char   *cursor;
    char    blend_code[3];
    CFrame *child;

    frame->attr.clip_enable = true;
    frame->attr.clip_depth = 150.0f;
    has_codes = 1;
    cursor = frame->name;

    if (*cursor == '\0') {
        has_codes = 0;
    }

    cursor++;

    for (;;) {
        if (*cursor == '\0') {
            has_codes = 0;
            break;
        }

        if (*cursor == '_' && cursor[-1] == '_') {
            break;
        }

        cursor++;
    }

    while (has_codes) {
        if (*cursor == '\0') {
            break;
        }

        switch (*cursor) {
            case 'c':
            case 'C':
                frame->attr.use_color = true;
                frame->attr.color[0] = 128.0f;
                frame->attr.color[1] = 128.0f;
                frame->attr.color[2] = 128.0f;
                frame->attr.color[3] = 128.0f;
                break;
            case 'n':
            case 'N':
                frame->attr.clip_enable = false;
                break;
            case 'a':
            case 'A':
                blend_code[0] = cursor[1];
                cursor += 2;
                blend_code[1] = *cursor;
                blend_code[2] = '\0';

                if (blend_code[0] >= 'a' && blend_code[0] <= 'z') {
                    blend_code[0] -= 32;
                }

                if (blend_code[1] >= 'a' && blend_code[1] <= 'z') {
                    blend_code[1] -= 32;
                }

                if (blend_code[0] == 'P' && blend_code[1] == 'P') {
                    frame->attr.blend_mode = 1;
                } else if (blend_code[0] == 'N' && blend_code[1] == 'N') {
                    frame->attr.blend_mode = -1;
                } else {
                    frame->attr.alpha_ref = htoi(blend_code);
                }

                break;
            case 'z':
            case 'Z':
                frame->attr.depth_write = false;
                break;
            case 'f':
            case 'F':
                frame->attr.fog_enable = false;
                break;
            case 's':
            case 'S':
                frame->attr.program_option = 1;
                break;
            case 'm':
            case 'M':
                frame->attr.eye_relative = true;
                break;
            case 'b':
            case 'B':
                cursor++;

                if (*cursor == '\0') {
                    cursor--;
                    break;
                }

                if (*cursor == 'Y' || *cursor == 'y') {
                    frame->attr.billboard = 2;
                }

                if (*cursor == 'A' || *cursor == 'a') {
                    frame->attr.billboard = 3;
                }

                break;
            case 't':
            case 'T':
                frame->attr.ambient_boost = true;
                break;
            case 'o':
            case 'O':
                frame->attr.ignore_depth = true;
                break;
            case 'v':
            case 'V':
                frame->attr.draw_on = 2;
                frame->flags = 2;
                break;
        }

        cursor++;
    }

    if (recurse) {
        for (child = frame->child; child != 0; child = child->brother) {
            SetFrameAttr(child, recurse);
        }
    }
}

int dset_mds_packet;
int dset_mds_objnum;

/* One edge of a shadow model while the pairing pass is running. The pass is over pairs, so an edge
   already matched is skipped rather than removed, and the pointer is where the answer is written
   back into the model itself. */
struct SHADOW_EDGE {
    short v0;        /**< Index of the vertex the edge starts at. */
    short v1;        /**< Index of the vertex the edge ends at. */
    short done;      /**< Whether the edge has already been paired. */
    int  *edge_flag; /**< Model's flag for the edge, set when the edge is interior. */
};

static void ArrangeShadowMDT(u_int *data);
static void CreateBBox(CBox3<float> *box, sceVu0FVECTOR *vertex, int num);

/* A scene file turned into frames. Every object of the file gets one, in the file's own order, so
   that a parent index is an index into the array being built; an object whose name marks it as a
   locator gets a frame and nothing else. */
CFrameVu1 *LoadMDSFile(u_int *data, CDataAlloc2<1> *alloc, int attr, char **double_names, char **retain_names) {
    /* Declared in this order because it is what lays them out in small data, and every reference
       to either is a displacement off $gp that the order decides. */
    static int  flag;
    static char init;

    u_int          i;
    char         **double_entry;
    char         **retain_entry;
    float          sx;
    float          sy;
    float          sz;
    float          rx;
    float          ry;
    float          rz;
    MDS_HEADER    *header;
    MDS_OBJECT    *object;
    CFrameVu1     *node;
    CFrameVu1     *frames;
    CVisualMDTVu1 *visual;
    u_long128     *block;
    float         *extremes[2];
    sceVu0FMATRIX  matrix;
    int            j;
    int            k;
    int            saved_attr;

    if (!data) {
        return 0;
    }

    /* The file is read into place by whoever loaded it and everything inside is addressed as
       quadwords from its front, so a misaligned block would mislay all of it. */
    if ((int) data % 16) {
        printf("address error!! %d \n", data);
    }

    FlushCache(0);

    if (!init) {
        flag = 0;
        init = 1;
    }

    header = (MDS_HEADER *) data;
    data += sizeof(MDS_HEADER) / sizeof(u_int);

    dset_mds_packet = 0;
    dset_mds_objnum = header->object_num;

    block = (u_long128 *) alloc->Alloc(header->object_num * sizeof(CFrameVu1) / 16);
    frames = new (block) CFrameVu1[header->object_num];

    for (i = 0; i < header->object_num; i++) {
        object = (MDS_OBJECT *) data;
        data += sizeof(MDS_OBJECT) / sizeof(u_int);

        if (memcmp(object->name, "func_", 5) == 0) {
            continue;
        }

        node = &frames[i];
        node->Initialize();
        sz = sy = sx = 1.0f;
        node->SetScale(sx, sy, sz);
        rx = ry = rz = 0.0f;
        node->SetRotation(rx, ry, rz);
        node->SetPosition(0.0f, 0.0f, 0.0f);

        for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; k++) {
                matrix[k][j] = object->matrix[k][j];
            }
        }

        strcpy(node->name, object->name);
        node->SetTransMatrix(matrix);
        SetFrameAttr(node, 0);

        if (object->parent < 0) {
            node->SetParent(0);
        } else {
            node->SetParent(&frames[object->parent]);
        }

        if (object->data_ofs) {
            MDT_HEADER *model = (MDT_HEADER *) ((char *) header + object->data_ofs);

            sceVu0FVECTOR *vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
            int            vertex_num = model->vertex_num;

            CreateBBox((CBox3<float> *) node->max, vertex, vertex_num);

            extremes[0] = node->min;
            extremes[1] = node->max;

            for (j = 0; j < 8; j++) {
                node->corner[j][3] = 1.0f;
                node->corner[j][0] = extremes[(j & 1) != 0][0];
                node->corner[j][1] = extremes[(j & 2) != 0][1];
                node->corner[j][2] = extremes[(j & 4) != 0][2];
            }

            saved_attr = attr;
            double_entry = double_names;

            if (double_entry) {
                while (*double_entry) {
                    if (FrameNameComp(node->name, *double_entry)) {
                        attr |= 2;
                        attr |= 4;
                        break;
                    }

                    double_entry++;
                }
            }

            retain_entry = retain_names;

            if (retain_entry) {
                while (*retain_entry) {
                    if (FrameNameComp(node->name, *retain_entry)) {
                        attr |= 2;
                        break;
                    }

                    retain_entry++;
                }
            }

            visual = CreateVisual((u_int *) ((char *) header + object->data_ofs), alloc, attr);

            if (attr & 2) {
                node->SetVisual(visual);
            } else {
                node->SetVisual(visual);
            }

            attr = saved_attr;
        }
    }

    flag = 1;
    return frames;
}

/* One model file turned into the object that draws it. Which of the three kinds it becomes is the
   attribute's business, and so is whether the model file is kept beside the built block and whether
   a second block is built so that the drawing has two to alternate between. */
CVisualMDTVu1 *CreateVisual(u_int *data, CDataAlloc2<1> *alloc, int attr) {
    CVisualMDTVu1 *visual;
    u_int         *copy;

    if (!data) {
        return 0;
    }

    if (attr & 2) {
        if (attr & 8) {
            visual = new ((u_long128 *) alloc->Alloc(3)) CVisualShadow;
        } else {
            visual = new ((u_long128 *) alloc->Alloc(3)) CVisualMDTVu1;
        }
    } else {
        if (attr & 8) {
            visual = new ((u_long128 *) alloc->Alloc(3)) CVisualShadow;
        } else {
            visual = (CVisualMDTVu1 *) new ((u_long128 *) alloc->Alloc(2)) CVisualVu1;
        }
    }

    visual->Initialize();
    alloc->Align64();

    /* The block is written where the allocator would hand out next and reserved afterwards, since
       only the visual knows how much of it the model needed. */
    if (attr & 8) {
        ((CVisualShadow *) visual)
            ->CreateVUdataShadow((u_int *) (alloc->base + alloc->used * 16), data);
        visual->vu_data_buffer[0] = visual->vu_data;
        visual->vu_data_buffer[1] = visual->vu_data;
        ArrangeShadowMDT(data);
    } else {
        visual->CreateVUdataFromMDT((u_int *) (alloc->base + alloc->used * 16), data, 0, 0);
    }

    alloc->Alloc(visual->vu_size);
    dset_mds_packet += visual->vu_size;
    alloc->Align64();

    if (attr & 2) {
        alloc->Align64();
        copy = (u_int *) alloc->Alloc((((MDT_HEADER *) data)->size >> 4) + 1);
        memcpy(copy, data, ((MDT_HEADER *) data)->size);

        if (attr & 2) {
            visual->SetMDTDataAddress(copy);
        }

        if (attr & 0x10) {
            visual->copy_on_draw = true;
        }

        if ((attr & 4) && !(attr & 0x10)) {
            alloc->Align64();

            if (attr & 8) {
                visual->vu_data_buffer[0] = visual->vu_data;
                ((CVisualShadow *) visual)
                    ->CreateVUdataShadow((u_int *) (alloc->base + alloc->used * 16), data);
            } else {
                visual->vu_data_buffer[0] = visual->vu_data;
                visual->CreateVUdataFromMDT((u_int *) (alloc->base + alloc->used * 16), data, 0, 0);
            }

            alloc->Alloc(visual->vu_size);
            visual->vu_data_buffer[1] = visual->vu_data;
        } else {
            visual->vu_data_buffer[0] = visual->vu_data;
            visual->vu_data_buffer[1] = visual->vu_data;
        }
    }

    alloc->Align64();
    return visual;
}

/* Which of a shadow model's edges lie on its silhouette, decided once here rather than per frame.
   Every edge of every triangle is listed with the plane normal of the triangle it came from; two
   edges that are the same pair of corners in opposite order belong to the two triangles that share
   them, and if those two face nearly the same way the edge is interior and is switched off in the
   model itself. What is left switched on is the outline. */
static void ArrangeShadowMDT(u_int *data) {
    SHADOW_EDGE   edges[1024];
    sceVu0FVECTOR normals[1024];
    sceVu0FVECTOR side0;
    sceVu0FVECTOR side1;
    sceVu0FVECTOR face;

    sceVu0FVECTOR *vertex;
    MDT_HEADER    *model;
    MDT_SHADOW    *mesh;
    MDT_SSHAPE    *shape;
    int            edge_count;
    int            i;
    int            j;
    MDT_SVERTEX   *corner;
    int            shape_num;
    int            index_count;
    int            k;
    int            m;

    model = (MDT_HEADER *) data;
    mesh = (MDT_SHADOW *) ((char *) model + model->mesh_ofs);
    vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);

    shape_num = mesh->shape_num;
    shape = mesh->shape;
    edge_count = 0;

    for (i = 0; i < shape_num; i++) {
        index_count = shape->index_num;
        corner = shape->vertex;

        for (j = 0; j < index_count / 3; j++) {
            sceVu0SubVector(side0, vertex[corner[1].index], vertex[corner[0].index]);
            sceVu0SubVector(side1, vertex[corner[2].index], vertex[corner[0].index]);
            sceVu0OuterProduct(face, side0, side1);
            sceVu0Normalize(normals[edge_count], face);
            sceVu0CopyVector(normals[edge_count + 1], normals[edge_count]);
            sceVu0CopyVector(normals[edge_count + 2], normals[edge_count]);

            edges[edge_count].v0 = corner[0].index;
            edges[edge_count].v1 = corner[1].index;
            edges[edge_count].done = 0;
            edges[edge_count].edge_flag = &corner[0].edge;
            corner[0].edge = false;
            edge_count++;

            edges[edge_count].v0 = corner[1].index;
            edges[edge_count].v1 = corner[2].index;
            edges[edge_count].done = 0;
            edges[edge_count].edge_flag = &corner[1].edge;
            corner[1].edge = false;
            edge_count++;

            edges[edge_count].v0 = corner[2].index;
            edges[edge_count].v1 = corner[0].index;
            edges[edge_count].done = 0;
            edges[edge_count].edge_flag = &corner[2].edge;
            corner[2].edge = false;
            edge_count++;

            if (edge_count > 1020) {
                printf("shadow initialize failed\n");

                while (1)
                    ;
            }

            corner += 3;
        }

        shape = (MDT_SSHAPE *) corner;
    }

    for (k = 0; k < edge_count - 1; k++) {
        if (edges[k].done) {
            continue;
        }

        for (m = k + 1; m < edge_count; m++) {
            if (edges[m].done) {
                continue;
            }

            if (edges[k].v0 != edges[m].v1) {
                continue;
            }

            if (edges[k].v1 != edges[m].v0) {
                continue;
            }

            if (DistVector(normals[k], normals[m]) < 0.0008f) {
                edges[k].done = 1;
                *edges[k].edge_flag = 1;
                edges[m].done = 1;
                *edges[m].edge_flag = 1;
                break;
            }
        }
    }
}

/* The four detail levels of one object, loaded as four scenes. A level the caller has no file for
   leaves a null behind rather than a scene with nothing in it. */
void LoadMDSFileLOD(CFrameVu1 **frame, u_int **data, CDataAlloc2<1> *alloc, int attr) {
    int i;

    for (i = 0; i < 4; i++) {
        if (!data[i]) {
            frame[i] = 0;
        } else {
            frame[i] = LoadMDSFile(data[i], alloc, attr, 0, 0);
        }
    }
}

/* A collision scene turned into frames. It is the scene loader with the drawing left out: the same
   file shape and the same parenting, but every object carries a shape to be asked questions of
   rather than geometry to be drawn, and nothing scales or positions a frame because the transform
   the file carries is the whole of it. */
CFrameVu1 *LoadCollisionFile(u_int *data, CDataAlloc2<1> *alloc) {
    CFrameVu1    *frames;
    u_int         i;
    u_int        *cursor;
    MDS_HEADER   *header;
    MDS_OBJECT   *object;
    CFrameVu1    *node;
    u_long128    *block;
    float        *extremes[2];
    sceVu0FMATRIX matrix;
    int           j;
    int           k;
    int           corner;

    cursor = data;
    header = (MDS_HEADER *) data;
    cursor += sizeof(MDS_HEADER) / sizeof(u_int);

    if (!((MDS_HEADER *) data)->object_num) {
        return 0;
    }

    block = (u_long128 *) alloc->Alloc(((MDS_HEADER *) data)->object_num * sizeof(CFrameVu1) / 16);
    frames = new (block) CFrameVu1[header->object_num];

    for (i = 0; i < header->object_num; i++) {
        object = (MDS_OBJECT *) cursor;
        cursor += sizeof(MDS_OBJECT) / sizeof(u_int);

        node = &frames[i];
        node->Initialize();

        for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; k++) {
                matrix[k][j] = object->matrix[k][j];
            }
        }

        strcpy(node->name, object->name);
        node->SetTransMatrix(matrix);

        if (object->parent < 0) {
            node->SetParent(0);
        } else {
            node->SetParent(&frames[object->parent]);
        }

        if (object->data_ofs) {
            MDT_HEADER *model = (MDT_HEADER *) ((char *) header + object->data_ofs);

            node->SetCollision(CreateCollisionMDT((u_int *) model, alloc));
            sceVu0FVECTOR *vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
            int            vertex_num = model->vertex_num;

            CreateBBox((CBox3<float> *) node->max, vertex, vertex_num);

            extremes[0] = node->min;
            extremes[1] = node->max;

            for (corner = 0; corner < 8; corner++) {
                node->corner[corner][3] = 1.0f;
                node->corner[corner][0] = extremes[(corner & 1) != 0][0];
                node->corner[corner][1] = extremes[(corner & 2) != 0][1];
                node->corner[corner][2] = extremes[(corner & 4) != 0][2];
            }
        }
    }

    return frames;
}

/* One model file turned into the shape a frame answers about. The file is copied into the block
   first, because everything the collision holds points into that copy and the caller's own is not
   expected to outlive the load; the triangles are then built out of it with the bound of each
   beside it, which is what lets a box query reject one without reading a vertex. */
CCollisionMDT *CreateCollisionMDT(u_int *data, CDataAlloc2<1> *alloc) {
    u_long128     *info;
    MDT_HEADER    *model;
    u_long128     *vertex;
    MDT_CPOLY     *poly;
    u_int          i;
    u_int          poly_count;
    CCollisionMDT *collision;
    MDT_HEADER    *source;
    MDT_CPOLY_SET *poly_set;
    CCPolyBox     *built;
    int            index;

    source = (MDT_HEADER *) data;
    model = (MDT_HEADER *) alloc->Alloc64((((MDT_HEADER *) data)->size >> 4) + 1);
    memcpy(model, source, source->size);

    collision = new ((u_long128 *) alloc->Alloc(4)) CCollisionMDT;
    collision->model = model;
    collision->CreateBBox();

    vertex = (u_long128 *) ((char *) model + model->vertex_ofs);
    info = (u_long128 *) ((char *) model + model->info_ofs);
    poly_set = &((MDT_COLLISION *) ((char *) model + model->mesh_ofs))->set;
    poly = poly_set->poly;

    built = (CCPolyBox *) alloc->Alloc(poly_set->num * sizeof(CCPolyBox) / 16);

    for (i = 0; i < (poly_count = poly_set->num); i++) {
        *(u_long128 *) built[i].poly.vertex[0] = vertex[poly->vertex[0]];
        *(u_long128 *) built[i].poly.vertex[1] = vertex[poly->vertex[1]];
        *(u_long128 *) built[i].poly.vertex[2] = vertex[poly->vertex[2]];

        index = poly->info_index;

        if (!model->info_ofs || index < 0) {
            memset(&built[i].poly.info, 0, 16);
        } else {
            *(u_long128 *) &built[i].poly.info = info[index];
        }

        poly++;

        VectorMaxMin(built[i].box.max, built[i].box.min, built[i].poly.vertex[0], built[i].poly.vertex[1], built[i].poly.vertex[2]);
        PlaneNormal(built[i].poly.normal, built[i].poly.vertex[0], built[i].poly.vertex[1], built[i].poly.vertex[2]);
    }

    collision->mesh = built;
    collision->mesh_count = poly_count;
    return collision;
}

/* A scene loaded into the allocator every scene without one of its own goes into. */
CFrameVu1 *LoadMDSFile(u_int *data, int attr, int unused) {
    return LoadMDSFile(data, &VisualData, attr, 0, 0);
}

/* The extent of a run of points, which is a model's bound when the points are its vertices. A model
   with no vertex array at all answers an empty bound rather than reading one. */
static void CreateBBox(CBox3<float> *box, sceVu0FVECTOR *vertex, int num) {
    int i;

    if (!vertex) {
        box->max[0] = box->min[0] = 0.0f;
        box->max[1] = box->min[1] = 0.0f;
        box->max[2] = box->min[2] = 0.0f;
        return;
    }

    box->max[0] = vertex[0][0];
    box->max[1] = vertex[0][1];
    box->max[2] = vertex[0][2];
    box->min[0] = vertex[0][0];
    box->min[1] = vertex[0][1];
    box->min[2] = vertex[0][2];

    for (i = 0; i < num; i++) {
        if (box->max[0] < vertex[0][0]) {
            box->max[0] = vertex[0][0];
        }

        if (box->max[1] < vertex[0][1]) {
            box->max[1] = vertex[0][1];
        }

        if (box->max[2] < vertex[0][2]) {
            box->max[2] = vertex[0][2];
        }

        if (box->min[0] > vertex[0][0]) {
            box->min[0] = vertex[0][0];
        }

        if (box->min[1] > vertex[0][1]) {
            box->min[1] = vertex[0][1];
        }

        if (box->min[2] > vertex[0][2]) {
            box->min[2] = vertex[0][2];
        }

        vertex++;
    }
}

/* A subtree copied into an allocator, node by node. Each copy is constructed where the allocator
   hands out next and then assigned from its original, so what a node carries travels with it and
   what ties it to its neighbours does not — the parenting is redone from the copies as the walk
   descends. */
CFrameVu1 *CopyFrameVu1(CFrameVu1 *frame, CDataAlloc2<1> *alloc) {
    CFrame    *first_child;
    CFrame    *child;
    CFrameVu1 *copy;

    if (!frame) {
        return 0;
    }

    copy = new ((u_long128 *) alloc->Alloc(sizeof(CFrameVu1) / 16)) CFrameVu1;
    copy->Initialize();
    *copy = *frame;

    first_child = frame->child;

    for (child = first_child; child; child = child->brother) {
        CopyFrameVu1((CFrameVu1 *) child, alloc)->SetParent(copy);
    }

    return copy;
}

/* The same over frames that draw nothing. */
CFrame *CopyFrame(CFrame *frame, CDataAlloc2<1> *alloc) {
    CFrame *first_child;
    CFrame *child;
    CFrame *copy;

    if (!frame) {
        return 0;
    }

    copy = new ((u_long128 *) alloc->Alloc(sizeof(CFrame) / 16)) CFrame;
    copy->Initialize();
    *copy = *frame;

    first_child = frame->child;

    for (child = first_child; child; child = child->brother) {
        CopyFrame(child, alloc)->SetParent(copy);
    }

    return copy;
}

void LoadLODData(CFrameVu1 **frame, char **name, u_int *data, int attr) {
}

/* A collision scene loaded into the allocator every scene without one of its own goes into. */
CFrameVu1 *LoadCollisionFile(u_int *data) {
    return LoadCollisionFile(data, &VisualData);
}

int CCollision::GetPolygon(int index, sceVu0FMATRIX v0, sceVu0FMATRIX v1, sceVu0FMATRIX v2) {
    return 0;
}

int CCollision::GetMaxY(float *position) {
    return 0;
}

int CCollision::Intersection(float *from, float *to, float *hit) {
    return 0;
}

int CCollision::PickUpNearPoly(CCPoly *poly) {
    return 0;
}

int CCollision::PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box) {
    return 0;
}

int CCollision::PickUpNearPoly(CCPoly *poly, float *position, float radius) {
    return 0;
}

void CCollision::Initialize() {
}
