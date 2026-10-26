#include "mglib.hpp"

#include <libdma.h>
#include <libgraph.h>
#include <libvu0.h>

#include "renderinfo.hpp"
#include "texture.hpp"

int            DBuffID;
int            mgWaitVSync;
sceVif1Packet *Vif1Packet;
int            VSyncField__2;
int            mgClearBackFlag;
int            mgAdjustX;
int            mgAdjustY;
sceGsTex1      mgTEX1Env;
sceGsTest      mgPixelTest;
sceGsZbuf      mgZBuffer;
sceGsAlpha     mgAlpha;
sceGsTexa      mgTexa;
int            mgTopVRAM;
int            mgZBufferAdr;

MG_PICKZ      mgPickZBuff[16];
sceGifTag     GiftagAD;
sceVu0FVECTOR mgBackColor;

RenderInfo      mgRenderInfo;
CTextureManager TexManager;

sceVu0FVECTOR mgZeroVector;
sceVu0FVECTOR mgUnitVector;
sceVu0FVECTOR mgUnitVector2;
sceVu0FVECTOR mgZeroVector2;
sceVu0FMATRIX mgUnitMatrix;
sceVu0FMATRIX mgZeroMatrix;
