#pragma once

/**
 * Pulls two cloth vertices towards the rest length given by the spring.
 */
void StretchBind2(float *point_a, float *point_b, float *spring);

/**
 * Returns the length of a vector.
 */
float vuabs(float *vector);
