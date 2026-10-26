#pragma name_counter 567

#include "menuetc.hpp"

#include "camera.hpp"
#include "clsmes.hpp"
#include "rect.hpp"

/* The menus' shared objects: the camera their models are drawn through, the message windows they
   print into, and the screen area they lay themselves out in. */
CCamera  MenuCamera(4.0f);
ClsMes   CommonMenuMes1;
ClsMes   CommonMenuMes2;
ClsMes   CommonMenuMes3;
ClsMes   AtoraNameMes;
CRect_i_ MenuDispRc(0, 0, 640, SCREEN_HEIGHT);
