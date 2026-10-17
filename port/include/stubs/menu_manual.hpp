#pragma once

#include "common.h"

// Expose the static ManualImgLoad as a global function so the port can link it
#define ManualImgLoad Mnu_ManualImgLoad

s16 Mnu_ManualImgLoad();