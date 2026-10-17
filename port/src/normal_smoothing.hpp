#pragma once

// Smoothing of the vertex normals of the models the port builds: retail's models are flat shaded, every face
// carrying its own normal, so a rounded thing is lit as a cut gem. With smoothing on, the normals of the vertices
// that sit at one point are averaged across the faces that meet there, except across an edge sharper than the
// crease angle, so a bottle's body is lit as a curve while its rim stays crisp.
//
// Off (0) unless DC_SMOOTH_NORMALS=<crease angle in degrees> is set, or a caller turns it on for the models it loads.

// Sets the crease angle in degrees for models built from now on; 0 turns smoothing off. Returns the previous angle.
float PortSetNormalSmoothing(float crease_degrees);
