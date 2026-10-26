#include "dungeonparts.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
#include "mglib.hpp"
#include "userstatus.hpp"

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

/** The floors dungeon 0 keeps Atla off, ended by -1. */
static int noEntryTbl00[] = {4, 8, 11, -1};

/** The floors dungeon 1 keeps Atla off, ended by -1. */
static int noEntryTbl01[] = {4, 9, 12, -1};

/** The floors dungeon 2 keeps Atla off, ended by -1. */
static int noEntryTbl02[] = {5, 9, 12, -1};

/** The floors dungeon 3 keeps Atla off, ended by -1. */
static int noEntryTbl03[] = {5, 9, 13, -1};

/** The floors dungeon 4 keeps Atla off, ended by -1. */
static int noEntryTbl04[] = {4, 8, 11, -1};

/** The floors dungeon 5 keeps Atla off, ended by -1. */
static int noEntryTbl05[] = {19, 20, 21, 22, 23, -1};

/** The floors each dungeon keeps Atla off, one list per dungeon. */
static int *noEntryTbl[6] = {noEntryTbl00, noEntryTbl01, noEntryTbl02, noEntryTbl03, noEntryTbl04, noEntryTbl05};

/** The floor each dungeon's upper half ends on. */
int CenterFloorTbl[6] = {8, 9, 9, 9, 8, 13};

/** The number of floors in each dungeon. */
static int MaxFloorTbl[6] = {15, 17, 18, 18, 15, 25};

/**
 * Gives the two items the clown offers on one floor.
 *
 * @mangled GetPieroItem__FiiPiPi
 * @address 0x1BFAB0
 * @size 0x440
 */
void GetPieroItem(int map_no, int ura_dungeon, int *item0, int *item1) {
    PIERO_ITEM_SET *list = PieroItemListPtr[map_no + ura_dungeon * 7];
    s16            *rate = ItemSetRateTbl[map_no];
    int             count0;
    int             count1;
    int             pick0;
    int             pick1;
    int             chance;

    if (UserStatus->cur_floor < floorNum[map_no]) {
        count0 = list[0].count[0];
        count1 = list[0].count[1];
        pick1 = pick0 = -1;

        while (pick0 == -1) {
            pick0 = (int) (((float) count0 * (float) rand()) / 2.1474836e9f);

            if (pick0 >= count0) {
                pick0 = 0;
            }

            chance = (int) ((100.0f * (float) rand()) / 2.1474836e9f);

            if (rate[list[0].item[0][pick0] - 1] >= chance) {
                pick0 = -1;
            }
        }

        while (pick1 == -1) {
            pick1 = (int) (((float) count1 * (float) rand()) / 2.1474836e9f);

            if (pick1 >= count1) {
                pick1 = 0;
            }

            chance = (int) ((100.0f * (float) rand()) / 2.1474836e9f);

            if (rate[list[0].item[1][pick1] - 1] >= chance) {
                pick1 = -1;
            }
        }

        *item0 = list[0].item[0][pick0];
        *item1 = list[0].item[1][pick1];
        return;
    }

    count0 = list[1].count[0];
    count1 = list[1].count[1];
    pick1 = pick0 = -1;

    while (pick0 == -1) {
        pick0 = (int) (((float) count0 * (float) rand()) / 2.1474836e9f);

        if (pick0 >= count0) {
            pick0 = 0;
        }

        chance = (int) ((100.0f * (float) rand()) / 2.1474836e9f);

        if (rate[list[1].item[0][pick0] - 1] >= chance) {
            pick0 = -1;
        }
    }

    while (pick1 == -1) {
        pick1 = (int) (((float) count1 * (float) rand()) / 2.1474836e9f);

        if (pick1 >= count1) {
            pick1 = 0;
        }

        chance = (int) ((100.0f * (float) rand()) / 2.1474836e9f);

        if (rate[list[1].item[1][pick1] - 1] >= chance) {
            pick1 = -1;
        }
    }

    *item0 = list[1].item[0][pick0];
    *item1 = list[1].item[1][pick1];
}

int PresetSmallItemNo_Get(int map_no, int floor_no, int special, int small);

char info_cfg_literal[] __attribute__((section(".rodata"))) = "info.cfg";

/**
 * Scales a coordinate from the item area table up to world scale.
 */
