#pragma once

#include "menu_draw.hpp"

/**
 * Loads a menu data file by a constant name into the buffer.
 */
inline int LoadFileMenuData(const char *name, unsigned int *buffer) {
    return LoadFileMenuData(const_cast<char *>(name), buffer);
}
