#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Describes the sections and vertex data of an MDT resource.
 */
struct MDT_HEADER {
    int          unk_00[2];
    unsigned int size;         /**< Size of the whole resource in bytes. */
    int          vertex_num;   /**< Number of vertex positions. */
    int          vertex_ofs;   /**< Byte offset from the header to the vertex positions. */
    int          normal[2];    /**< Normal vectors; the second word is their byte offset from the header. */
    int          colour_count; /**< Number of editable per-vertex colour vectors. */
    int          colour_ofs;   /**< Byte offset from the header to those colour vectors. */
    int          unk_24;
    int          mesh_ofs; /**< Byte offset from the header to the mesh section: draw strips, collision triangles or shadow shapes. */
    int          uv[3];    /**< Texture coordinates; the second word is their byte offset from the header. */
    int          info_ofs; /**< Byte offset from the header to the material table, or to the polygon attributes of a collision mesh. */
};

/**
 * Stores the vector parameters and texture name for an MDT material.
 */
struct MDT_MATERIAL {
    sceVu0FVECTOR diffuse;  /**< Diffuse colour; its fourth component is the material's opacity. */
    sceVu0FVECTOR ambient;  /**< Ambient colour of the material. */
    sceVu0FVECTOR specular; /**< Third colour vector the material uploads to the vector unit. */
    int           unk_30;
    char          texture[44]; /**< Name of the texture the material draws with. */
};

/**
 * Stores the three vertex indices of an MDT collision triangle.
 */
struct MDT_CPOLY {
    int vertex[3];  /**< Indices of the triangle's three vertex positions. */
    int info_index; /**< Index of the triangle's attribute record, or negative for none. */
    int unk_10;
};

/**
 * Describes the triangle array in an MDT collision mesh.
 */
struct MDT_CPOLY_SET {
    int          unk_00;
    unsigned int num;     /**< Number of triangles in the set. */
    MDT_CPOLY    poly[1]; /**< The triangles, as many as num gives. */
};

/**
 * Stores the collision-mesh section of an MDT resource.
 */
struct MDT_COLLISION {
    int           unk_00[4];
    MDT_CPOLY_SET set; /**< Triangles of the collision mesh. */
};

/**
 * Stores a vertex reference and edge state for an MDT shadow triangle.
 */
struct MDT_SVERTEX {
    int index; /**< Index of the corner's vertex position. */
    int unk_04;
    int edge; /**< Set when the edge from this corner to the next joins two faces facing the same way, so it casts no silhouette. */
};

/**
 * Describes one indexed shape in an MDT shadow mesh.
 */
struct MDT_SSHAPE {
    int         unk_00;
    int         index_num; /**< Number of corners in the shape, three per triangle. */
    int         unk_08;
    MDT_SVERTEX vertex[1]; /**< Corners of the shape's triangles, as many as index_num gives. */
};

/**
 * Stores the shape array in an MDT shadow-mesh section.
 */
struct MDT_SHADOW {
    int        unk_00[2];
    int        shape_num; /**< Number of shapes in the section. */
    int        unk_0c;
    MDT_SSHAPE shape[1]; /**< First shape; each is followed directly by the next. */
};