static inline float ToWorldScale(float coordinate) {
    return coordinate * 10.0f;
}

int SearchiDoPutArea(MAPPARTS *cells, int x, int y, int width, int height, float *pos) {
    float           quad[196][4][3];
    float           px[4];
    float           py[4];
    float           pz[4];
    int             count = 0;
    ITEM_FREE_AREA *areas = ItemFreeAreaAll[selectMapNo];

    for (int row = y; row < y + height; row++) {
        for (int col = x; col < x + width; col++) {
            int parts_no = (cells + row * 20)[col].parts_no;
            int direction = (cells + row * 20)[col].direction;

            for (int area_no = 0; areas[area_no].parts_no != -1 && count < 196; area_no++) {
                if (parts_no != areas[area_no].parts_no) {
                    continue;
                }

                for (int rect_no = 0; rect_no < areas[area_no].rect_num; rect_no++) {
                    int turn = areas[area_no].direction;
                    turn += direction;

                    if (turn > 3) {
                        turn -= 4;
                    }

                    float angle = (PI * (90.0f * (float) (4 - turn))) / 180.0f;
                    px[0] = ToWorldScale(areas[area_no].rect[rect_no].x0);
                    py[0] = areas[area_no].rect[rect_no].y0 * 10.0f;
                    pz[0] = areas[area_no].rect[rect_no].z0 * 10.0f;
                    px[3] = areas[area_no].rect[rect_no].x1 * 10.0f;
                    py[3] = areas[area_no].rect[rect_no].y1 * 10.0f;
                    pz[3] = areas[area_no].rect[rect_no].z1 * 10.0f;
                    px[1] = px[3];
                    py[1] = py[0];
                    pz[1] = pz[0];
                    px[2] = px[0];
                    py[2] = py[0];
                    pz[2] = pz[3];

                    for (int corner = 0; corner < 4; corner++) {
                        if (count < 196) {
                            float corner_z;
                            float corner_x;
                            quad[count][corner][0] = -(corner_z = pz[corner]) * sinf(angle) - (corner_x = px[corner]) * cosf(angle);
                            quad[count][corner][2] = -corner_x * sinf(angle) + corner_z * cosf(angle);
                            quad[count][corner][0] *= -1.0f;
                            quad[count][corner][0] += 160.0f * (float) col;
                            quad[count][corner][2] += 160.0f * (float) row;
                            quad[count][corner][1] = py[corner];
                        }
                    }

                    count++;

                    if (count >= 196) {
                        break;
                    }
                }
            }
        }
    }

    int   pick = (int) (((float) count * (float) rand()) / 2.1474836e9f);
    float max_x = quad[pick][0][0];
    float min_x = max_x;
    float max_z = quad[pick][0][2];
    float min_z = max_z;

    for (int corner = 1; corner < 4; corner++) {
        if (min_x > quad[pick][corner][0]) {
            min_x = quad[pick][corner][0];
        }

        if (max_x < quad[pick][corner][0]) {
            max_x = quad[pick][corner][0];
        }

        if (min_z > quad[pick][corner][2]) {
            min_z = quad[pick][corner][2];
        }

        if (max_z < quad[pick][corner][2]) {
            max_z = quad[pick][corner][2];
        }
    }

    float span_x = max_x - min_x;
    float span_z = max_z - min_z;
    pos[0] = min_x + (span_x * (float) rand()) / 2.1474836e9f;
    pos[1] = quad[pick][0][1];
    pos[2] = min_z + (span_z * (float) rand()) / 2.1474836e9f;
    pos[3] = 1.0f;
    return count;
}

/**
 * Reports whether an Atla may be placed on one floor.
 *
 * @mangled chkAtraFloor__Fii
 * @address 0x1C0940
 * @size 0x74
 */
