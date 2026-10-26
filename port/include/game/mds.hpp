#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc_fwd.hpp"

class CCollisionMDT;
class CFrame;
class CFrameVu1;
class CVisualMDTVu1;

/**
 * The scene file: a header, then one fixed-size record per object, then the model files those
   records point into. It is loaded as one block and nothing in it is relocated, so every offset a
   record carries is counted from the start of the file rather than from the record.
 */

struct MDS_HEADER {
    int          unk_00[2];
    unsigned int object_num; /**< Number of object records following the header. */
    int          unk_08;
};

/**
 * One object of the scene. The name is what a model is looked up and filtered by, the parent is an
   index into the same array with a negative meaning none, and the matrix is the object's transform
   in its parent's space.
 */
struct MDS_OBJECT {
    int           unk_00[2];
    char          name[32]; /**< Null-terminated object name. */
    int           data_ofs; /**< Model-data offset from the start of the MDS image. */
    int           parent;   /**< Parent object index, or a negative value for no parent. */
    sceVu0FMATRIX matrix;   /**< Transform relative to the parent object. */
};

/**
 * How many quadwords the last scene loaded spent on visual data, and how many objects it held.
   Both are written by the loader and read by whoever is accounting for the block it filled.
 */
extern int dset_mds_packet;
extern int dset_mds_objnum;

/**
 * A scene turned into an array of frames, one per object, parented to each other as the file says.
   The two name lists select objects that get their visual built differently; either may be null.
   The three-argument form is the same thing against the global visual allocator.
 */
CFrameVu1 *LoadMDSFile(u_int *data, CDataAlloc2<1> *alloc, int attr, char **double_names, char **retain_names);
CFrameVu1 *LoadMDSFile(u_int *data, int attr, int unused);
void       LoadMDSFileLOD(CFrameVu1 **frame, u_int **data, CDataAlloc2<1> *alloc, int attr);

/**
 * The same for a collision scene, whose objects carry shapes to be asked questions of rather than
   geometry to be drawn.
 */
CFrameVu1 *LoadCollisionFile(u_int *data, CDataAlloc2<1> *alloc);
CFrameVu1 *LoadCollisionFile(u_int *data);

/**
 * A subtree copied into an allocator, which is the only way a frame hierarchy is duplicated: the
   copies are parented to each other rather than to the originals.
 */
CFrameVu1 *CopyFrameVu1(CFrameVu1 *frame, CDataAlloc2<1> *alloc);
CFrame    *CopyFrame(CFrame *frame, CDataAlloc2<1> *alloc);

/**
 * Exported and empty, and nothing in either program calls it.
 */
void LoadLODData(CFrameVu1 **frame, char **name, u_int *data, int attr);

/**
 * One model file turned into the thing that draws it, or into the thing that answers about it.
 */
CVisualMDTVu1 *CreateVisual(u_int *data, CDataAlloc2<1> *alloc, int attr);
CCollisionMDT *CreateCollisionMDT(u_int *data, CDataAlloc2<1> *alloc);
