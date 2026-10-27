#include "dungeonparts.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <print>
#include <span>
#include <vector>

#include "boxvu0.hpp"
#include "collision.hpp"
#include "dngstatusdata.hpp"
#include "dranmapfield.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "editloop.hpp"
#include "editmenu.hpp"
#include "editpartsdata.hpp"
#include "frame.hpp"
#include "itemdata.hpp"
#include "mglib.hpp"
#include "userstatus.hpp"

namespace dc::game {

namespace {

/** The areas where items can be put down on map 1. */
ITEM_FREE_AREA ItemFreeAreaD01[27] = {
    {0, 1, 0, 1, {{-2.0f, 0.0f, -4.0f, 2.0f, 0.0f, 4.0f}}},
    {5, 1, 0, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {6, 1, 1, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {7, 1, 2, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {8, 1, 3, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {9, 1, 0, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {10, 1, 1, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {11, 1, 2, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {12, 1, 3, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {13, 1, 0, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {14, 1, 1, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {15, 1, 2, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {16, 1, 3, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {17, 1, 0, 0, {{-8.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {18, 2, 0, 0, {{-8.0f, 0.0f, 4.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {19, 2, 1, 0, {{-8.0f, 0.0f, 4.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {20, 2, 2, 0, {{-8.0f, 0.0f, 4.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {21, 2, 3, 0, {{-8.0f, 0.0f, 4.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {22, 2, 0, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {23, 2, 1, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {24, 2, 2, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {25, 2, 3, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {26, 2, 0, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {27, 2, 1, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {28, 2, 2, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {29, 2, 3, 0, {{-8.0f, 0.0f, -8.0f, -4.0f, 0.0f, 8.0f}, {4.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {-1, 0, 0, 0},
};

/** The areas where items can be put down on map 2. */
ITEM_FREE_AREA ItemFreeAreaD02[15] = {
    {0, 2, 0, 1, {{-0.5f, 0.0f, -2.0f, 0.5f, 0.0f, 5.0f}, {0.0f, 0.0f, -5.0f, 1.5f, 0.0f, 2.0f}}},
    {5, 1, 0, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {6, 1, 1, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {7, 1, 2, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {8, 1, 3, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {9, 1, 0, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {10, 1, 1, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {11, 1, 2, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {12, 1, 3, 0, {{-8.0f, 0.0f, 0.0f, 8.0f, 0.0f, 8.0f}}},
    {13, 1, 0, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {14, 1, 1, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {15, 1, 2, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {16, 1, 3, 0, {{3.0f, 0.0f, 3.0f, 8.0f, 0.0f, 8.0f}}},
    {17, 1, 0, 0, {{-6.0f, 0.0f, -6.0f, 6.0f, 0.0f, 6.0f}}},
    {-1, 0, 0, 0},
};

/** The areas where items can be put down on map 3. */
ITEM_FREE_AREA ItemFreeAreaD03[27] = {
    {0, 1, 0, 1, {{-1.0f, 1.0f, -4.0f, 0.0f, 1.0f, 4.0f}}},
    {5, 2, 0, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 7.0f}}},
    {6, 2, 1, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 7.0f}}},
    {7, 2, 2, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 7.0f}}},
    {8, 2, 3, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 7.0f}}},
    {9, 2, 0, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 7.0f}}},
    {10, 2, 1, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 7.0f}}},
    {11, 2, 2, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 7.0f}}},
    {12, 2, 3, 0, {{-8.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {-1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 7.0f}}},
    {13, 4, 0, 0, {{-1.0f, 0.0f, 3.0f, 1.0f, 0.0f, 8.0f}, {1.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {7.0f, 0.0f, 1.0f, 8.0f, 0.0f, 8.0f}, {3.0f, 0.0f, -1.0f, 8.0f, 0.0f, 1.0f}}},
    {14, 4, 1, 0, {{-1.0f, 0.0f, 3.0f, 1.0f, 0.0f, 8.0f}, {1.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {7.0f, 0.0f, 1.0f, 8.0f, 0.0f, 8.0f}, {3.0f, 0.0f, -1.0f, 8.0f, 0.0f, 1.0f}}},
    {15, 4, 2, 0, {{-1.0f, 0.0f, 3.0f, 1.0f, 0.0f, 8.0f}, {1.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {7.0f, 0.0f, 1.0f, 8.0f, 0.0f, 8.0f}, {3.0f, 0.0f, -1.0f, 8.0f, 0.0f, 1.0f}}},
    {16, 4, 3, 0, {{-1.0f, 0.0f, 3.0f, 1.0f, 0.0f, 8.0f}, {1.0f, 0.0f, 7.0f, 8.0f, 0.0f, 8.0f}, {7.0f, 0.0f, 1.0f, 8.0f, 0.0f, 8.0f}, {3.0f, 0.0f, -1.0f, 8.0f, 0.0f, 1.0f}}},
    {17, 1, 0, 0, {{-8.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {18, 2, 0, 0, {{-8.0f, 0.0f, -1.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -1.0f, 8.0f, 0.0f, 8.0f}}},
    {19, 2, 1, 0, {{-8.0f, 0.0f, -1.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -1.0f, 8.0f, 0.0f, 8.0f}}},
    {20, 2, 2, 0, {{-8.0f, 0.0f, -1.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -1.0f, 8.0f, 0.0f, 8.0f}}},
    {21, 2, 3, 0, {{-8.0f, 0.0f, -1.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -1.0f, 8.0f, 0.0f, 8.0f}}},
    {22, 2, 0, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {23, 2, 1, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {24, 2, 2, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {25, 2, 3, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {26, 2, 0, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {27, 2, 1, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {28, 2, 2, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {29, 2, 3, 0, {{-8.0f, 0.0f, -8.0f, -7.0f, 0.0f, 8.0f}, {7.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {-1, 0, 0, 0},
};

/** The areas where items can be put down on map 4. */
ITEM_FREE_AREA ItemFreeAreaD04[27] = {
    {0, 1, 0, 1, {{-1.0f, 0.0f, -4.0f, 1.0f, 0.0f, 4.0f}}},
    {5, 2, 0, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {6, 2, 1, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {7, 2, 2, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {8, 2, 3, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {9, 2, 0, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {10, 2, 1, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {11, 2, 2, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {12, 2, 3, 0, {{-8.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}, {-4.0f, 0.0f, -4.0f, 4.0f, 0.0f, -2.0f}}},
    {13, 2, 0, 0, {{-2.0f, 0.0f, -4.0f, 5.0f, 0.0f, 8.0f}, {-4.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {14, 2, 1, 0, {{-2.0f, 0.0f, -4.0f, 5.0f, 0.0f, 8.0f}, {-4.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {15, 2, 2, 0, {{-2.0f, 0.0f, -4.0f, 5.0f, 0.0f, 8.0f}, {-4.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {16, 2, 3, 0, {{-2.0f, 0.0f, -4.0f, 5.0f, 0.0f, 8.0f}, {-4.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {17, 1, 0, 0, {{-6.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {18, 2, 0, 0, {{-8.0f, 0.0f, -2.0f, -5.0f, 0.0f, 5.0f}, {5.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {19, 2, 1, 0, {{-8.0f, 0.0f, -2.0f, -5.0f, 0.0f, 5.0f}, {5.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {20, 2, 2, 0, {{-8.0f, 0.0f, -2.0f, -5.0f, 0.0f, 5.0f}, {5.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {21, 2, 3, 0, {{-8.0f, 0.0f, -2.0f, -5.0f, 0.0f, 5.0f}, {5.0f, 0.0f, -2.0f, 8.0f, 0.0f, 5.0f}}},
    {22, 2, 0, 0, {{-6.0f, 0.4f, -6.0f, -3.0f, 0.4f, 6.0f}, {3.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {23, 2, 1, 0, {{-6.0f, 0.4f, -6.0f, -3.0f, 0.4f, 6.0f}, {3.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {24, 2, 2, 0, {{-6.0f, 0.4f, -6.0f, -3.0f, 0.4f, 6.0f}, {3.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {25, 2, 3, 0, {{-6.0f, 0.4f, -6.0f, -3.0f, 0.4f, 6.0f}, {3.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {26, 2, 0, 0, {{-6.0f, 0.4f, -6.0f, -5.0f, 0.4f, 6.0f}, {5.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {27, 2, 1, 0, {{-6.0f, 0.4f, -6.0f, -5.0f, 0.4f, 6.0f}, {5.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {28, 2, 2, 0, {{-6.0f, 0.4f, -6.0f, -5.0f, 0.4f, 6.0f}, {5.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {29, 2, 3, 0, {{-6.0f, 0.4f, -6.0f, -5.0f, 0.4f, 6.0f}, {5.0f, 0.4f, -6.0f, 6.0f, 0.4f, 6.0f}}},
    {-1, 0, 0, 0},
};

/** The areas where items can be put down on map 5. */
ITEM_FREE_AREA ItemFreeAreaD05[27] = {
    {0, 1, 0, 1, {{-0.5f, 0.0f, -4.0f, 0.5f, 0.0f, 4.0f}}},
    {5, 1, 0, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {6, 1, 1, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {7, 1, 2, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {8, 1, 3, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {9, 1, 0, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {10, 1, 1, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {11, 1, 2, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {12, 1, 3, 0, {{-8.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {13, 1, 0, 0, {{6.0f, 0.0f, 6.0f, 8.0f, 0.0f, 8.0f}}},
    {14, 1, 1, 0, {{6.0f, 0.0f, 6.0f, 8.0f, 0.0f, 8.0f}}},
    {15, 1, 2, 0, {{6.0f, 0.0f, 6.0f, 8.0f, 0.0f, 8.0f}}},
    {16, 1, 3, 0, {{6.0f, 0.0f, 6.0f, 8.0f, 0.0f, 8.0f}}},
    {17, 1, 0, 0, {{-8.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {18, 2, 0, 0, {{-8.0f, 0.0f, 4.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {19, 2, 1, 0, {{-8.0f, 0.0f, 4.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {20, 2, 2, 0, {{-8.0f, 0.0f, 4.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {21, 2, 3, 0, {{-8.0f, 0.0f, 4.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, 4.0f, 8.0f, 0.0f, 8.0f}}},
    {22, 2, 0, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {23, 2, 1, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {24, 2, 2, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {25, 2, 3, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {26, 2, 0, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {27, 2, 1, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {28, 2, 2, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {29, 2, 3, 0, {{-8.0f, 0.0f, -8.0f, -5.0f, 0.0f, 8.0f}, {5.0f, 0.0f, -8.0f, 8.0f, 0.0f, 8.0f}}},
    {-1, 0, 0, 0},
};

/** The areas where items can be put down on map 6. */
ITEM_FREE_AREA ItemFreeAreaD06[15] = {
    {0, 1, 0, 1, {{-1.5f, 0.0f, -5.0f, 1.5f, 0.0f, 5.0f}}},
    {5, 1, 0, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {6, 1, 1, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {7, 1, 2, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {8, 1, 3, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {9, 1, 0, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {10, 1, 1, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {11, 1, 2, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {12, 1, 3, 0, {{-5.0f, 0.0f, -1.5f, 5.0f, 0.0f, 5.0f}}},
    {17, 1, 0, 0, {{-2.0f, -1.0f, -2.0f, 2.0f, -1.0f, 2.0f}}},
    {18, 1, 0, 0, {{-5.0f, 0.0f, -1.0f, 5.0f, 0.0f, 5.0f}}},
    {19, 1, 1, 0, {{-5.0f, 0.0f, -1.0f, 5.0f, 0.0f, 5.0f}}},
    {20, 1, 2, 0, {{-5.0f, 0.0f, -1.0f, 5.0f, 0.0f, 5.0f}}},
    {21, 1, 3, 0, {{-5.0f, 0.0f, -1.0f, 5.0f, 0.0f, 5.0f}}},
    {-1, 0, 0, 0},
};

/** The areas where items can be put down on map 7. */
ITEM_FREE_AREA ItemFreeAreaD07[28] = {
    {0, 1, 0, 1, {{-1.5f, 0.0f, -3.5f, 1.5f, 0.0f, 3.5f}}},
    {3, 1, 0, 1, {{-1.5f, 0.0f, -1.5f, 1.5f, 0.0f, 1.5f}}},
    {5, 1, 0, 0, {{-7.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {6, 1, 1, 0, {{-7.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {7, 1, 2, 0, {{-7.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {8, 1, 3, 0, {{-7.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {9, 1, 0, 0, {{-7.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {10, 1, 1, 0, {{-7.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {11, 1, 2, 0, {{-7.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {12, 1, 3, 0, {{-7.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {13, 2, 0, 0, {{-1.0f, 0.0f, -1.0f, 3.0f, 0.0f, 7.0f}, {-1.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {14, 2, 1, 0, {{-1.0f, 0.0f, -1.0f, 3.0f, 0.0f, 7.0f}, {-1.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {15, 2, 2, 0, {{-1.0f, 0.0f, -1.0f, 3.0f, 0.0f, 7.0f}, {-1.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {16, 2, 3, 0, {{-1.0f, 0.0f, -1.0f, 3.0f, 0.0f, 7.0f}, {-1.0f, 0.0f, -1.0f, 7.0f, 0.0f, 3.0f}}},
    {17, 1, 0, 0, {{-7.0f, 0.5f, -7.0f, 7.0f, 0.5f, 7.0f}}},
    {18, 2, 0, 0, {{-7.0f, 0.0f, -1.5f, -4.0f, 0.0f, 3.0f}, {4.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {19, 2, 1, 0, {{-7.0f, 0.0f, -1.5f, -4.0f, 0.0f, 3.0f}, {4.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {20, 2, 2, 0, {{-7.0f, 0.0f, -1.5f, -4.0f, 0.0f, 3.0f}, {4.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {21, 2, 3, 0, {{-7.0f, 0.0f, -1.5f, -4.0f, 0.0f, 3.0f}, {4.0f, 0.0f, -1.5f, 7.0f, 0.0f, 3.0f}}},
    {22, 2, 0, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {23, 2, 1, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {24, 2, 2, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {25, 2, 3, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {26, 2, 0, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {27, 2, 1, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {28, 2, 2, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {29, 2, 3, 0, {{-6.0f, 0.5f, -6.0f, -3.0f, 0.5f, 6.0f}, {3.0f, 0.5f, -6.0f, 6.0f, 0.5f, 6.0f}}},
    {-1, 0, 0, 0},
};

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 0. */
s16 ItemSetRateList0[385] = {
    50, 50, 50, 50, 95, 60, 65, 70, 60, 95, 100, 100, 100, 60, 98, 98, 98, 98, 98, 98,
    50, 50, 50, 60, 65, 50, 80, 95, 80, 95, 95, 50, 50, 50, 50, 50, 90, 60, 65, 70,
    75, 80, 95, 95, 50, 50, 50, 50, 50, 60, 80, 80, 80, 85, 95, 95, 50, 50, 50, 50,
    50, 60, 70, 80, 95, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 80, 90, 80, 80, 80,
    80, 85, 80, 80, 85, 80, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    95, 60, 65, 70, 60, 95, 100, 100, 100, 60, 98, 98, 98, 98, 98, 98, 60, 100, 100, 100,
    95, 60, 65, 70, 60, 50, 50, 50, 50, 60, 60, 60, 60, 60, 98, 98, 98, 60, 50, 50,
    50, 60, 65, 50, 80, 95, 80, 95, 95, 80, 95, 95, 95, 50, 50, 50, 50, 90, 60, 65,
    70, 90, 95, 95, 95, 50, 80, 95, 95, 65, 50, 50, 50, 50, 60, 80, 80, 80, 85, 95,
    95, 50, 80, 95, 95, 80, 50, 50, 50, 50, 60, 70, 80, 80, 95, 95, 95, 50, 60, 70,
    50, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95, 80, 95, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 1. */
s16 ItemSetRateList1[385] = {
    50, 60, 60, 50, 95, 60, 65, 70, 60, 95, 100, 100, 100, 60, 98, 98, 98, 98, 98, 98,
    50, 60, 60, 60, 65, 60, 80, 95, 80, 95, 95, 50, 50, 50, 50, 50, 90, 60, 65, 70,
    75, 80, 95, 95, 50, 50, 50, 50, 50, 60, 80, 80, 80, 85, 95, 95, 50, 50, 50, 50,
    50, 60, 70, 80, 95, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 60, 60, 50,
    95, 60, 65, 70, 60, 95, 100, 100, 100, 60, 98, 98, 98, 98, 98, 98, 60, 100, 100, 100,
    95, 60, 65, 70, 60, 50, 60, 50, 50, 60, 60, 60, 60, 60, 98, 98, 98, 60, 50, 60,
    60, 60, 65, 60, 80, 95, 80, 95, 95, 80, 95, 95, 95, 50, 50, 50, 50, 90, 60, 65,
    70, 90, 95, 95, 95, 50, 80, 95, 95, 65, 50, 50, 50, 50, 60, 80, 80, 80, 85, 95,
    95, 50, 80, 95, 95, 80, 50, 50, 50, 50, 60, 70, 80, 80, 95, 95, 95, 50, 60, 70,
    50, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95, 80, 95, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 2. */
s16 ItemSetRateList2[385] = {
    50, 65, 65, 50, 95, 50, 65, 70, 50, 95, 100, 100, 100, 65, 98, 98, 98, 98, 98, 98,
    50, 65, 65, 60, 65, 65, 80, 95, 50, 95, 95, 50, 50, 60, 60, 50, 80, 50, 50, 70,
    75, 80, 95, 95, 50, 50, 50, 50, 50, 60, 80, 80, 80, 85, 95, 95, 50, 50, 50, 50,
    50, 60, 70, 80, 95, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 65, 65, 50,
    95, 50, 65, 70, 50, 95, 100, 100, 100, 65, 98, 98, 98, 98, 98, 98, 65, 100, 100, 100,
    95, 50, 65, 70, 50, 50, 65, 50, 50, 50, 50, 50, 50, 50, 98, 98, 98, 50, 50, 65,
    65, 60, 65, 65, 80, 95, 50, 95, 95, 80, 95, 95, 95, 50, 60, 60, 50, 80, 50, 50,
    70, 90, 95, 95, 95, 50, 80, 95, 95, 50, 50, 50, 50, 50, 60, 80, 80, 80, 85, 95,
    95, 50, 80, 95, 95, 80, 50, 50, 50, 50, 60, 70, 80, 80, 95, 95, 95, 50, 60, 70,
    50, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95, 80, 95, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 3. */
s16 ItemSetRateList3[385] = {
    50, 70, 70, 50, 95, 50, 65, 70, 50, 95, 100, 100, 100, 70, 98, 98, 98, 98, 98, 98,
    50, 70, 70, 60, 65, 70, 80, 90, 50, 95, 95, 50, 50, 65, 65, 50, 50, 50, 50, 70,
    75, 80, 95, 95, 50, 50, 65, 65, 65, 65, 80, 80, 80, 85, 95, 95, 50, 50, 65, 65,
    65, 60, 70, 80, 95, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 75, 75, 75, 75, 75, 75,
    75, 75, 75, 75, 75, 75, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 70, 70, 50,
    95, 50, 65, 70, 50, 95, 100, 100, 100, 70, 98, 98, 98, 98, 98, 98, 70, 100, 100, 100,
    95, 50, 65, 70, 50, 50, 70, 50, 50, 50, 50, 50, 50, 50, 98, 98, 98, 50, 50, 70,
    70, 60, 65, 70, 80, 90, 50, 95, 95, 80, 90, 90, 90, 50, 65, 65, 50, 50, 50, 50,
    70, 90, 95, 95, 95, 50, 80, 90, 95, 50, 50, 65, 65, 65, 65, 80, 80, 80, 85, 95,
    95, 50, 80, 90, 95, 80, 50, 65, 65, 65, 60, 70, 80, 80, 95, 95, 95, 50, 60, 70,
    65, 65, 50, 50, 85, 50, 50, 50, 85, 85, 90, 95, 95, 80, 90, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 4. */
s16 ItemSetRateList4[385] = {
    50, 75, 75, 50, 95, 50, 65, 70, 50, 95, 100, 100, 100, 75, 98, 98, 98, 98, 98, 98,
    50, 75, 75, 60, 65, 75, 80, 90, 50, 95, 95, 50, 50, 70, 70, 50, 50, 50, 50, 70,
    75, 80, 95, 95, 50, 50, 70, 70, 70, 70, 75, 75, 80, 85, 95, 95, 50, 50, 70, 70,
    70, 60, 70, 80, 95, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 70, 70, 70, 70, 70, 70,
    70, 70, 70, 70, 70, 70, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 75, 75, 50,
    95, 50, 65, 70, 50, 95, 100, 100, 100, 75, 98, 98, 98, 98, 98, 98, 75, 100, 100, 100,
    95, 50, 65, 70, 50, 50, 75, 50, 50, 50, 50, 50, 50, 50, 98, 98, 98, 50, 50, 75,
    75, 60, 65, 75, 80, 90, 50, 95, 95, 80, 90, 90, 90, 50, 70, 70, 50, 50, 50, 50,
    70, 90, 95, 95, 95, 50, 80, 90, 95, 50, 50, 70, 70, 70, 70, 75, 75, 80, 85, 95,
    95, 50, 80, 90, 95, 75, 50, 70, 70, 70, 60, 70, 80, 80, 90, 95, 95, 50, 60, 70,
    70, 70, 50, 50, 85, 50, 50, 50, 85, 85, 90, 95, 95, 80, 90, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 5. */
s16 ItemSetRateList5[385] = {
    50, 80, 80, 50, 80, 50, 65, 70, 50, 95, 100, 100, 100, 80, 98, 95, 98, 98, 98, 98,
    50, 80, 80, 60, 65, 80, 80, 85, 50, 95, 95, 50, 50, 75, 75, 50, 50, 50, 50, 70,
    75, 80, 95, 95, 50, 50, 75, 75, 75, 75, 75, 75, 80, 85, 95, 95, 50, 50, 75, 75,
    75, 60, 70, 80, 85, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 70, 70, 70, 70, 70, 70,
    70, 70, 70, 70, 70, 70, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 80, 95, 95,
    80, 95, 65, 95, 95, 95, 100, 100, 100, 95, 98, 95, 98, 98, 98, 98, 80, 100, 100, 100,
    80, 50, 95, 70, 50, 95, 95, 50, 50, 50, 95, 50, 95, 50, 98, 98, 98, 50, 50, 80,
    80, 95, 65, 95, 95, 95, 50, 95, 95, 80, 85, 85, 85, 50, 75, 95, 50, 50, 95, 95,
    95, 85, 90, 95, 95, 50, 80, 85, 85, 50, 50, 75, 75, 75, 75, 75, 75, 80, 85, 95,
    95, 50, 80, 85, 95, 75, 50, 75, 75, 75, 60, 70, 80, 80, 90, 95, 95, 50, 60, 70,
    75, 75, 50, 50, 85, 50, 50, 50, 85, 85, 90, 95, 95, 80, 85, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** Chance out of a hundred that each item is turned down when drawn in dungeon 6. */
s16 ItemSetRateList6[385] = {
    50, 80, 80, 50, 80, 50, 65, 70, 50, 95, 100, 100, 100, 80, 98, 95, 98, 98, 98, 98,
    50, 80, 80, 60, 65, 80, 80, 85, 50, 95, 95, 50, 50, 75, 75, 50, 50, 50, 50, 70,
    75, 80, 95, 95, 50, 50, 75, 75, 75, 75, 75, 75, 80, 85, 95, 95, 50, 50, 75, 75,
    75, 60, 70, 80, 85, 90, 95, 95, 50, 50, 50, 85, 50, 50, 50, 85, 85, 85, 95, 95,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 70, 70, 70, 70, 70, 70,
    70, 70, 70, 70, 70, 70, 90, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 90, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 80, 95, 95,
    80, 95, 65, 95, 95, 95, 100, 100, 100, 95, 98, 95, 98, 98, 98, 98, 80, 100, 100, 100,
    80, 50, 95, 70, 50, 95, 95, 50, 50, 50, 95, 50, 95, 50, 98, 98, 98, 50, 50, 80,
    80, 95, 65, 95, 95, 95, 50, 95, 95, 80, 85, 85, 85, 50, 75, 95, 50, 50, 95, 95,
    95, 85, 90, 95, 95, 50, 80, 85, 85, 50, 50, 75, 75, 75, 75, 75, 75, 80, 85, 95,
    95, 50, 80, 85, 95, 75, 50, 75, 75, 75, 60, 70, 80, 80, 90, 95, 95, 50, 60, 70,
    75, 75, 50, 50, 85, 50, 50, 50, 85, 85, 90, 95, 95, 80, 85, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 0,
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 0. */
ITEM_PUT_SET ItemPutListTbl0[4] = {
    {256, 63, {
        ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BREAD, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_BOMB,
        ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_TRAM_OIL, ITEM_PRICKLY, ITEM_MIMI,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_REPAIR_POWDER,
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_STAND_IN_POWDER, ITEM_TRAM_OIL, ITEM_CHEESE, ITEM_ESCAPE_POWDER, ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS,
        ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_STAND_IN_POWDER, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_STONE_BREAKER, ITEM_TRAM_OIL, ITEM_ANTIDOTE_AMULET, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM,
        ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA,
        -1,
    }},
    {255, 63, {
        ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BREAD, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_BOMB,
        ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_TRAM_OIL, ITEM_PRICKLY, ITEM_MIMI,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_REPAIR_POWDER,
        ITEM_REPAIR_POWDER, ITEM_TRAM_OIL, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_CHEESE, ITEM_ESCAPE_POWDER, ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS,
        ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_TRAM_OIL, ITEM_ANTIDOTE_AMULET, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM,
        ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA,
        -1,
    }},
    {11, 75, {
        ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BREAD, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_BOMB,
        ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_PRICKLY, ITEM_MIMI,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_REPAIR_POWDER,
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_CHEESE, ITEM_ESCAPE_POWDER, ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS,
        ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_TRAM_OIL, ITEM_ANTIDOTE_AMULET, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM,
        ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_BOMB, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM,
        ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_BOMB,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 1. */
ITEM_PUT_SET ItemPutListTbl1[3] = {
    {256, 76, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_BREAD,
        ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_PRICKLY, ITEM_MIMI,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM,
        ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE,
        ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_SHAMSHIR,
        ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_STONE, ITEM_STONE, ITEM_SUN_DEW, ITEM_SUN_DEW, ITEM_STAMINA_DRINK, ITEM_POWERUP_POWDER, ITEM_SUN_DEW,
        ITEM_SUN_DEW, ITEM_SUN_DEW, ITEM_SUN_DEW, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_BREAD,
        -1,
    }},
    {255, 76, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_BREAD,
        ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_PRICKLY, ITEM_MIMI,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM,
        ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE,
        ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_SHAMSHIR,
        ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_STAMINA_DRINK, ITEM_POWERUP_POWDER, ITEM_SUN_DEW,
        ITEM_SUN_DEW, ITEM_SUN_DEW, ITEM_SUN_DEW, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_BREAD,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 2. */
ITEM_PUT_SET ItemPutListTbl2[3] = {
    {256, 78, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_POWERUP_POWDER, ITEM_TREASURE_KEY, ITEM_DRAN_S_FEATHER,
        ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK,
        ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
        ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING, ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
        ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_FLAPPING_FISH, ITEM_FLAPPING_FISH, ITEM_ICE_BLOCK, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_KITCHEN_KNIFE,
        ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER,
        ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD,
        -1,
    }},
    {255, 83, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_POWERUP_POWDER, ITEM_TREASURE_KEY, ITEM_DRAN_S_FEATHER,
        ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK,
        ITEM_ANTIDOTE_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
        ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING, ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
        ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_FLAPPING_FISH, ITEM_FLAPPING_FISH, ITEM_ICE_BLOCK, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_GLADIUS,
        ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT,
        ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 3. */
ITEM_PUT_SET ItemPutListTbl3[3] = {
    {256, 83, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_SECRET_PATH_KEY, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK,
        ITEM_ANTIDOTE_DRINK, ITEM_SECRET_PATH_KEY, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
        ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING, ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
        ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_SECRET_PATH_KEY, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD,
        ITEM_WEAPON_GLADIUS, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_CHOORA,
        ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_THORN_ARMLET,
        ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING,
        -1,
    }},
    {255, 87, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_SECRET_PATH_KEY, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK,
        ITEM_ANTIDOTE_DRINK, ITEM_SECRET_PATH_KEY, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
        ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING, ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
        ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_SECRET_PATH_KEY, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD,
        ITEM_WEAPON_GLADIUS, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_CHOORA,
        ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_THORN_ARMLET,
        ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 4. */
ITEM_PUT_SET ItemPutListTbl4[3] = {
    {256, 91, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_BRAVERY_LAUNCH, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK,
        ITEM_ANTIDOTE_DRINK, ITEM_BRAVERY_LAUNCH, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
        ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING, ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
        ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_BRAVERY_LAUNCH, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD,
        ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_BONE_SLINGSHOT,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_MAGICAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS,
        ITEM_WEAPON_SCORPION, ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD,
        -1,
    }},
    {255, 95, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_BRAVERY_LAUNCH, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK,
        ITEM_ANTIDOTE_DRINK, ITEM_BRAVERY_LAUNCH, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
        ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING, ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
        ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK,
        ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY,
        ITEM_BRAVERY_LAUNCH, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD,
        ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_BONE_SLINGSHOT,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_MAGICAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS,
        ITEM_WEAPON_SCORPION, ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 5. */
ITEM_PUT_SET ItemPutListTbl5[3] = {
    {256, 94, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_FLAPPING_DUSTER,
        ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING,
        ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH,
        ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED,
        ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_FLAPPING_DUSTER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_7BRANCH_SWORD, ITEM_WEAPON_STEEL_SLINGSHOT,
        ITEM_FLAPPING_DUSTER, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_MANEATER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_MAGICAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING,
        ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS, ITEM_WEAPON_SCORPION,
        ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK, ITEM_WEAPON_G_CRUSHER,
        ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER, ITEM_WEAPON_AGA_S_SWORD,
        -1,
    }},
    {255, 94, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_FLAPPING_DUSTER,
        ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING,
        ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH,
        ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED,
        ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_FLAPPING_DUSTER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_7BRANCH_SWORD, ITEM_WEAPON_STEEL_SLINGSHOT,
        ITEM_FLAPPING_DUSTER, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_MANEATER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_MAGICAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING,
        ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS, ITEM_WEAPON_SCORPION,
        ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK, ITEM_WEAPON_G_CRUSHER,
        ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER, ITEM_WEAPON_AGA_S_SWORD,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 6. */
ITEM_PUT_SET ItemPutListTbl6[3] = {
    {256, 93, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_CRYSTAL_EYEBALL, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK, ITEM_CRYSTAL_EYEBALL, ITEM_ANTIDOTE_DRINK,
        ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING,
        ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH,
        ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_CRYSTAL_EYEBALL, ITEM_CRYSTAL_EYEBALL, ITEM_CRYSTAL_EYEBALL,
        ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_ATTACH_ATTACK, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_7BRANCH_SWORD, ITEM_WEAPON_STEEL_SLINGSHOT,
        ITEM_WEAPON_MANEATER, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_AGA_S_SWORD, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_MAGICAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING,
        ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS, ITEM_WEAPON_SCORPION,
        ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK, ITEM_WEAPON_G_CRUSHER,
        ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER,
        -1,
    }},
    {255, 93, {
        ITEM_REPAIR_POWDER, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_TREASURE_KEY, ITEM_POWERUP_POWDER, ITEM_DRAN_S_FEATHER,
        ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BOMB, ITEM_STAMINA_DRINK, ITEM_CRYSTAL_EYEBALL, ITEM_ANTIDOTE_DRINK,
        ITEM_HOLY_WATER, ITEM_SOAP, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_MIGHTY_HEALING,
        ITEM_PRICKLY, ITEM_MIMI, ITEM_EVY, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH,
        ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_CRYSTAL_EYEBALL, ITEM_CRYSTAL_EYEBALL, ITEM_CRYSTAL_EYEBALL,
        ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_ATTACH_ATTACK, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_7BRANCH_SWORD, ITEM_WEAPON_STEEL_SLINGSHOT,
        ITEM_WEAPON_MANEATER, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_AGA_S_SWORD, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_MAGICAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING, ITEM_WEAPON_FAIRY_S_RING,
        ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS, ITEM_WEAPON_SCORPION,
        ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK, ITEM_WEAPON_G_CRUSHER,
        ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 0's back floors. */
ITEM_PUT_SET ItemPutListTbl7[3] = {
    {256, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {255, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 1's back floors. */
ITEM_PUT_SET ItemPutListTbl8[3] = {
    {256, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {255, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 2's back floors. */
ITEM_PUT_SET ItemPutListTbl9[3] = {
    {256, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {255, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 3's back floors. */
ITEM_PUT_SET ItemPutListTbl10[3] = {
    {256, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {255, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

// clang-format off
/** The treasure box item lists of dungeon 4's back floors. */
ITEM_PUT_SET ItemPutListTbl11[3] = {
    {256, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {255, 52, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP,
        ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH,
        ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_POWERUP_POWDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

} // namespace

// clang-format off
/** The treasure box item lists of dungeon 5's back floors. */
ITEM_PUT_SET ItemPutListTbl12[3] = {
    {256, 97, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_SHAMSHIR,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING,
        ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS,
        ITEM_WEAPON_SCORPION, ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER, ITEM_WEAPON_AGA_S_SWORD, ITEM_WEAPON_MANEATER,
        ITEM_WEAPON_7BRANCH_SWORD, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET,
        ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA,
        ITEM_POWERUP_POWDER,
        -1,
    }},
    {255, 97, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_SHAMSHIR,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_DOUBLE_IMPACT, ITEM_WEAPON_FROZEN_TUNA, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING,
        ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS,
        ITEM_WEAPON_SCORPION, ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER, ITEM_WEAPON_AGA_S_SWORD, ITEM_WEAPON_MANEATER,
        ITEM_WEAPON_7BRANCH_SWORD, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_MIGHTY_HEALING, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET,
        ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_DRAN_S_FEATHER, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA,
        ITEM_POWERUP_POWDER,
        -1,
    }},
    {-1, -1},
};
// clang-format on

namespace {

// clang-format off
/** The treasure box item lists of dungeon 6's back floors. */
ITEM_PUT_SET ItemPutListTbl13[3] = {
    {256, 73, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_POWERUP_POWDER, ITEM_WEAPON_STEVE, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING,
        ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS,
        ITEM_WEAPON_SCORPION, ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER, ITEM_WEAPON_AGA_S_SWORD, ITEM_WEAPON_MANEATER,
        ITEM_WEAPON_7BRANCH_SWORD,
        -1,
    }},
    {255, 73, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND,
        ITEM_ATTACH_HOLY, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY,
        ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER,
        ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_METALBREAKER, ITEM_ATTACH_MIMIC_BREAKER, ITEM_ATTACH_MAGE_SLAYER, ITEM_FIRE_GEM,
        ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_WEAPON_ANTIQUE_SWORD, ITEM_POWERUP_POWDER, ITEM_WEAPON_STEVE, ITEM_WEAPON_TURTLE_SHELL,
        ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING, ITEM_WEAPON_GODDESS_RING,
        ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_DESTRUCTION_RING, ITEM_WEAPON_SATAN_S_RING, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL, ITEM_WEAPON_CACTUS,
        ITEM_WEAPON_SCORPION, ITEM_WEAPON_PARTISAN, ITEM_WEAPON_TERRA_SWORD, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER, ITEM_WEAPON_BRAVE_ARK, ITEM_WEAPON_LAMB_S_SWORD, ITEM_WEAPON_DRAIN_SEEKER, ITEM_WEAPON_CROSS_HINDER, ITEM_WEAPON_AGA_S_SWORD, ITEM_WEAPON_MANEATER,
        ITEM_WEAPON_7BRANCH_SWORD,
        -1,
    }},
    {-1, -1},
};
// clang-format on

} // namespace

/** The treasure box item lists, seven dungeons and then their back floors. */
ITEM_PUT_SET *ItemPutListPtr[14] = {
    ItemPutListTbl0,
    ItemPutListTbl1,
    ItemPutListTbl2,
    ItemPutListTbl3,
    ItemPutListTbl4,
    ItemPutListTbl5,
    ItemPutListTbl6,
    ItemPutListTbl7,
    ItemPutListTbl8,
    ItemPutListTbl9,
    ItemPutListTbl10,
    ItemPutListTbl11,
    ItemPutListTbl12,
    ItemPutListTbl13,
};

namespace {

// clang-format off
/** The clown's item lists of dungeon 0. */
PIERO_ITEM_SET PieroItemList0[2] = {
    {{19, 24}, {{
        ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_TRAM_OIL,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
    {{19, 28}, {{
        ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_TRAM_OIL,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 1. */
PIERO_ITEM_SET PieroItemList1[2] = {
    {{19, 28}, {{
        ITEM_WEAPON_BASELARD, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }}},
    {{25, 28}, {{
        ITEM_WEAPON_BASELARD, ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX,
        ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET,
        ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 2. */
PIERO_ITEM_SET PieroItemList2[2] = {
    {{27, 29}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1,
    }}},
    {{27, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 3. */
PIERO_ITEM_SET PieroItemList3[2] = {
    {{28, 29}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1,
    }}},
    {{32, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 4. */
PIERO_ITEM_SET PieroItemList4[2] = {
    {{33, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
    {{33, 41}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 5. */
PIERO_ITEM_SET PieroItemList5[2] = {
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 6. */
PIERO_ITEM_SET PieroItemList6[2] = {
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 0's back floors. */
PIERO_ITEM_SET PieroItemList7[2] = {
    {{19, 24}, {{
        ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_TRAM_OIL,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
    {{19, 28}, {{
        ITEM_WEAPON_BASELARD, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_TRAM_OIL,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 1's back floors. */
PIERO_ITEM_SET PieroItemList8[2] = {
    {{20, 28}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_BASELARD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX,
        ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }}},
    {{25, 28}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_BASELARD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX,
        ITEM_WEAPON_SMALL_SWORD, ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET,
        ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 2's back floors. */
PIERO_ITEM_SET PieroItemList9[2] = {
    {{27, 29}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1,
    }}},
    {{27, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 3's back floors. */
PIERO_ITEM_SET PieroItemList10[2] = {
    {{31, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
    {{31, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER, ITEM_WEAPON_BATTLE_AX,
        ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_ANTI_FREEZE_AMULET,
        ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 4's back floors. */
PIERO_ITEM_SET PieroItemList11[2] = {
    {{33, 35}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }}},
    {{33, 41}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 5's back floors. */
PIERO_ITEM_SET PieroItemList12[2] = {
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

// clang-format off
/** The clown's item lists of dungeon 6's back floors. */
PIERO_ITEM_SET PieroItemList13[2] = {
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
    {{33, 42}, {{
        ITEM_WEAPON_BUSTER_SWORD, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_WEAPON_CRYSKNIFE, ITEM_WEAPON_SHAMSHIR, ITEM_WEAPON_TSUKIKAGE, ITEM_WEAPON_BONE_RAPIER, ITEM_WEAPON_SAX, ITEM_WEAPON_SMALL_SWORD,
        ITEM_WEAPON_CHOPPER, ITEM_WEAPON_SAND_BREAKER, ITEM_WEAPON_CHOORA, ITEM_WEAPON_STEEL_HAMMER, ITEM_WEAPON_MAGICAL_HAMMER, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_TURTLE_SHELL, ITEM_WEAPON_TRIAL_HAMMER,
        ITEM_WEAPON_BATTLE_AX, ITEM_WEAPON_GAIA_HAMMER, ITEM_WEAPON_LAST_JUDGEMENT, ITEM_BREAD, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER,
        ITEM_ANTI_FREEZE_AMULET, ITEM_ANTICURSEAMULET, ITEM_ANTIGOO_AMULET, ITEM_ANTIDOTE_AMULET, ITEM_WEAPON_JAVELIN, ITEM_WEAPON_HALBERT, ITEM_WEAPON_DE_SANGA, ITEM_WEAPON_5_FOOT_NAIL,
        ITEM_WEAPON_CACTUS,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    }, {
        ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND,
        ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE,
        ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, ITEM_MIMI, ITEM_PRICKLY, ITEM_SUN_DEW,
        ITEM_WEAPON_STEEL_SLINGSHOT, ITEM_WEAPON_BANDIT_SLINGSHOT, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_STEVE, ITEM_WEAPON_HARDSHOOTER, ITEM_WEAPON_BANDIT_S_RING, ITEM_WEAPON_CRYSTAL_RING, ITEM_WEAPON_PLATINUM_RING,
        ITEM_WEAPON_THORN_ARMLET, ITEM_WEAPON_POCKLEKUL, ITEM_WEAPON_FAIRY_S_RING, ITEM_WEAPON_JACKAL, ITEM_WEAPON_BLESSING_GUN, ITEM_WEAPON_SNAIL, ITEM_WEAPON_SWALLOW, ITEM_WEAPON_SKUNK,
        ITEM_WEAPON_G_CRUSHER, ITEM_WEAPON_HEXA_BLASTER,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
    }}},
};
// clang-format on

} // namespace

/** The clown's item lists, seven dungeons and then their back floors. */
PIERO_ITEM_SET *PieroItemListPtr[14] = {
    PieroItemList0,
    PieroItemList1,
    PieroItemList2,
    PieroItemList3,
    PieroItemList4,
    PieroItemList5,
    PieroItemList6,
    PieroItemList7,
    PieroItemList8,
    PieroItemList9,
    PieroItemList10,
    PieroItemList11,
    PieroItemList12,
    PieroItemList13,
};

/** Chance out of a hundred that each item is turned down when drawn, one table per dungeon. */
s16 *ItemSetRateTbl[7] = {
    ItemSetRateList0,
    ItemSetRateList1,
    ItemSetRateList2,
    ItemSetRateList3,
    ItemSetRateList4,
    ItemSetRateList5,
    ItemSetRateList6,
};

/** The areas where items can be put down on each map. */
ITEM_FREE_AREA *ItemFreeAreaAll[7] = {
    ItemFreeAreaD01,
    ItemFreeAreaD02,
    ItemFreeAreaD03,
    ItemFreeAreaD04,
    ItemFreeAreaD05,
    ItemFreeAreaD06,
    ItemFreeAreaD07,
};

/** The floor that divides each dungeon's lower item lists from its upper ones. */
int floorNum[7] = {8, 9, 9, 9, 8, 12, 50};

char info_cfg_literal[] = "info.cfg";

namespace {

/** The floors dungeon 0 keeps Atla off, ended by -1. */
constexpr int noEntryTbl00[] = {4, 8, 11, -1};

/** The floors dungeon 1 keeps Atla off, ended by -1. */
constexpr int noEntryTbl01[] = {4, 9, 12, -1};

/** The floors dungeon 2 keeps Atla off, ended by -1. */
constexpr int noEntryTbl02[] = {5, 9, 12, -1};

/** The floors dungeon 3 keeps Atla off, ended by -1. */
constexpr int noEntryTbl03[] = {5, 9, 13, -1};

/** The floors dungeon 4 keeps Atla off, ended by -1. */
constexpr int noEntryTbl04[] = {4, 8, 11, -1};

/** The floors dungeon 5 keeps Atla off, ended by -1. */
constexpr int noEntryTbl05[] = {19, 20, 21, 22, 23, -1};

/** The floors each dungeon keeps Atla off, one list per dungeon. */
constexpr std::array<const int *, 6> noEntryTbl = {noEntryTbl00, noEntryTbl01, noEntryTbl02, noEntryTbl03, noEntryTbl04, noEntryTbl05};

/** The floor each dungeon's upper half ends on. */
constexpr std::array<int, 6> CenterFloorTbl = {8, 9, 9, 9, 8, 13};

/** The number of floors in each dungeon. */
constexpr std::array<int, 6> MaxFloorTbl = {15, 17, 18, 18, 15, 25};

/** Most placement boxes SearchiDoPutArea gathers. */
constexpr int kMaxPutQuads = 196;

/** Width of a dungeon floor's grid, in cells. */
constexpr int kGridWidth = 20;

/** Side of one grid cell in world units. */
constexpr float kCellSize = 160.0f;

/** The cell and its eight neighbours, as x and z offsets, in the order their collision is gathered. */
constexpr std::array<std::array<int, 2>, 9> kNeighbourOffsets{
    {{0, 0}, {-1, 0}, {0, -1}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}}
};

/**
 * Scales a random number onto 0 to range through float, as retail does.
 */
float ScaledRand(float range) {
    // Retail divides by the PS2's RAND_MAX of 2^31 - 1 whatever the host's RAND_MAX is.
    return (range * static_cast<float>(std::rand())) / 2.1474836e9f;
}

/**
 * Truncates ScaledRand to an index.
 */
int RandBelow(float range) {
    return static_cast<int>(ScaledRand(range));
}

CDngStatusData *DungeonStatus() {
    return static_cast<CDngStatusData *>(UserStatus);
}

/**
 * Draws one item from one of the clown's two lists, turning items down at the
 * dungeon's rates until one is kept.
 */
int PickPieroItem(const PIERO_ITEM_SET &set, int list_no, const s16 *rate) {
    const int count = set.count[list_no];
    int       pick = -1;

    while (pick == -1) {
        pick = RandBelow(static_cast<float>(count));

        if (pick >= count) {
            pick = 0;
        }

        const int chance = RandBelow(100.0f);

        if (rate[set.item[list_no][pick] - 1] >= chance) {
            pick = -1;
        }
    }

    return set.item[list_no][pick];
}

/**
 * Scales a coordinate from the item area table up to world scale.
 */
float ToWorldScale(float coordinate) {
    return coordinate * 10.0f;
}

/**
 * Reports whether an Atla may be placed on one floor.
 */
int chkAtraFloor(int dungeon, int floor) {
    if (dungeon >= DUNGEON_DEMON_SHAFT) {
        return 0;
    }

    const int *floors = noEntryTbl[dungeon];

    for (int i = 0; floors[i] != -1; i++) {
        if (floor == floors[i]) {
            return 0;
        }
    }

    return 1;
}

/**
 * Records that one Atla lies on one floor of a dungeon.
 */
void RegisterAtra(int dungeon, int floor, int atra_id) {
    DungeonStatus()->SetGetAtra(dungeon, floor, atra_id);
}

/**
 * Gives a map part's collision model, or null for an empty cell.
 */
CFrame *PartsCollision(CDungeonMap *map, int parts_no) {
    if (parts_no == -1) {
        return nullptr;
    }

    return map->parts[parts_no].collision;
}

/**
 * Gives the quarter turns a map part's collision model is given, or 0 for an empty cell.
 */
int PartsCollisionTurn(CDungeonMap *map, int parts_no) {
    if (parts_no == -1) {
        return 0;
    }

    return map->parts[parts_no].collision_turn;
}

/**
 * Converts quarter turns into the rotation a model is given, wrapping a turn
 * of 3 to -1 as retail does.
 */
float QuarterTurnAngle(float turn) {
    if (3.0f == turn) {
        turn = -1.0f;
    }

    return (PI * (-90.0f * turn)) / 180.0f;
}

} // namespace

void GetPieroItem(int map_no, int ura_dungeon, int *item0, int *item1) {
    const PIERO_ITEM_SET *list = PieroItemListPtr[map_no + ura_dungeon * 7];
    const s16            *rate = ItemSetRateTbl[map_no];
    const PIERO_ITEM_SET &set = UserStatus->cur_floor < floorNum[map_no] ? list[0] : list[1];

    const int first = PickPieroItem(set, 0, rate);
    const int second = PickPieroItem(set, 1, rate);
    *item0 = first;
    *item1 = second;
}

namespace {

constexpr int kRateCount = 385;
constexpr int kMaxListItems = 128;
constexpr int kMaxDrawTries = 0xFFFF;

// Only the first normal list has four rows.
constexpr int kItemPutListCounts[14] = {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};

/* Draws candidates until one passes its turn-down chance, or returns -1 once the tries run out. */
int DrawCandidate(const std::vector<int> &candidates, std::span<const s16> rates) {
    const int count = static_cast<int>(candidates.size());

    for (int tries = 0;;) {
        const float roll = (static_cast<float>(count) * static_cast<float>(rand())) / 2.1474836e9f;
        int         pick = static_cast<int>(roll);

        if (roll - static_cast<float>(pick) > 0.0f) {
            pick++;
        }

        if (pick < 0 || pick >= count) {
            pick = 0;
        }

        const int chance = static_cast<int>((100.0f * static_cast<float>(rand())) / 2.1474836e9f);
        const int item_no = candidates[pick];

        if (item_no <= 0 || item_no > kRateCount) {
            return -1;
        }
        if (rates[item_no - 1] < chance) {
            return item_no;
        }

        tries++;

        if (tries >= kMaxDrawTries) {
            return -1;
        }
    }
}

} // namespace

/* The chance out of a hundred that each item is turned down when a treasure box draws it comes
   from the dungeon's rate list, the list GetPieroItem also reads for the clown's boxes. */
int PresetSmallItemNo_Get(int map_no, int floor_no, int special, int small) {
    if (map_no < 0 || map_no >= 7 || special < 0 || special > 1) {
        return -1;
    }

    const ITEM_PUT_SET *list = ItemPutListPtr[map_no + special * 7];
    const int           list_count = kItemPutListCounts[map_no + special * 7];

    if (list == nullptr || ItemSetRateTbl[map_no] == nullptr) {
        return -1;
    }

    auto rates = std::make_unique<s16[]>(kRateCount);
    std::copy_n(ItemSetRateTbl[map_no], kRateCount, rates.get());

    static_assert(ITEM_WEAPON_START + 0x7B <= kRateCount + 1);

    // Weapons the player already holds come up less often.
    for (int item_no = ITEM_WEAPON_START; item_no < ITEM_WEAPON_START + 0x7B; item_no++) {
        const int held = GetNumHowManyItemsHave(item_no);
        s16      &rate = rates[item_no - 1];

        if (held > 0) {
            if (held == 1) {
                rate -= 10;
            }

            if (held >= 2) {
                rate -= 20;
            }

            rate = std::max<s16>(rate, 0);
        }
    }

    int wanted = -1;
    int scan = 0;

    for (; scan < list_count; scan++) {
        const int floor = list[scan].floor;

        if (floor == -1) {
            break;
        }

        if (floor_no + 1 == floor) {
            wanted = floor_no + 1;
        }
    }

    if (scan == list_count) {
        return -1;
    }

    if (wanted == -1) {
        wanted = floor_no < floorNum[map_no] ? 0x100 : 0xFF;
    }

    int list_no = 0;

    while (wanted != list[list_no].floor) {
        list_no++;

        if (list_no >= list_count) {
            std::println("err itembox list ");
            return -1;
        }
    }

    const ITEM_PUT_SET &set = list[list_no];

    if (set.item_count < 0 || set.item_count > kMaxListItems) {
        return -1;
    }

    std::vector<int> candidates;
    candidates.reserve(kMaxListItems);

    for (const int item_no : std::span(set.item, set.item_count)) {
        const bool is_attachment = item_no >= ITEM_ATTACH_START && item_no < ITEM_WEAPON_START;
        const bool is_weapon = item_no >= ITEM_WEAPON_START;

        if (small != 0 ? is_attachment : is_weapon) {
            candidates.push_back(item_no);
        }
    }

    if (candidates.empty()) {
        return -1;
    }

    return DrawCandidate(candidates, std::span<const s16>(rates.get(), kRateCount));
}

int SearchiDoPutArea(MAPPARTS *cells, int x, int y, int width, int height, float *pos) {
    using Quad = std::array<std::array<float, 3>, 4>;

    auto                  quads = std::make_unique<Quad[]>(kMaxPutQuads);
    std::array<float, 4>  px{};
    std::array<float, 4>  py{};
    std::array<float, 4>  pz{};
    int                   count = 0;
    const ITEM_FREE_AREA *areas = ItemFreeAreaAll[selectMapNo];

    for (int row = y; row < y + height; row++) {
        for (int col = x; col < x + width; col++) {
            const int parts_no = cells[(row * kGridWidth) + col].parts_no;
            const int direction = cells[(row * kGridWidth) + col].direction;

            for (int area_no = 0; areas[area_no].parts_no != -1 && count < kMaxPutQuads; area_no++) {
                const ITEM_FREE_AREA &area = areas[area_no];

                if (parts_no != area.parts_no) {
                    continue;
                }

                for (int rect_no = 0; rect_no < area.rect_num; rect_no++) {
                    const ITEM_FREE_RECT &rect = area.rect[rect_no];
                    int                   turn = area.direction;
                    turn += direction;

                    if (turn > 3) {
                        turn -= 4;
                    }

                    const float angle = (PI * (90.0f * static_cast<float>(4 - turn))) / 180.0f;
                    px[0] = ToWorldScale(rect.x0);
                    py[0] = ToWorldScale(rect.y0);
                    pz[0] = ToWorldScale(rect.z0);
                    px[3] = ToWorldScale(rect.x1);
                    py[3] = ToWorldScale(rect.y1);
                    pz[3] = ToWorldScale(rect.z1);
                    px[1] = px[3];
                    py[1] = py[0];
                    pz[1] = pz[0];
                    px[2] = px[0];
                    py[2] = py[0];
                    pz[2] = pz[3];

                    Quad &quad = quads[count];

                    for (int corner = 0; corner < 4; corner++) {
                        const float corner_x = px[corner];
                        const float corner_z = pz[corner];
                        quad[corner][0] = -corner_z * sinf(angle) - corner_x * cosf(angle);
                        quad[corner][2] = -corner_x * sinf(angle) + corner_z * cosf(angle);
                        quad[corner][0] *= -1.0f;
                        quad[corner][0] += kCellSize * static_cast<float>(col);
                        quad[corner][2] += kCellSize * static_cast<float>(row);
                        quad[corner][1] = py[corner];
                    }

                    count++;

                    if (count >= kMaxPutQuads) {
                        break;
                    }
                }
            }
        }
    }

    const Quad &quad = quads[RandBelow(static_cast<float>(count))];
    float       max_x = quad[0][0];
    float       min_x = max_x;
    float       max_z = quad[0][2];
    float       min_z = max_z;

    for (int corner = 1; corner < 4; corner++) {
        min_x = std::min(min_x, quad[corner][0]);
        max_x = std::max(max_x, quad[corner][0]);

        min_z = std::min(min_z, quad[corner][2]);
        max_z = std::max(max_z, quad[corner][2]);
    }

    const float span_x = max_x - min_x;
    const float span_z = max_z - min_z;
    pos[0] = min_x + ScaledRand(span_x);
    pos[1] = quad[0][1];
    pos[2] = min_z + ScaledRand(span_z);
    pos[3] = 1.0f;
    return count;
}

/**
 * Builds the list of Atla one dungeon may hand out.
 */
void BtAtraListMake(int dungeon) {
    if (dungeon >= DUNGEON_DEMON_SHAFT) {
        return;
    }

    const ATRA_APPEAR *appear = AtraAppearData[dungeon];
    const int          center = CenterFloorTbl[dungeon];
    const int          floor_count = MaxFloorTbl[dungeon];
    int                count = 0;
    int                upper = 0;
    int                lower = 0;

    for (; appear[count].id != -1; count++) {
        const int floor = appear[count].floor;

        if (floor == -1) {
            upper += appear[count].count;
        }

        if (floor == -2) {
            lower += appear[count].count;
        }

        if (floor != -1 && floor != -2) {
            RegisterAtra(dungeon, floor - 1, count);
        }
    }

    for (int placed_num = 0; placed_num < upper; placed_num++) {
        bool placed = false;

        while (!placed) {
            const int floor = RandBelow(static_cast<float>(center - 1));

            if (DungeonStatus()->GetMaxAtraNum(dungeon, floor) < 8 && chkAtraFloor(dungeon, floor + 1) != 0) {
                RegisterAtra(dungeon, floor, -2);
                placed = true;
            }
        }
    }

    for (int placed_num = 0; placed_num < lower; placed_num++) {
        bool placed = false;

        while (!placed) {
            const int floor = RandBelow(static_cast<float>((floor_count - center) - 1)) + center;

            if (DungeonStatus()->GetMaxAtraNum(dungeon, floor) < 8 && chkAtraFloor(dungeon, floor + 1) != 0) {
                RegisterAtra(dungeon, floor, -2);
                placed = true;
            }
        }
    }

    for (int i = 0; i < count; i++) {
        ATRA_SAVE &entry = UserStatus->atra_registry[dungeon][i];
        entry.id = appear[i].id;
        entry.floor = appear[i].floor;
        entry.refcount = appear[i].count;
    }
}

int BtAtraFloorCyoice(int map_no, int floor_no, int *atra_no) {
    if (map_no >= 6) {
        return 0;
    }

    DungeonStatus()->GetMaxAtraNum(map_no, floor_no);
    const int center = CenterFloorTbl[map_no];
    DungeonStatus()->SetCopyAtraList(map_no, floor_no, atra_no);

    // Larger than the 100 registry entries: a roll can land on index 100.
    std::array<ATRA_SAVE, 128> registry{};
    std::ranges::copy(UserStatus->atra_registry[map_no], registry.begin());

    const int half = floor_no > center - 1 ? -2 : -1;
    int       count = 0;

    for (int i = 0; i < 8; i++) {
        if (atra_no[i] >= 0) {
            count++;
        } else if (atra_no[i] == -2) {
            bool placed = false;

            while (!placed) {
                const int pick = RandBelow(100.0f);

                if (registry[pick].id != -1 && registry[pick].refcount > 0 && half == registry[pick].floor) {
                    atra_no[i] = pick;
                    registry[pick].refcount--;
                    placed = true;
                }
            }

            count++;
        }
    }

    std::array<int, 8> packed{};
    packed.fill(-1);
    int packed_num = 0;

    for (int i = 0; i < 8; i++) {
        if (atra_no[i] >= 0) {
            packed[packed_num++] = atra_no[i];
        }
    }

    std::ranges::copy(packed, atra_no);
    return count;
}

int setCollisionData(CDungeonMap *map, CCPoly *poly, float *position, float radius, float height) {
    CBoxVu0 box;
    int     count = 0;

    box.max[0] = position[0] + radius;
    box.max[1] = position[1] + radius * height;
    box.max[2] = position[2] + radius;
    box.min[0] = position[0] - radius;
    box.min[1] = position[1] - radius * height;
    box.min[2] = position[2] - radius;

    if (map->map_type != 1) {
        sceVu0FVECTOR part_pos;

        for (int i = 0; map->parts[i].frame[0] != nullptr; i++) {
            CFrame *collision = map->parts[i].collision;

            if (collision == nullptr) {
                continue;
            }

            sceVu0CopyVector(part_pos, map->parts[i].frame_offset[0]);
            int turn = static_cast<int>(map->parts[i].frame_turn[0]);
            turn += PartsCollisionTurn(map, i);

            if (turn > 3) {
                turn -= 3;
            }

            collision->SetRotation(0.0f, QuarterTurnAngle(static_cast<float>(turn)), 0.0f);
            collision->SetPosition(part_pos);
            count += collision->PickUpNearPoly(&poly[count], box);
        }

        for (auto &treasure_box : map->boxes) {
            if (treasure_box.used != 0) {
                CFrame *collision = map->box_collision_model;
                collision->SetPosition(treasure_box.pos);
                count += collision->PickUpNearPoly(&poly[count], box);
            }
        }

        count = map->CreateCollision(poly, box, count);
        return NowDranMapField->AddCollision(poly, count, box);
    }

    for (const auto &offset : kNeighbourOffsets) {
        int x = static_cast<int>(position[0] / kCellSize);
        int z = static_cast<int>(position[2] / kCellSize);
        x += offset[0];
        z += offset[1];

        if (x < 0 || x > 19 || z < 0 || z > 19) {
            continue;
        }

        const auto &cell = map->cells[x + (z * kGridWidth)];
        CFrame     *model = PartsCollision(map, cell.parts_no);

        if (model == nullptr) {
            continue;
        }

        auto angle = static_cast<float>(cell.direction);
        angle += static_cast<float>(PartsCollisionTurn(map, cell.parts_no));

        if (angle > 3.0f) {
            angle -= 3.0f;
        }

        model->SetRotation(0.0f, QuarterTurnAngle(angle), 0.0f);
        model->SetPosition(kCellSize * static_cast<float>(x), 0.0f, kCellSize * static_cast<float>(z));
        count += model->PickUpNearPoly(&poly[count], box);
    }

    for (auto &treasure_box : map->boxes) {
        if (treasure_box.used != 0) {
            CFrame *model = map->box_collision_model;
            model->SetPosition(treasure_box.pos);
            count += model->PickUpNearPoly(&poly[count], box);
        }
    }

    return map->CreateCollision(poly, box, count);
}

CFrame *CDungeonParts::GetSearchFrame(char *name) {
    for (CFrame *model : frame) {
        if (model != nullptr) {
            CFrame *found = model->SearchFrame(name);

            if (found != nullptr) {
                return found;
            }
        }
    }

    if (collision != nullptr) {
        CFrame *found = collision->SearchFrame(name);

        if (found != nullptr) {
            return found;
        }
    }

    if (camera_collision != nullptr) {
        CFrame *found = camera_collision->SearchFrame(name);

        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

void CDungeonParts::SetHealZone(float *position, float width, float depth) {
    sceVu0CopyVector(heal_pos, position);
    heal_width = width;
    heal_depth = depth;
    heal_on = 1;
}

void CDungeonParts::Draw() {
    sceVu0FVECTOR position;

    for (int i = 0; i < 6; i++) {
        if (frame[i] == nullptr) {
            continue;
        }

        int turn = static_cast<int>(static_cast<float>(direction) + frame_turn[i]);

        if (turn > 3) {
            turn -= 3;
        }

        frame[i]->SetRotation(0.0f, QuarterTurnAngle(static_cast<float>(turn)), 0.0f);

        // frame_offset has five rows; the sixth model reads the fields that follow it.
        sceVu0CopyVector(position, pos);
        position[0] += frame_offset[i][0];
        position[1] += frame_offset[i][1];
        position[2] += frame_offset[i][2];
        position[3] = 1.0f;
        frame[i]->SetPosition(position);
        MGDraw(frame[i]);
    }
}

void CDungeonParts::DrawCalc(int x, int z, int turn, int map_type) {
    sceVu0FVECTOR position;

    for (int i = 0; i < 6; i++) {
        CFrame *model = frame[i];

        if (model == nullptr) {
            continue;
        }

        int model_turn = static_cast<int>(static_cast<float>(direction) + frame_turn[i]);

        if (model_turn > 3) {
            model_turn -= 3;
        }

        model->SetRotation(0.0f, QuarterTurnAngle(static_cast<float>(model_turn)), 0.0f);

        sceVu0CopyVector(position, pos);
        position[0] += frame_offset[i][0];
        position[1] += frame_offset[i][1];
        position[2] += frame_offset[i][2];
        position[3] = 1.0f;
        model->SetPosition(position);
    }

    if (collision == nullptr) {
        return;
    }

    if (map_type == 1) {
        int model_turn = turn + this->collision_turn;

        if (model_turn > 3) {
            model_turn -= 3;
        }

        collision->SetRotation(0.0f, QuarterTurnAngle(static_cast<float>(model_turn)), 0.0f);
        collision->SetPosition(kCellSize * static_cast<float>(x), 0.0f, kCellSize * static_cast<float>(z));
        return;
    }

    int model_turn = static_cast<int>(frame_turn[0] + static_cast<float>(this->collision_turn));

    if (model_turn > 3) {
        model_turn -= 3;
    }

    collision->SetRotation(0.0f, QuarterTurnAngle(static_cast<float>(model_turn)), 0.0f);
    // The collision model sits at the first frame's offset alone, not offset from the part's position.
    collision->SetPosition(frame_offset[0]);
}

void CDungeonParts::initalize() {
    for (int i = 0; i < 6; i++) {
        frame[i] = nullptr;
        frame_turn[i] = 0.0f;
    }

    direction_offset = 0;
    collision = nullptr;
    camera_collision = nullptr;
    camera_collision_turn = 0;
    collision_turn = 0;
    fire_num = 0;
    water.used = 0;
    water.has_fall = 0;
    heal_on = 0;
    direction = 0;
}

} // namespace dc::game