int chkAtraFloor(int dungeon, int floor) {
    if (dungeon >= DUNGEON_DEMON_SHAFT) {
        return 0;
    }

    int *floors = noEntryTbl[dungeon];

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
static inline void RegisterAtra(int dungeon, int floor, int atra_id) {
    ((CDngStatusData *) UserStatus)->SetGetAtra(dungeon, floor, atra_id);
}

/**
 * Builds the list of Atla one dungeon may hand out.
 *
 * @mangled BtAtraListMake__Fi
 * @address 0x1C09C0
 * @size 0x358
 */
void BtAtraListMake(int dungeon) {
    if (dungeon >= DUNGEON_DEMON_SHAFT) {
        return;
    }

    ATRA_APPEAR *appear = AtraAppearData[dungeon];
    int          center = CenterFloorTbl[dungeon];
    int          max = MaxFloorTbl[dungeon];
    int          count = 0;
    int          upper = 0;
    int          lower = 0;

    for (; appear[count].id != -1; count++) {
        int floor = appear[count].floor;

        if (floor == -1) {
            upper += appear[count].count;
        }

        if (floor == -2) {
            lower += appear[count].count;
        }

        if (floor != -1 && floor != -2) {
            RegisterAtra(dungeon, --floor, count);
        }
    }

    int i;

    for (i = 0; i < upper; i++) {
        int placed = false;

        while (placed == 0) {
            int floor = (int) (((float) (center - 1) * (float) rand()) / 2.1474836e9f);

            if (((CDngStatusData *) UserStatus)->GetMaxAtraNum(dungeon, floor) < 8 && chkAtraFloor(dungeon, floor + 1) != 0) {
                RegisterAtra(dungeon, floor, -2);
                placed = true;
            }
        }
    }

    for (i = 0; i < lower; i++) {
        int placed = false;

        while (placed == 0) {
            int floor = (int) (((float) ((max - center) - 1) * (float) rand()) / 2.1474836e9f);
            floor += center;

            if (((CDngStatusData *) UserStatus)->GetMaxAtraNum(dungeon, floor) < 8 && chkAtraFloor(dungeon, floor + 1) != 0) {
                RegisterAtra(dungeon, floor, -2);
                placed = true;
            }
        }
    }

    for (int j = 0; j < count; j++) {
        UserStatus->atra_registry[dungeon][j].id = appear[j].id;
        UserStatus->atra_registry[dungeon][j].floor = appear[j].floor;
        UserStatus->atra_registry[dungeon][j].refcount = appear[j].count;
    }
}

/**
 * Selects the atla identifiers that appear on a floor: keeps the fixed ones,
 * draws each open slot from the dungeon's list for its half, packs them to the
 * front and returns how many there are.
 *
 * @mangled BtAtraFloorCyoice__FiiPi
 * @address 0x1C0D20
 * @size 0x2A0
 */
int BtAtraFloorCyoice(int map_no, int floor_no, int *atra_no) {
    ATRA_SAVE registry[128];
    int       packed[8];

    if (map_no >= 6) {
        return 0;
    }

    ((CDngStatusData *) UserStatus)->GetMaxAtraNum(map_no, floor_no);
    int center = CenterFloorTbl[map_no];
    ((CDngStatusData *) UserStatus)->SetCopyAtraList(map_no, floor_no, atra_no);

    for (int j = 0; j < 100; j++) {
        registry[j].id = UserStatus->atra_registry[map_no][j].id;
        registry[j].floor = UserStatus->atra_registry[map_no][j].floor;
        registry[j].refcount = UserStatus->atra_registry[map_no][j].refcount;
    }

    int half = -1;

    if (floor_no > center - 1) {
        half = -2;
    }

    int count = 0;

    for (int i = 0; i < 8; i++) {
        if (atra_no[i] >= 0) {
            count++;
        } else if (atra_no[i] == -2) {
            int placed = false;

            while (placed == 0) {
                int pick = (int) ((100.0f * (float) rand()) / 2.1474836e9f);

                if (registry[pick].id != -1 && registry[pick].refcount > 0 && half == registry[pick].floor) {
                    atra_no[i] = pick;
                    registry[pick].refcount--;
                    placed = true;
                }
            }

            count++;
        }
    }

    for (int k = 0; k < 8; k++) {
        packed[k] = -1;
    }

    int packed_num = 0;

    for (int m = 0; m < 8; m++) {
        if (atra_no[m] >= 0) {
            packed[packed_num++] = atra_no[m];
        }
    }

    memcpy(atra_no, packed, sizeof(packed));
    return count;
}

/**
 * Gives a map part's collision model, or null for an empty cell.
 */
static inline CFrame *PartsCollision(CDungeonMap *map, int parts_no) {
    if (parts_no == -1) {
        return NULL;
    }

    return map->parts[parts_no].collision;
}

/**
 * Gives the quarter turns a map part's collision model is given, or 0 for an empty cell.
 */
static inline int PartsCollisionTurn(CDungeonMap *map, int parts_no) {
    if (parts_no == -1) {
        return 0;
    }

    return map->parts[parts_no].collision_turn;
}

int setCollisionData(CDungeonMap *map, CCPoly *poly, float *position, float radius, float height) {
    CBoxVu0 box;
    int     count = 0;
    CFrame *collision;
    int     i;
    int     turn;

    box.max[0] = position[0] + radius;
    box.max[1] = position[1] + radius * height;
    box.max[2] = position[2] + radius;
    box.min[0] = position[0] - radius;
    box.min[1] = position[1] - radius * height;
    box.min[2] = position[2] - radius;
    int around[9][2] = {
        {0,  0 },
        {-1, 0 },
        {0,  -1},
        {1,  0 },
        {0,  1 },
        {-1, -1},
        {1,  -1},
        {-1, 1 },
        {1,  1 }
    };

    if (map->map_type != 1) {
        sceVu0FVECTOR part_pos;

        for (i = 0; map->parts[i].frame[0] != NULL; i++) {
            if (i == -1) {
                collision = NULL;
            } else {
                collision = map->parts[i].collision;
            }

            if (collision == NULL) {
                continue;
            }

            sceVu0CopyVector(part_pos, map->parts[i].frame_offset[0]);
            turn = (int) map->parts[i].frame_turn[0];
            turn += PartsCollisionTurn(map, i);

            if (turn > 3) {
                turn -= 3;
            }

            if (turn == 3) {
                turn = -1;
            }

            float angle = (PI * (-90.0f * (float) turn)) / 180.0f;
            collision->SetRotation(0.0f, angle, 0.0f);
            collision->SetPosition(part_pos);
            count += collision->PickUpNearPoly(&poly[count], box);
        }

        for (i = 0; i < 24; i++) {
            if (map->boxes[i].used != 0) {
                collision = map->box_collision_model;
                collision->SetPosition(map->boxes[i].pos);
                count += collision->PickUpNearPoly(&poly[count], box);
            }
        }

        count = map->CreateCollision(poly, box, count);
        count = NowDranMapField->AddCollision(poly, count, box);
    } else {
        CFrame *model;

        for (int j = 0; j < 9; j++) {
            int x = (int) (position[0] / 160.0f);
            int z = (int) (position[2] / 160.0f);
            x += around[j][0];
            z += around[j][1];

            if (x < 0 || x > 19 || z < 0 || z > 19) {
                continue;
            }

            model = PartsCollision(map, map->cells[x + z * 20].parts_no);

            if (model == NULL) {
                continue;
            }

            float angle = (float) map->cells[x + z * 20].direction;
            angle += (float) PartsCollisionTurn(map, map->cells[x + z * 20].parts_no);

            if (angle > 3.0f) {
                angle -= 3.0f;
            }

            if (angle == 3.0f) {
                angle = -1.0f;
            }

            angle = (PI * (-90.0f * angle)) / 180.0f;
            model->SetRotation(0.0f, angle, 0.0f);
            model->SetPosition(160.0f * (float) x, 0.0f, 160.0f * (float) z);
            count += model->PickUpNearPoly(&poly[count], box);
        }

        for (int k = 0; k < 24; k++) {
            if (map->boxes[k].used != 0) {
                model = map->box_collision_model;
                model->SetPosition(map->boxes[k].pos);
                count += model->PickUpNearPoly(&poly[count], box);
            }
        }

        count = map->CreateCollision(poly, box, count);
    }

    return count;
}

CFrame *CDungeonParts::GetSearchFrame(char *name) {
    for (int i = 0; i < 6; i++) {
        if (frame[i] != NULL) {
            CFrame *found = frame[i]->SearchFrame(name);

            if (found != NULL) {
                return found;
            }
        }
    }

    // The collision and the second model are searched after the drawn ones.
    if (collision != NULL) {
        CFrame *found = collision->SearchFrame(name);

        if (found != NULL) {
            return found;
        }
    }

    if (camera_collision != NULL) {
        CFrame *found = camera_collision->SearchFrame(name);

        if (found != NULL) {
            return found;
        }
    }

    return NULL;
}

/**
 * Places a healing zone within one dungeon part.
 *
 * @mangled SetHealZone__13CDungeonPartsFPfff
 * @address 0x1C1670
 * @size 0x58
 */
void CDungeonParts::SetHealZone(float *position, float width, float depth) {
    sceVu0CopyVector(heal_pos, position);
    heal_width = width;
    heal_depth = depth;
    heal_on = 1;
}

/**
 * Draws one dungeon part and everything standing on it.
 *
 * @mangled Draw__13CDungeonPartsFv
 * @address 0x1C16D0
 * @size 0x17C
 */
void CDungeonParts::Draw() {
    sceVu0FVECTOR position;

    for (int i = 0; i < 6; i++) {
        if (frame[i] == NULL) {
            continue;
        }

        int turn = (int) ((float) direction + frame_turn[i]);

        if (turn > 3) {
            turn -= 3;
        }

        float angle = (float) turn;

        if (3.0f == angle) {
            angle = -1.0f;
        }

        angle = (PI * (-90.0f * angle)) / 180.0f;
        frame[i]->SetRotation(0.0f, angle, 0.0f);

        sceVu0CopyVector(position, pos);
        position[0] += frame_offset[i][0];
        position[1] += frame_offset[i][1];
        position[2] += frame_offset[i][2];
        position[3] = 1.0f;
        frame[i]->SetPosition(position);
        MGDraw(frame[i]);
    }
}

/**
 * Chooses the level of detail each of a part's frames draws at.
 *
 * @mangled DrawCalc__13CDungeonPartsFiiii
 * @address 0x1C1850
 * @size 0x348
 */
void CDungeonParts::DrawCalc(int x, int z, int turn, int map_type) {
    sceVu0FVECTOR position;

    for (int i = 0; i < 6; i++) {
        CFrame *model = frame[i];

        if (model == NULL) {
            continue;
        }

        int model_turn = (int) ((float) direction + frame_turn[i]);

        if (model_turn > 3) {
            model_turn -= 3;
        }

        float angle = (float) model_turn;

        if (3.0f == angle) {
            angle = -1.0f;
        }

        angle = (PI * (-90.0f * angle)) / 180.0f;
        model->SetRotation(0.0f, angle, 0.0f);

        sceVu0CopyVector(position, pos);
        position[0] += frame_offset[i][0];
        position[1] += frame_offset[i][1];
        position[2] += frame_offset[i][2];
        position[3] = 1.0f;
        frame[i]->SetPosition(position);
    }

    CFrame *model = collision;

    if (model == NULL) {
        return;
    }

    if (map_type == 1) {
        int collision_turn = turn + this->collision_turn;

        if (collision_turn > 3) {
            collision_turn -= 3;
        }

        if (collision_turn == 3) {
            collision_turn = -1;
        }

        model->SetRotation(0.0f, (PI * (-90.0f * (float) collision_turn)) / 180.0f, 0.0f);
        float y = 0.0f;
        collision->SetPosition(160.0f * (float) x, y, 160.0f * (float) z);
        return;
    }

    int collision_turn = (int) (frame_turn[0] + (float) this->collision_turn);

    if (collision_turn > 3) {
        collision_turn -= 3;
    }

    if (collision_turn == 3) {
        collision_turn = -1;
    }

    model->SetRotation(0.0f, (PI * (-90.0f * (float) collision_turn)) / 180.0f, 0.0f);
    sceVu0CopyVector(position, pos);
    position[0] += frame_offset[0][0];
    position[1] += frame_offset[0][1];
    position[2] += frame_offset[0][2];
    position[3] = 1.0f;
    collision->SetPosition(frame_offset[0]);
}

/**
 * Clears one dungeon part.
 *
 * @mangled initalize__13CDungeonPartsFv
 * @address 0x1C1BA0
 * @size 0x5C
 */
void CDungeonParts::initalize() {
    for (int i = 0; i < 6; i++) {
        frame[i] = NULL;
        frame_turn[i] = 0.0f;
    }

    direction_offset = 0;
    collision = NULL;
    camera_collision = NULL;
    camera_collision_turn = 0;
    collision_turn = 0;
    fire_num = 0;
    water.used = 0;
    water.has_fall = 0;
    heal_on = 0;
    direction = 0;
}
