#pragma once

/**
 * Loads the cell matrix and the column step that Trans_AddCell applies.
 */
void pretest(float matrix[4][4], float *translation);

/**
 * Transforms a grid position by the pretest matrix and advances it by one column step.
 */
void Trans_AddCell(float *output, float *position);
