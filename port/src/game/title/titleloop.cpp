#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "editloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "memcard.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "object.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/cursol.hpp"
#include "title/dispfade.hpp"
#include "title/logo.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_c.hpp"
#include "title/rushmovi.hpp"
#include "title/scfader.hpp"
#include "title/script.hpp"
#include "title/sprite.hpp"
#include "title/title.hpp"
#include "title/titleloop.hpp"
#include "vutext.hpp"
#include "wind.hpp"

int    Fade1;
int    Fade2;
int    Fade3;
int    Fade4;
int    Wait;
int    opcnt;
int    keywait;
u_char brink;
int    brinkcnt;
int    EffCnt;

void TitleInit(int mode) {
    float scale = 750.0f;
    float far_z = 65535.0f;

    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 200000);
    SetDataBuffer(&MotionData, 500000);
    SetDataBuffer(&TextureData, 300000);
    SetPacketReadBuffer(40000, 300000);

    MGSetRenderInfo(scale, 4.0f, far_z);
    Camera.SetRef(nullptr, 0.0f, 0.0f, 0.0f);
    Camera.SetPos(nullptr, 0.0f, 0.0f, 0.0f);
    FCamera.SetFollow(0.0f, 0.0f, 0.0f);
    FCamera.SetAngle(0.0f);
    FCamera.SetDistance(180.0f);
    FCamera.SetHeight(2.0f);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    setbilinear(1);

    LOADTEXTURE_INFO menu_textures[] = {
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#fontbase#512#256#1",                             26, 0},
        {"meswin/gaiji.img",                                26, 0},
        {"meswin/fuki256.img",                              26, 0},
        {"meswin/syst04.img",                               26, 0},
        {"",                                                0,  0}
    };

    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, menu_textures, read_buffer);
    LoadFileMenuData("stayframe.img", read_buffer);
    TexManager.EnterFixTextureZ(reinterpret_cast<u_char *>(read_buffer));

    switch (LanguageCode) {
        case LANG_JAPANESE:
        case LANG_ENGLISH_US:
            LoadFile("titledat/title.pak", read_buffer, nullptr);
            break;
        case LANG_ENGLISH_UK:
            LoadFile("titledat/title_eu.pak", read_buffer, nullptr);
            break;
        case LANG_FRENCH:
            LoadFile("titledat/title_f.pak", read_buffer, nullptr);
            break;
        case LANG_GERMAN:
            LoadFile("titledat/title_g.pak", read_buffer, nullptr);
            break;
        case LANG_ITALIAN:
            LoadFile("titledat/title_i.pak", read_buffer, nullptr);
            break;
        case LANG_SPANISH:
            LoadFile("titledat/title_s.pak", read_buffer, nullptr);
            break;
        default:
            break;
    }

    LOADTEXTURE_INFO2 title_textures[] = {
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 0, 0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 1, 0},
        {nullptr,                                         0, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         0, 0}
    };

    title_textures[2].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "bg.img", nullptr));
    title_textures[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "title.img", nullptr));
    title_textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "main.img", nullptr));
    title_textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pat01.img", nullptr));
    title_textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pat02.img", nullptr));
    title_textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start.img", nullptr));
    title_textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start3.img", nullptr));
    title_textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "icon01.img", nullptr));
    TexManager.LoadTextureBlock(-1, title_textures);

    sceVu0FVECTOR pos;

    ObjectFrame3 = LoadMDSFile(GetPackFile(read_buffer, "sky.mds", nullptr), 2, 0);
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = -28.0f;
    ObjectFrame3->SetPosition(pos);

    Cloud__2.LoadPackData(read_buffer, "cloud.cfg", &MotionData, nullptr);
    Cloud__2.motion_type.state.time = 100.0f;
    Cloud__2.motion_type.state.blend_step = 0.1f;
    Cloud__2.motion_type.state.motion_no = 0;
    Cloud__2.motion_type.state.playing_no = 0;
    Cloud__2.motion_type.state.blending = false;
    Cloud__2.motion_no = 0;
    Cloud__2.SetPosition(0.0f, 0.0f, -5.0f);

    Logo.LoadPackData(read_buffer, "logo.cfg", &MotionData, nullptr);
    Logo.motion_type.state.time = 1.0f;
    Logo.motion_type.state.blend_step = 0.1f;
    Logo.motion_type.state.motion_no = 0;
    Logo.motion_type.state.playing_no = 0;
    Logo.motion_type.state.blending = false;
    Logo.motion_no = 0;

    char *spark_names[9] = {
        "logo_p1.cfg", "logo_p2.cfg", "logo_p3.cfg", "logo_p4.cfg", "logo_p5.cfg",
        "logo_p6.cfg", "logo_p7.cfg", "logo_p8.cfg", "logo_p9.cfg"};

    for (int i = 0; i < 9; i++) {
        Spark[i].LoadPackData(read_buffer, spark_names[i], &MotionData, nullptr);
        Spark[i].motion_type.state.time = 1.0f;
        Spark[i].motion_type.state.blend_step = 0.1f;
        Spark[i].motion_type.state.motion_no = 0;
        Spark[i].motion_type.state.playing_no = 0;
        Spark[i].motion_type.state.blending = false;
        Spark[i].motion_no = 0;
        Spark[i].SetPosition(0.0f, 11.599f, -19.099f);
    }

    CSnd.SetReverb(0, 4, 5);
    CSnd.SetReverb(1, 0, 0);
    CSnd.LoadSoundFileFromPack("title.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);

    Fade1 = Fade2 = Fade3 = Fade4 = Wait = 0;
    opcnt = 0;
    keywait = 0;
    brinkcnt = 0;
    brink = 0;
    EffCnt = 0;
    CProcess.no = TITLE_STEP_FADE_IN;
    CFade.value = 0;

    CSprite.Init();
    CLogo.Init();
    CCursol.Init();

    if (mode == 1) {
        CCursol.select = TITLE_MENU_LOAD;
        CCursol.Set(316.0f);

        for (int i = 0; i < 10; i++) {
            CCursol.Move();
        }
    }

    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
    GamePad.MenuModeOn(120);
}
