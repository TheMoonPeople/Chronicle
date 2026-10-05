#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 775

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "character.hpp"
#include "cloth.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gameutil.hpp"
#include "mds.hpp"
#include "scriptinterpreter.hpp"
#include "sysmes.hpp"
#include "texture.hpp"

typedef MOTION_INFO tagMOTION_KEY;

/**
 * Describes one command a character configuration file may contain.
 */
struct COMMAND_INFO {
    char *name;          /**< Keyword that introduces the command. */
    int   arg_types[16]; /**< SCRIPT_ARGUMENT_TYPE of each argument, ended by -1. */
};

u_int *GetPackFile(u_int *pack, char *name, int *size = 0);
void  *InitCloth(CFrameVu1 *frame, input_str &input, CDataAlloc2<1> *alloc);

static void CommandVERTEX_ANIME(void **argv);
static void CommandSHADOW_VERTEX_ANIME(void **argv);
static void CommandMODEL(void **argv);
static void CommandSHADOW_MODEL(void **argv);
static void CommandMOTION(void **argv);
static void CommandSHADOW_MOTION(void **argv);
static void CommandKEY_START(void **argv);
static void CommandKEY(void **argv);
static void CommandMOTION_END(void **argv);
static void CommandCLOTH(void **argv);
static void CommandBODY_SIZE(void **argv);
static void CommandALLOC_MDT(void **argv);
static void CommandALLOC_DBUFF(void **argv);
static void CommandALLOC_SHADOW_MDT(void **argv);
static void CommandALLOC_SHADOW_DBUFF(void **argv);
static void CommandIMG(void **argv);
static void CommandIMG_END(void **argv);
static void CommandFOOT(void **argv);
static void CommandEVENT(void **argv);
static int  GetArg(input_str &input, int *arg_types, void **argv);
static int  SearchCommand(input_str &input, int *command);
static int  SkipSpace(input_str &input);
static int  CheckChar(char ch);
static void PreProcess(input_str &input);

static CDataAlloc2<1> *mds_buffer;
static CDataAlloc2<1> *buffer;
static CDataAlloc2<1> *exbuffer;
static CDataAlloc2<1> *img_buffer;
static int             texture_block;
static CCharacter     *character;
static int             vertex_anime;
static int             def_vtype;
static int             shadow_vertex_anime;
static u_int          *pack_data;
static int             motion_info_num;
static int             key_start;
static int             key_no;
static tagMOTION_KEY  *motion_info;
static int             cloth_cnt;
static int             now_motion_data;
static int             shadow_motion;
static int             dont_delete_texb;
static int             alloc_mdt_list;
static int             alloc_dbuff_list;
static int             alloc_smdt_list;
static int             alloc_sdbuff_list;
static int             fsound_num;
static int             tex_anime_list;
static char           *config_file;
static int             config_file_size;

static char   alloc_mdt[9][16];
static char   alloc_dbuff[9][16];
static char   alloc_smdt[9][16];
static char   alloc_sdbuff[9][16];
static u_int *load_img[4];

