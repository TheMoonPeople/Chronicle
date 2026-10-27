#pragma once

#include "common.h"

#include <libpkt.h>
#include <libvu0.h>

namespace dc::game {

class CBoxVu0;
class CCollision;
class CCPoly;
struct RenderInfo;

/**
 * Resets the EE DMA environment and the GS transfer path used for rendering.
 */
void DevInit();

/**
 * Per-frame drawing attributes; SetAttr copies them one field per mask bit.
 */
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

    /**
     * Constructs the attributes with their defaults.
     */
    CFrameAttr();

    /**
     * Resets every attribute to its default.
     */
    void Initialize();
};

/**
 * A node of the scene hierarchy: a named transform linked to its parent and siblings, with a cached world matrix.
 */
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

    /**
     * Rebuilds the inverse of the world matrix and returns it.
     */
    float (*GetInverseMatrix())[4];

    /**
     * Copies another frame's transform and attributes, detached from the hierarchy.
     */
    CFrame &operator=(CFrame &other);

    /**
     * Constructs a detached frame with an identity transform.
     */
    CFrame();

    /**
     * Sets the translation and invalidates the world matrix when it changes.
     */
    void SetPosition(float x, float y, float z);

    /**
     * Sets the translation from a vector and invalidates the world matrix when it changes.
     */
    void SetPosition(float *position);

    /**
     * Sets the scale and invalidates the world matrix when it changes.
     */
    void SetScale(float x, float y, float z);

    /**
     * Sets the scale from a vector and invalidates the world matrix when it changes.
     */
    void SetScale(float *scale);

    /**
     * Counts this frame and every frame below it.
     */
    int GetFrameNum();

    /**
     * Attaches the frame under a parent, unless it already has one.
     */
    void SetParent(CFrame *parent);

    /**
     * Appends a frame to the end of this frame's sibling list.
     */
    void SetBrother(CFrame *brother);

    /**
     * Appends a child frame, or clears the child list when given null.
     */
    void SetChild(CFrame *child);

    /**
     * Makes the frame follow another frame's transform without becoming its child.
     */
    void SetReference(CFrame *reference);

    /**
     * Stops following a referenced frame.
     */
    void DeleteReference();

    /**
     * Writes the local-to-world matrix, rebuilding the cache if it or a parent's is stale.
     */
    void GetLWMatrix(sceVu0FMATRIX matrix);

    /**
     * Sets the local transform from a matrix.
     */
    void SetTransMatrix(sceVu0FMATRIX matrix);

    /**
     * Sets the local rotation from a w-first quaternion, keeping the translation.
     */
    void SetTransMatrix(float *quaternion);

    /**
     * Finds the first frame in this subtree whose name stem matches name, or null.
     */
    CFrame *SearchFrame(char *name);

    /**
     * Writes the frame's bound, in its parent's space when it has one, optionally including children.
     */
    void GetBoundBox(CBoxVu0 *box, int children);

    /**
     * Scales the local bound about its centre and rebuilds its corners.
     */
    void ScaleBoundBox(float *scale);

    /**
     * Copies the attributes selected by mask (all when zero), optionally to the whole subtree.
     */
    void SetAttr(CFrameAttr &attr, int children, int mask);

    /**
     * Transforms a local point to world space.
     */
    void GetWorldPosition(float *world_point, float *local_point);

    /**
     * Sets the Euler rotation in radians and enables it.
     */
    void SetRotation(float x, float y, float z);

    /**
     * Writes the Euler rotation, or zero when none has been set.
     */
    void GetRotation(float *rotation);

    /**
     * Sets the rotation bits; bit 2 implies bit 1.
     */
    void SetRotType(int type);

    /**
     * Attaches collision polygons to the frame.
     */
    void SetCollision(CCollision *collision);

    /**
     * Collects the world-space collision polygons of this subtree that reach into box; returns their count.
     */
    int PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box);

    /**
     * Draws the frame into a word cursor; returns the number of words written.
     */
    virtual int DrawVu1(unsigned int *packet, RenderInfo *info);

    /**
     * Draws the frame into a VIF1 packet.
     */
    virtual int DrawVu1(sceVif1Packet *packet, RenderInfo *info);

    /**
     * Resets the frame to a detached identity transform with default attributes.
     */
    virtual void Initialize();
};

class CFrameVu1;

/**
 * Which of a frame's own axes `LookAt` holds perpendicular to the direction of its target.
 */
enum _FRAMECONSTRAINT {
    FRAME_CONSTRAINT_Z,
    FRAME_CONSTRAINT_Y,
    FRAME_CONSTRAINT_X
};

/**
 * Returns 1 when two frame names have the same stem before any "__", otherwise 0.
 */
int FrameNameComp(char *left, char *right);

/**
 * Applies the attribute flags encoded in a frame's name, optionally to its subtree.
 */
void SetFrameAttr(CFrame *frame, int recurse);

/**
 * Turns a frame towards a world-space point.
 */
int LookAt(CFrameVu1 *frame, float *target, _FRAMECONSTRAINT constraint);

/**
 * Turns a frame towards another frame.
 */
int LookAt(CFrameVu1 *frame, CFrameVu1 *target, _FRAMECONSTRAINT constraint);

} // namespace dc::game
