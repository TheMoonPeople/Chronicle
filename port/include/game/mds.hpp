#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc_fwd.hpp"

namespace dc::game {

class CCollisionMDT;
class CFrame;
class CFrameVu1;
class CVisualMDTVu1;

/**
 * Header of a scene file: one fixed-size record per object follows it, then the model files those
 * records point into. The file is loaded as one block and nothing in it is relocated, so every
 * offset a record carries is counted from the start of the file rather than from the record.
 */
struct MDS_HEADER {
    int          unk_00[2];
    unsigned int object_num; /**< Number of object records following the header. */
    int          unk_08;
};

/**
 * One object of the scene. The name is what a model is looked up and filtered by, the parent is an
 * index into the same array with a negative meaning none, and the matrix is the object's transform
 * in its parent's space.
 */
struct MDS_OBJECT {
    int           unk_00[2];
    char          name[32]; /**< Null-terminated object name. */
    int           data_ofs; /**< Model-data offset from the start of the MDS image. */
    int           parent;   /**< Parent object index, or a negative value for no parent. */
    sceVu0FMATRIX matrix;   /**< Transform relative to the parent object. */
};

/**
 * Quadwords the last scene loaded spent on visual data, written by the loader for whoever is
 * accounting for the block it filled.
 */
extern int dset_mds_packet;

/**
 * Number of objects the last scene loaded held, written by the loader alongside
 * dset_mds_packet.
 */
extern int dset_mds_objnum;

/**
 * Turns a scene into an array of frames, one per object, parented to each other as the file says.
 * The two name lists select objects that get their visual built differently; either may be null.
 */
CFrameVu1 *LoadMDSFile(u_int *data, CDataAlloc2<1> *alloc, int attr, char **double_names, char **retain_names);

/**
 * Turns a scene into an array of frames the same way as the five-argument form, against the
 * global visual allocator and with no name lists.
 */
CFrameVu1 *LoadMDSFile(u_int *data, int attr, int unused);

/**
 * Turns the four levels of detail of a scene into frames, one data pointer and one frame
 * pointer per level; a level with no data gets a null frame.
 */
void LoadMDSFileLOD(CFrameVu1 **frame, u_int **data, CDataAlloc2<1> *alloc, int attr);

/**
 * Turns a collision scene into frames, whose objects carry shapes to be asked questions of rather
 * than geometry to be drawn.
 */
CFrameVu1 *LoadCollisionFile(u_int *data, CDataAlloc2<1> *alloc);

/**
 * Turns a collision scene into frames the same way as the two-argument form, against the global
 * visual allocator.
 */
CFrameVu1 *LoadCollisionFile(u_int *data);

/**
 * Copies a drawable frame subtree into an allocator. The copies are parented to each other rather
 * than to the originals.
 */
CFrameVu1 *CopyFrameVu1(CFrameVu1 *frame, CDataAlloc2<1> *alloc);

/**
 * Copies a frame subtree into an allocator. The copies are parented to each other rather than to
 * the originals.
 */
CFrame *CopyFrame(CFrame *frame, CDataAlloc2<1> *alloc);

/**
 * Does nothing; it is kept because it is exported, though nothing calls it.
 */
void LoadLODData(CFrameVu1 **frame, char **name, u_int *data, int attr);

/**
 * Turns one model file into the visual that draws it.
 */
CVisualMDTVu1 *CreateVisual(u_int *data, CDataAlloc2<1> *alloc, int attr);

/**
 * Turns one collision model file into the object that answers queries about it.
 */
CCollisionMDT *CreateCollisionMDT(u_int *data, CDataAlloc2<1> *alloc);

} // namespace dc::game