static COMMAND_INFO Command[19] = {
    {"VERTEX_ANIME",        {SCRIPT_ARGUMENT_INTEGER, -1}                                                                        },
    {"SHADOW_VERTEX_ANIME", {SCRIPT_ARGUMENT_INTEGER, -1}                                                                        },
    {"MODEL",               {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"SHADOW_MODEL",        {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"MOTION",              {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, -1}},
    {"SHADOW_MOTION",       {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, -1}                         },
    {"KEY",                 {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                        },
    {"KEY_START",           {SCRIPT_ARGUMENT_INTEGER, -1}                                                                        },
    {"MOTION_END",          {-1}                                                                                                 },
    {"CLOTH",               {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"BODY_SIZE",           {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                            },
    {"ALLOC_MDT",           {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"ALLOC_DBUFF",         {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"ALLOC_SHADOW_MDT",    {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"ALLOC_SHADOW_DBUFF",  {SCRIPT_ARGUMENT_STRING, -1}                                                                         },
    {"IMG",                 {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, -1}                                                },
    {"IMG_END",             {-1}                                                                                                 },
    {"FOOT",                {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                   },
    {"EVENT",               {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                        }
};

static void (*CommandExe[19])(void **) = {
    CommandVERTEX_ANIME,
    CommandSHADOW_VERTEX_ANIME,
    CommandMODEL,
    CommandSHADOW_MODEL,
    CommandMOTION,
    CommandSHADOW_MOTION,
    CommandKEY,
    CommandKEY_START,
    CommandMOTION_END,
    CommandCLOTH,
    CommandBODY_SIZE,
    CommandALLOC_MDT,
    CommandALLOC_DBUFF,
    CommandALLOC_SHADOW_MDT,
    CommandALLOC_SHADOW_DBUFF,
    CommandIMG,
    CommandIMG_END,
    CommandFOOT,
    CommandEVENT};

void ReadInfo(CCharacter *chara, u_int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *motion_alloc, CDataAlloc2<1> *extra_alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type, int keep_textures) {
    char  arg_storage[16][256];
    char *argv[18];
    int   file_size;
    int   command;

    def_vtype = visual_type;
    mds_buffer = model_alloc;
    buffer = motion_alloc;
    exbuffer = extra_alloc;
    img_buffer = image_alloc;
    character = chara;
    pack_data = pack;
    vertex_anime = 0;
    shadow_vertex_anime = 0;
    motion_info_num = 0;
    key_start = 0;
    key_no = 0;
    motion_info = 0;
    cloth_cnt = 0;
    now_motion_data = 0;
    shadow_motion = 0;
    texture_block = texture_block_no;
    alloc_mdt[0][0] = 0;
    alloc_dbuff[0][0] = 0;
    alloc_smdt[0][0] = 0;
    alloc_sdbuff[0][0] = 0;
    alloc_mdt_list = 0;
    alloc_dbuff_list = 0;
    alloc_smdt_list = 0;
    alloc_sdbuff_list = 0;
    fsound_num = 0;
    dont_delete_texb = keep_textures;

    for (int i = 0; i < 4; i++) {
        load_img[i] = 0;
    }

    tex_anime_list = 0;

    if (chara != 0) {
        char *config_text = (char *) GetPackFile(pack, name, &file_size);

        if (config_text == 0) {
            printf("not found %s\n", name);
        } else {
            config_file = config_text;
            config_file_size = file_size;
            input_str input;
            input.data = config_file;
            input.size = file_size;
            input.pos = 0;
            PreProcess(input);
            SkipSpace(input);

            for (int i = 0; i < 16; i++) {
                argv[i] = arg_storage[i];
            }

            while (SearchCommand(input, &command)) {
                if (command < 19 && command >= 0) {
                    int arg_status = GetArg(input, Command[command].arg_types, (void **) argv);

                    if (arg_status == 0) {
                        return;
                    }

                    if (arg_status < 0) {
                        printf("error!! at %s\n", Command[command].name);
                    }

                    CommandExe[command]((void **) argv);
                }
            }
        }
    }
}

static void CommandVERTEX_ANIME(void **argv) {
    vertex_anime = *(int *) argv[0];
}

static void CommandSHADOW_VERTEX_ANIME(void **argv) {
    shadow_vertex_anime = *(int *) argv[0];
}

static void CommandMODEL(void **argv) {
    character->Initialize();
    int visual_type = def_vtype;

    if (vertex_anime != 0) {
        visual_type = 6;
    }

    if ((def_vtype & 16) != 0) {
        visual_type = 18;
    }

    u_int *mds_file = GetPackFile(pack_data, (char *) argv[0]);

    if (mds_file == 0) {
        char *missing_name = (char *) argv[0];
        printf("not found %s\n", missing_name);
    } else {
        if (alloc_dbuff_list > 0 || alloc_mdt_list > 0) {
            char *mdt_names[12];
            char *dbuff_names[12];
            int   i;

            for (i = 0; i < alloc_mdt_list; i++) {
                mdt_names[i] = alloc_mdt[i];
            }

            mdt_names[i] = 0;

            for (i = 0; i < alloc_dbuff_list; i++) {
                dbuff_names[i] = alloc_dbuff[i];
            }

            dbuff_names[i] = 0;
            character->frame = LoadMDSFile(mds_file, mds_buffer, 0, dbuff_names, mdt_names);
        } else {
            character->frame = LoadMDSFile(mds_file, mds_buffer, visual_type, 0, 0);
        }

        character->config = (char *) mds_buffer->Alloc((config_file_size >> 4) + 1);
        character->config_size = config_file_size;
        memcpy(character->config, config_file, config_file_size);
    }
}

static void CommandSHADOW_MODEL(void **argv) {
    int visual_type = 8;

    if (shadow_vertex_anime != 0) {
        visual_type = 14;
    }

    u_int *mds_file = GetPackFile(pack_data, (char *) argv[0]);

    if (mds_file == 0) {
        char *missing_name = (char *) argv[0];
        printf("not found %s\n", missing_name);
    } else {
        character->shadow_frame = LoadMDSFile(mds_file, mds_buffer, visual_type, 0, 0);
    }
}

static void CommandMOTION(void **argv) {
    if (character->frame == 0) {
        return;
    }

    now_motion_data = *(int *) argv[0];
    character->ClearEvent(now_motion_data);

    if (now_motion_data < 0 || now_motion_data >= 8) {
        return;
    }

    if (now_motion_data > 0) {
        character->motion[now_motion_data] = (tagMOTION_TYPE *) character->motion_storage[now_motion_data].storage;
    }

    tagMOTION_TYPE *motion = character->motion[now_motion_data];
    memset(motion, 0, sizeof(tagMOTION_TYPE));
    char            *motion_name = (char *) argv[1];
    char            *bone_name = (char *) argv[2];
    char            *weight_name = (char *) argv[3];
    MOTION_FILE_INFO files[3];
    files[1].name = motion_name;
    files[0].name = bone_name;
    files[2].name = weight_name;
    files[0].size = 0;
    files[1].size = 0;
    files[2].size = 0;
    files[0].data = GetPackFile(pack_data, bone_name, &files[0].size);
    files[1].data = GetPackFile(pack_data, motion_name, &files[1].size);
    files[2].data = GetPackFile(pack_data, weight_name, &files[2].size);

    if (files[0].size == 0) {
        files[0].name = 0;
    }

    if (files[1].size == 0) {
        files[1].name = 0;
    }

    if (files[2].size == 0) {
        files[2].name = 0;
    }

    CreateAnimeDataEX(motion, buffer, files);

    if (character->frame_info == 0) {
        AnimeDataInit(character->frame, motion, mds_buffer, &character->frame_info);
    }

    motion->frame_info = character->frame_info;
    motion_info_num = 0;
    key_no = 0;
    motion_info = 0;
}

static void CommandSHADOW_MOTION(void **argv) {
    if (character->shadow_frame == 0) {
        return;
    }

    if (now_motion_data < 0 || now_motion_data >= 8) {
        return;
    }

    if (now_motion_data > 0) {
        character->shadow_motion[now_motion_data] = (tagMOTION_TYPE *) character->shadow_motion_storage[now_motion_data].storage;
    }

    tagMOTION_TYPE *motion = character->shadow_motion[now_motion_data];
    memset(motion, 0, sizeof(tagMOTION_TYPE));
    shadow_motion = 1;
    char            *motion_name = (char *) argv[0];
    char            *bone_name = (char *) argv[1];
    char            *weight_name = (char *) argv[2];
    MOTION_FILE_INFO files[3];
    files[0].size = 0;
    files[1].size = 0;
    files[2].size = 0;
    files[1].name = motion_name;
    files[0].name = bone_name;
    files[2].name = weight_name;
    files[0].data = GetPackFile(pack_data, bone_name, &files[0].size);
    files[1].data = GetPackFile(pack_data, motion_name, &files[1].size);
    files[2].data = GetPackFile(pack_data, weight_name, &files[2].size);

    if (bone_name[0] == 0) {
        files[0].name = 0;
    }

    if (weight_name[0] == 0) {
        files[2].name = 0;
    }

    if (files[0].data == 0 && files[1].data == 0 && files[2].data == 0) {
        return;
    }

    CreateAnimeDataEX(motion, buffer, files);

    if (character->shadow_frame_info == 0) {
        AnimeDataInit(character->shadow_frame, motion, mds_buffer, &character->shadow_frame_info);
    }

    motion->frame_info = character->shadow_frame_info;
}

static void CommandKEY_START(void **argv) {
    if (now_motion_data < 0 || now_motion_data >= 8) {
        return;
    }

    key_start = *(int *) argv[0];
    character->motion_start[now_motion_data] = key_start;
}

static void CommandKEY(void **argv) {
    if (motion_info == 0) {
        motion_info = (tagMOTION_KEY *) (buffer->base + buffer->used * 16);
    }

    tagMOTION_KEY *key = motion_info + key_no;
    key->start = *(int *) argv[0];
    key->end = *(int *) argv[1];
    key->speed = *(float *) argv[2];
#ifdef PAL_TIMING
    key->speed = 6.0f * key->speed / 5.0f;
#endif
    key_no++;
    motion_info_num = key_no;
}

static void CommandMOTION_END(void **argv) {
    if (now_motion_data < 0 || now_motion_data >= 8 || motion_info == 0) {
        return;
    }

    tagMOTION_TYPE *motion = character->motion[now_motion_data];
    buffer->Alloc((((unsigned int) (motion_info_num + 1) << 4) >> 4) + 1);
    motion_info[motion_info_num].start = -1;
    motion_info[motion_info_num].end = -1;
    motion_info[motion_info_num].speed = -1.0f;
    motion->motion_info = motion_info;
    motion->state.time = (float) motion->motion_info->start;
    motion->state.blend_step = 0.1f;
    motion->state.motion_no = 0;
    motion->state.playing_no = 0;
    motion->state.blending = false;

    if (shadow_motion != 0) {
        tagMOTION_TYPE *shadow_motion_type = character->shadow_motion[now_motion_data];
        shadow_motion_type->motion_info = motion_info;
        shadow_motion_type->state.time = (float) shadow_motion_type->motion_info->start;
        shadow_motion_type->state.blend_step = 0.1f;
        shadow_motion_type->state.motion_no = 0;
        shadow_motion_type->state.playing_no = 0;
        shadow_motion_type->state.blending = false;
    }

    character->motion_end[now_motion_data] = key_start + key_no;
}

static void CommandCLOTH(void **argv) {
    int    size;
    u_int *cloth_file = GetPackFile(pack_data, (char *) argv[0], &size);

    if (cloth_file != 0) {
        input_str input;
        input.data = (char *) cloth_file;
        input.size = size;
        input.pos = 0;
        character->cloth[cloth_cnt] = (CCloth *) InitCloth((CFrameVu1 *) character->frame, input, buffer);
        cloth_cnt++;
    }
}

static void CommandBODY_SIZE(void **argv) {
    character->body_height = *(float *) argv[0];
    character->body_width = *(float *) argv[1];
    character->body_depth = *(float *) argv[2];
}

static void CommandALLOC_MDT(void **argv) {
    if (alloc_mdt_list < 8) {
        strcpy(alloc_mdt[alloc_mdt_list++], (char *) argv[0]);
    }
}

static void CommandALLOC_DBUFF(void **argv) {
    if (alloc_dbuff_list < 8) {
        strcpy(alloc_dbuff[alloc_dbuff_list++], (char *) argv[0]);
    }
}

static void CommandALLOC_SHADOW_MDT(void **argv) {
    if (alloc_smdt_list < 8) {
        strcpy(alloc_smdt[alloc_smdt_list++], (char *) argv[0]);
    }
}

static void CommandALLOC_SHADOW_DBUFF(void **argv) {
    if (alloc_sdbuff_list < 8) {
        strcpy(alloc_sdbuff[alloc_sdbuff_list++], (char *) argv[0]);
    }
}

static void CommandIMG(void **argv) {
    if (img_buffer == 0) {
        return;
    }

    int slot = *(int *) argv[0];

    if (slot < 0 || slot >= 4) {
        return;
    }

    int    size;
    u_int *image_file = GetPackFile(pack_data, (char *) argv[1], &size);

    if (image_file != 0) {
        load_img[slot] = (u_int *) img_buffer->Alloc((size >> 4) + 1);
        memcpy(load_img[slot], image_file, size);
    }
}

static void CommandIMG_END(void **argv) {
    LOADTEXTURE_INFO2 textures[5];
    textures[0].block_no = 0;
    textures[0].mipmap = false;
    textures[0].name = 0;
    int i;
    int texture_count = 0;

    if (character->images[0] != 0 && load_img[0] == 0) {
        load_img[0] = character->images[0];
    }

    for (i = 0; i < 4; i++) {
        character->images[i] = load_img[i];

        if (load_img[i] != 0) {
            textures[texture_count].block_no = texture_block;
            textures[texture_count].mipmap = false;
            textures[texture_count].name = (char *) load_img[i];
            texture_count++;
        }
    }

    textures[texture_count].block_no = 0;
    textures[texture_count].mipmap = false;
    textures[texture_count].name = 0;

    if (texture_count > 0) {
        if (dont_delete_texb == 0) {
            TexManager.DeleteTextureBlock(texture_block);
        }

        TexManager.LoadTextureBlockEX(texture_block, textures);
    }

    if (config_file != 0) {
        character->tex_anime.LoadCFGFile(config_file, config_file_size);
    }
}

static void CommandFOOT(void **argv) {
    character->SetFootSound(*(float *) argv[0], *(float *) argv[1], now_motion_data);
}

static void CommandEVENT(void **argv) {
    if (*(int *) argv[1] >= 0) {
        character->SetEvent(*(float *) argv[0], *(int *) argv[1], *(int *) argv[2], now_motion_data);
    }
}

static int GetArg(input_str &input, int *arg_types, void **argv) {
    char word[256];

    if (arg_types[0] < 0) {
        return 1;
    }

    if (!SkipSpace(input)) {
        return 0;
    }

    int arg_count = 0;

    while (arg_types[arg_count++] >= 0)
        ;

    int ch;

    for (int i = 0; i < arg_count - 1; i++) {
        int length = 0;

        if (!SkipSpace(input)) {
            return 0;
        }

        while (1) {
            if (input.get(&ch) == 0) {
                return 0;
            }

            if (ch == ',' || !CheckChar(ch)) {
                break;
            }

            word[length++] = ch;
        }

        word[length] = 0;

        if (arg_types[i] != SCRIPT_ARGUMENT_FLOAT) {
            length = 1;

            if (arg_types[i] != SCRIPT_ARGUMENT_INTEGER) {
                switch (arg_types[i]) {
                    case SCRIPT_ARGUMENT_STRING:
                        break;
                    default:
                        goto invalid_type;
                }

                {
                    if (word[0] != '"') {
                        return -1;
                    }

                    while (1) {
                        char letter = word[length];

                        if (letter == '"') {
                            word[length] = 0;
                            break;
                        }

                        if (letter == 0) {
                            return -1;
                        }

                        length++;
                    }

                    strcpy((char *) argv[i], word + 1);
                    goto next_arg;
                }
            } else {
                for (length = 0; word[length] != 0; length++) {
                    char letter = word[length];

                    if (letter < '0' || letter > '9') {
                        return -1;
                    }
                }

                *(int *) argv[i] = atoi(word);
                goto next_arg;
            }
        } else {
            for (length = 0; word[length] != 0; length++) {
                char letter = word[length];

                if ((letter < '0' || letter > '9') && letter != '.' && letter != '-') {
                    return -1;
                }
            }

            *(float *) argv[i] = (float) atof(word);
            goto next_arg;
        }

    invalid_type:
        return -1;
    next_arg:;
    }

    return 1;
}

static int SearchCommand(input_str &input, int *command) {
    char word[256];

    if (!SkipSpace(input)) {
        return 0;
    }

    int length = 0;
    int ch;

    while (1) {
        if (input.get(&ch) == 0 && length == 0) {
            return 0;
        }

        if (!CheckChar(ch)) {
            break;
        }

        word[length++] = ch;
    }

    word[length] = 0;

    for (int i = 0; i < 19; i++) {
        if (strcmp(Command[i].name, word) == 0) {
            *command = i;
            return 1;
        }
    }

    *command = 20;
    return 1;
}

static int SkipSpace(input_str &input) {
    char *text;
    int   pos;

    text = input.data;
    pos = input.pos;

    while (pos < input.size) {
        if (CheckChar(text[pos])) {
            break;
        }

        pos++;
    }

    input.pos = pos;

    if (pos >= input.size) {
        return 0;
    }

    return 1;
}

static int CheckChar(char ch) {
    int is_space = 0;

    if (ch == ' ') {
        is_space = 1;
    }

    if (ch == '\t') {
        is_space = 1;
    }

    if (ch == '\n') {
        is_space = 1;
    }

    if (ch == '\r') {
        is_space = 1;
    }

    return !is_space;
}

static void PreProcess(input_str &input) {
    u_char *text;
    int     pos;

    text = (u_char *) input.data;
    pos = 0;

    while (pos < input.size) {
        if (text[pos] == '/' && text[pos + 1] == '/') {
            while (pos < input.size) {
                if (text[pos] == '\n' || text[pos] == '\r') {
                    break;
                }

                text[pos] = ' ';
                pos++;
            }
        }

        if (text[pos] == '/' && text[pos + 1] == '*') {
            while (pos < input.size) {
                if (text[pos] == '*' && text[pos + 1] == '/') {
                    text[pos] = ' ';
                    text[pos + 1] = ' ';
                    break;
                }

                text[pos] = ' ';
                pos++;
            }

            continue;
        }

        pos++;
    }
}

void CCharacter::StopCloth(int unused) {
    int i;

    for (i = 0; i < 4; i++) {
        if (cloth[i] != 0) {
            cloth[i]->stop = true;
        }
    }
}
#ifndef PORT
/**
 * Blends two bone transforms for a stretched vertex, on the vector unit.
 *
 * @mangled StretchBind2__FPfPfPf
 * @address 0x13B3E0
 * @size 0x64
 */
void StretchBind2(float *point_a, float *point_b, float *spring) {
    asm {
        lqc2 $vf10, 0x0($4)
        lqc2 $vf11, 0x0($5)
        lqc2 $vf12, 0x0($6)
        vmulx.xyzw $vf1, $vf0, $vf0x
        vsub.xyz $vf13, $vf10, $vf11
        vnop
        vnop
        vnop
        vaddy.x $vf5, $vf1, $vf13y
        vaddz.x $vf6, $vf1, $vf13z
        vmula.x $ACC, $vf13, $vf13
        vmadday.x $ACC, $vf5, $vf13y
        vmaddz.x $vf7, $vf6, $vf13z
        vrsqrt $Q, $vf12x, $vf7x
        vaddax.xyzw $ACC, $vf13, $vf0x
        vwaitq
        vmsubq.xyzw $vf25, $vf13, $Q
        vaddax.xyzw $ACC, $vf10, $vf0x
        vmsuby.xyz $vf10, $vf25, $vf12y
        vaddax.xyzw $ACC, $vf11, $vf0x
        vmaddz.xyz $vf11, $vf25, $vf12z
        sqc2 $vf10, 0x0($4)
        sqc2 $vf11, 0x0($5)
    }
}

/**
 * Gives the length of a three-component vector, on the vector unit.
 *
 * @mangled vuabs__FPf
 * @address 0x13B450
 * @size 0x30
 */
float vuabs(float *vector) {
    asm {
        lqc2 $vf4, 0x0($4)
        vmul.xyz $vf4, $vf4, $vf4
        vmr32.xy $vf5, $vf4
        vmr32.x $vf6, $vf5
        vadd.x $vf7, $vf4, $vf5
        vadd.x $vf5, $vf6, $vf7
        vsqrt $Q, $vf5x
        vwaitq
        cfc2.ni $2, $vi22
        mtc1 $2, $f0
    }
}
#endif
