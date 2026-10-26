#pragma once

#include "common.h"

#include <libvu0.h>

class CBoxVu0;
class CCollision;
class CCPoly;
struct RenderInfo;
class sceVif1Packet;

/**
 * Resets the EE DMA environment and the GS transfer path used for rendering.
 *
 * @mangled DevInit__Fv
 * @address 0x127BA0
 * @size 0x4C
 */
void DevInit();

/* The per-frame drawing attributes, kept inside the frame rather than beside it. SetAttr copies
   them one field per mask bit, so a field's position and width are settled while its meaning is
   not — which is why most of them are still unnamed. */
class CFrameAttr {
public:
    short         draw_on;     /**< Visibility bits: 1 draws the frame, 2 skips its children (and, without 1, the whole subtree), 4 is skipped by its parent. */
    float         clip_depth;  /**< Depth handed to the render info that the next frame's near-clip test compares against. */
    char          clip_enable; /**< Whether a frame crossing the clip depth is drawn with clipping; cleared by the 'n' name flag. */
    char          unk_09;
    char          remake_pending;  /**< Nonzero when the visual's vertex or material data changed and must be rebuilt before the next draw. */
    char          program_option;  /**< Sets bit 4 of the microprogram's draw flags; set by the 's' name flag. */
    char          fog_enable;      /**< Whether fog applies to the frame; cleared by the 'f' name flag. */
    char          far_clip_enable; /**< Whether far_clip holds a far clipping distance from the opening script. */
    char          unk_0E;
    char          unk_0F;
    float         far_clip;      /**< Far clipping distance the opening script sets. */
    char          use_color;     /**< Whether the frame is lit by color as ambient light instead of the scene's lights; set by the 'c' name flag. */
    sceVu0FVECTOR color;         /**< Ambient colour the frame is lit by while use_color is set. */
    char          cull_enable;   /**< Whether the frame's bound is tested against the screen before drawing. */
    char          ambient_boost; /**< Whether the frame drops the directional lights and adds 30% of the first light's colour to the ambient; set by the 't' name flag. */
    sceVu0FVECTOR unk_40;
    short         alpha_ref;    /**< Alpha test reference, or -1 to keep the default; set by the 'a' name flag. */
    short         blend_mode;   /**< Positive for additive blending, negative for subtractive, zero for the default. */
    char          depth_write;  /**< Whether the frame writes depth; cleared by the 'z' name flag. */
    char          ignore_depth; /**< Whether the frame passes the depth test regardless of depth; set by the 'o' name flag. */
    short         eye_relative; /**< Whether the microprogram receives the eye position in model space; set by the 'm' name flag. */
    short         billboard;    /**< Billboard mode: 2 turns the frame about Y towards the camera, 3 turns it fully; zero leaves the hierarchy's transform. */

    CFrameAttr();

    void Initialize();
};

/* A node of the scene hierarchy: a transform with a name, linked to its neighbours rather than
   held in an array. The world matrix is cached and world_valid says whether the cache still
   holds; everything that moves a frame clears it. */
class CFrame {
public:
    int           flags;       /**< Collision search bits: 1 searches this frame's collision, 2 skips its children, 4 alone excludes the frame. */
    CCollision   *collision;   /**< Collision polygons attached to the frame, or null. */
    sceVu0FVECTOR max;         /**< Maximum corner of the frame's bound in local space. */
    sceVu0FVECTOR min;         /**< Minimum corner of the frame's bound in local space. */
    sceVu0FVECTOR corner[8];   /**< Eight corners of the bound, kept for transforming it. */
    CFrameAttr    attr;        /**< Drawing attributes of the frame. */
    CFrame       *parent;      /**< Parent frame, or the referenced frame while reference is set. */
    int           reference;   /**< Whether parent is a followed frame rather than a true parent. */
    char          name[32];    /**< Frame name, whose part after "__" carries attribute flags. */
    CFrame       *child;       /**< First child frame, or null. */
    CFrame       *brother;     /**< Next sibling frame, or null. */
    CFrame       *elder;       /**< Previous sibling frame, or null. */
    sceVu0FMATRIX world;       /**< Cached local-to-world matrix. */
    sceVu0FMATRIX inverse;     /**< Inverse of the world matrix, as GetInverseMatrix last built it. */
    sceVu0FMATRIX local;       /**< Local transform relative to the parent. */
    sceVu0FVECTOR scale;       /**< Scale applied when the local matrix is built from its parts. */
    sceVu0FVECTOR position;    /**< Translation applied when the local matrix is built from its parts. */
    sceVu0FVECTOR rotation;    /**< Euler rotation in radians applied when rot_type bit 0 is set. */
    int           world_valid; /**< Whether the cached world matrix is current. */
    int           no_rotation; /**< Whether no rotation has been set, so GetRotation reports zero. */
    int           rot_type;    /**< Rotation bits: 1 applies rotation, 2 rotates about the frame's own origin. */
    int           srt;         /**< Whether the local matrix is rebuilt from scale, rotation and position instead of taken as set. */

    float (*GetInverseMatrix())[4];
    CFrame &operator=(CFrame &other);

    CFrame();

    void    SetPosition(float x, float y, float z);
    void    SetPosition(float *position);
    void    SetScale(float x, float y, float z);
    void    SetScale(float *scale);
    int     GetFrameNum();
    void    SetParent(CFrame *parent);
    void    SetBrother(CFrame *brother);
    void    SetChild(CFrame *child);
    void    SetReference(CFrame *reference);
    void    DeleteReference();
    void    GetLWMatrix(sceVu0FMATRIX matrix);
    void    SetTransMatrix(sceVu0FMATRIX matrix);
    void    SetTransMatrix(float *quaternion);
    CFrame *SearchFrame(char *name);
    void    GetBoundBox(CBoxVu0 *box, int children);
    void    ScaleBoundBox(float *scale);
    void    SetAttr(CFrameAttr &attr, int children, int mask);
    void    GetWorldPosition(float *world_point, float *local_point);
    void    SetRotation(float x, float y, float z);
    void    GetRotation(float *rotation);
    void    SetRotType(int type);
    void    SetCollision(CCollision *collision);
    int     PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box);

    virtual int  DrawVu1(unsigned int *packet, RenderInfo *info);
    virtual int  DrawVu1(sceVif1Packet *packet, RenderInfo *info);
    virtual void Initialize();
};

class CFrameVu1;

/** Which of a frame's own axes `LookAt` holds perpendicular to the direction of its target. */
enum _FRAMECONSTRAINT {
    FRAME_CONSTRAINT_Z,
    FRAME_CONSTRAINT_Y,
    FRAME_CONSTRAINT_X
};

int  FrameNameComp(char *left, char *right);
void SetFrameAttr(CFrame *frame, int recurse);
int  LookAt(CFrameVu1 *frame, float *target, _FRAMECONSTRAINT constraint);
int  LookAt(CFrameVu1 *frame, CFrameVu1 *target, _FRAMECONSTRAINT constraint);
