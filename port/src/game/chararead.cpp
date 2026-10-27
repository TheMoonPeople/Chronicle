#include "chararead.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <print>
#include <string>
#include <string_view>

#include "character.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gameutil.hpp"
#include "mds.hpp"
#include "scriptinterpreter.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "vu0_ops.hpp"

namespace dc::game {

using namespace dc::data;

namespace {

using tagMOTION_KEY = MOTION_INFO;

/**
 * Describes one command a character configuration file may contain.
 */
struct COMMAND_INFO {
    const char *name;          /**< Keyword that introduces the command. */
    int         arg_types[16]; /**< SCRIPT_ARGUMENT_TYPE of each argument, ended by -1. */
};

constexpr int         COMMAND_COUNT = 19;
constexpr int         UNKNOWN_COMMAND = 20;
constexpr int         ARGUMENT_MAX = 16;
constexpr std::size_t ARGUMENT_SIZE = 256;
constexpr int         ALLOC_NAME_MAX = 8;

void CommandVERTEX_ANIME(void **argv);
void CommandSHADOW_VERTEX_ANIME(void **argv);
void CommandMODEL(void **argv);
void CommandSHADOW_MODEL(void **argv);
void CommandMOTION(void **argv);
void CommandSHADOW_MOTION(void **argv);
void CommandKEY_START(void **argv);
void CommandKEY(void **argv);
void CommandMOTION_END(void **argv);
void CommandCLOTH(void **argv);
void CommandBODY_SIZE(void **argv);
void CommandALLOC_MDT(void **argv);
void CommandALLOC_DBUFF(void **argv);
void CommandALLOC_SHADOW_MDT(void **argv);
void CommandALLOC_SHADOW_DBUFF(void **argv);
void CommandIMG_END(void **argv);
void CommandFOOT(void **argv);
void CommandEVENT(void **argv);
int  GetArg(input_str &input, const int *arg_types, void **argv);
bool SearchCommand(input_str &input, int *command);
bool SkipSpace(input_str &input);
bool CheckChar(char ch);
void PreProcess(input_str &input);

CDataAlloc2<1> *mds_buffer;
CDataAlloc2<1> *buffer;
CDataAlloc2<1> *exbuffer;
CDataAlloc2<1> *img_buffer;
int             texture_block;
CCharacter     *character;
int             vertex_anime;
int             def_vtype;
int             shadow_vertex_anime;
u_int          *pack_data;
int             motion_info_num;
int             key_start;
int             key_no;
tagMOTION_KEY  *motion_info;
int             cloth_cnt;
int             now_motion_data;
int             shadow_motion;
int             dont_delete_texb;
int             alloc_mdt_list;
int             alloc_dbuff_list;
int             alloc_smdt_list;
int             alloc_sdbuff_list;
int             fsound_num;
int             tex_anime_list;
char           *config_file;
int             config_file_size;

std::array<std::string, ALLOC_NAME_MAX> alloc_mdt;
std::array<std::string, ALLOC_NAME_MAX> alloc_dbuff;
std::array<std::string, ALLOC_NAME_MAX> alloc_smdt;
std::array<std::string, ALLOC_NAME_MAX> alloc_sdbuff;
u_int                                  *load_img[4];

constexpr std::array<COMMAND_INFO, COMMAND_COUNT> Command = {
    {
     {.name = "VERTEX_ANIME", .arg_types = {SCRIPT_ARGUMENT_INTEGER, -1}},
     {.name = "SHADOW_VERTEX_ANIME", .arg_types = {SCRIPT_ARGUMENT_INTEGER, -1}},
     {.name = "MODEL", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "SHADOW_MODEL", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "MOTION", .arg_types = {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "SHADOW_MOTION", .arg_types = {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "KEY", .arg_types = {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "KEY_START", .arg_types = {SCRIPT_ARGUMENT_INTEGER, -1}},
     {.name = "MOTION_END", .arg_types = {-1}},
     {.name = "CLOTH", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "BODY_SIZE", .arg_types = {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "ALLOC_MDT", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "ALLOC_DBUFF", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "ALLOC_SHADOW_MDT", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "ALLOC_SHADOW_DBUFF", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "IMG", .arg_types = {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "IMG_END", .arg_types = {-1}},
     {.name = "FOOT", .arg_types = {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "EVENT", .arg_types = {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}},
     }
};

constexpr std::array<void (*)(void **), COMMAND_COUNT> CommandExe = {
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

} // namespace

void ReadInfo(CCharacter *chara, u_int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *motion_alloc, CDataAlloc2<1> *extra_alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type, int keep_textures) {
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
    motion_info = nullptr;
    cloth_cnt = 0;
    now_motion_data = 0;
    shadow_motion = 0;
    texture_block = texture_block_no;
    alloc_mdt[0].clear();
    alloc_dbuff[0].clear();
    alloc_smdt[0].clear();
    alloc_sdbuff[0].clear();
    alloc_mdt_list = 0;
    alloc_dbuff_list = 0;
    alloc_smdt_list = 0;
    alloc_sdbuff_list = 0;
    fsound_num = 0;
    dont_delete_texb = keep_textures;
    std::ranges::fill(load_img, nullptr);
    tex_anime_list = 0;

    if (chara == nullptr) {
        return;
    }

    int   file_size = 0;
    char *config_text = reinterpret_cast<char *>(GetPackFile(pack, name, &file_size));

    if (config_text == nullptr) {
        std::println("not found {}", name);
        return;
    }

    config_file = config_text;
    config_file_size = file_size;
    input_str input;
    input.data = config_file;
    input.size = file_size;
    input.pos = 0;
    PreProcess(input);
    SkipSpace(input);

    auto                             arg_storage = std::make_unique<std::array<std::array<char, ARGUMENT_SIZE>, ARGUMENT_MAX>>();
    std::array<void *, ARGUMENT_MAX> argv{};

    for (int i = 0; i < ARGUMENT_MAX; i++) {
        argv[i] = (*arg_storage)[i].data();
    }

    int command = 0;

    while (SearchCommand(input, &command)) {
        if (command < COMMAND_COUNT && command >= 0) {
            const int arg_status = GetArg(input, Command[command].arg_types, argv.data());

            if (arg_status == 0) {
                return;
            }

            if (arg_status < 0) {
                std::println("error!! at {}", Command[command].name);
            }

            CommandExe[command](argv.data());
        }
    }
}

namespace {

void CommandVERTEX_ANIME(void **argv) {
    vertex_anime = *static_cast<int *>(argv[0]);
}

void CommandSHADOW_VERTEX_ANIME(void **argv) {
    shadow_vertex_anime = *static_cast<int *>(argv[0]);
}

void CommandMODEL(void **argv) {
    character->Initialize();
    int visual_type = def_vtype;

    if (vertex_anime != 0) {
        visual_type = 6;
    }

    if ((def_vtype & 16) != 0) {
        visual_type = 18;
    }

    char  *model_name = static_cast<char *>(argv[0]);
    u_int *mds_file = GetPackFile(pack_data, model_name, nullptr);

    if (mds_file == nullptr) {
        std::println("not found {}", model_name);
        return;
    }

    if (alloc_dbuff_list > 0 || alloc_mdt_list > 0) {
        // Both lists end with a null entry.
        std::array<char *, ALLOC_NAME_MAX + 1> mdt_names{};
        std::array<char *, ALLOC_NAME_MAX + 1> dbuff_names{};

        for (int i = 0; i < alloc_mdt_list; i++) {
            mdt_names[i] = alloc_mdt[i].data();
        }

        for (int i = 0; i < alloc_dbuff_list; i++) {
            dbuff_names[i] = alloc_dbuff[i].data();
        }

        character->frame = LoadMDSFile(mds_file, mds_buffer, 0, dbuff_names.data(), mdt_names.data());
    } else {
        character->frame = LoadMDSFile(mds_file, mds_buffer, visual_type, nullptr, nullptr);
    }

    character->config = reinterpret_cast<char *>(mds_buffer->Alloc((config_file_size >> 4) + 1));
    character->config_size = config_file_size;
    std::memcpy(character->config, config_file, config_file_size);
}

void CommandSHADOW_MODEL(void **argv) {
    int visual_type = 8;

    if (shadow_vertex_anime != 0) {
        visual_type = 14;
    }

    char  *model_name = static_cast<char *>(argv[0]);
    u_int *mds_file = GetPackFile(pack_data, model_name, nullptr);

    if (mds_file == nullptr) {
        std::println("not found {}", model_name);
    } else {
        character->shadow_frame = LoadMDSFile(mds_file, mds_buffer, visual_type, nullptr, nullptr);
    }
}

void CommandMOTION(void **argv) {
    if (character->frame == nullptr) {
        return;
    }

    now_motion_data = *static_cast<int *>(argv[0]);
    character->ClearEvent(now_motion_data);

    if (now_motion_data < 0 || now_motion_data >= CHARA_MOTION_MAX) {
        return;
    }

    if (now_motion_data > 0) {
        character->motion[now_motion_data] = reinterpret_cast<tagMOTION_TYPE *>(character->motion_storage[now_motion_data].storage);
    }

    tagMOTION_TYPE *motion = character->motion[now_motion_data];
    std::memset(motion, 0, sizeof(tagMOTION_TYPE));
    char            *motion_name = static_cast<char *>(argv[1]);
    char            *bone_name = static_cast<char *>(argv[2]);
    char            *weight_name = static_cast<char *>(argv[3]);
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

    for (MOTION_FILE_INFO &file : files) {
        if (file.size == 0) {
            file.name = nullptr;
        }
    }

    CreateAnimeDataEX(motion, buffer, files);

    if (character->frame_info == nullptr) {
        AnimeDataInit(character->frame, motion, mds_buffer, &character->frame_info);
    }

    motion->frame_info = character->frame_info;
    motion_info_num = 0;
    key_no = 0;
    motion_info = nullptr;
}

void CommandSHADOW_MOTION(void **argv) {
    if (character->shadow_frame == nullptr) {
        return;
    }

    if (now_motion_data < 0 || now_motion_data >= CHARA_MOTION_MAX) {
        return;
    }

    if (now_motion_data > 0) {
        character->shadow_motion[now_motion_data] = reinterpret_cast<tagMOTION_TYPE *>(character->shadow_motion_storage[now_motion_data].storage);
    }

    tagMOTION_TYPE *motion = character->shadow_motion[now_motion_data];
    std::memset(motion, 0, sizeof(tagMOTION_TYPE));
    shadow_motion = 1;
    char            *motion_name = static_cast<char *>(argv[0]);
    char            *bone_name = static_cast<char *>(argv[1]);
    char            *weight_name = static_cast<char *>(argv[2]);
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

    if (bone_name[0] == '\0') {
        files[0].name = nullptr;
    }

    if (weight_name[0] == '\0') {
        files[2].name = nullptr;
    }

    if (files[0].data == nullptr && files[1].data == nullptr && files[2].data == nullptr) {
        return;
    }

    CreateAnimeDataEX(motion, buffer, files);

    if (character->shadow_frame_info == nullptr) {
        AnimeDataInit(character->shadow_frame, motion, mds_buffer, &character->shadow_frame_info);
    }

    motion->frame_info = character->shadow_frame_info;
}

void CommandKEY_START(void **argv) {
    if (now_motion_data < 0 || now_motion_data >= CHARA_MOTION_MAX) {
        return;
    }

    key_start = *static_cast<int *>(argv[0]);
    character->motion_start[now_motion_data] = key_start;
}

void CommandKEY(void **argv) {
    // Keys are written into the free space of the motion buffer and only allocated at MOTION_END.
    if (motion_info == nullptr) {
        motion_info = reinterpret_cast<tagMOTION_KEY *>(&buffer->base[static_cast<std::ptrdiff_t>(buffer->used) * 16]);
    }

    tagMOTION_KEY &key = motion_info[key_no];
    key.start = *static_cast<int *>(argv[0]);
    key.end = *static_cast<int *>(argv[1]);
    key.speed = *static_cast<float *>(argv[2]);
    key_no++;
    motion_info_num = key_no;
}

void CommandMOTION_END(void ** /*argv*/) {
    if (now_motion_data < 0 || now_motion_data >= CHARA_MOTION_MAX || motion_info == nullptr) {
        return;
    }

    tagMOTION_TYPE *motion = character->motion[now_motion_data];
    buffer->Alloc(static_cast<int>(((static_cast<unsigned int>(motion_info_num + 1) << 4) >> 4) + 1));
    motion_info[motion_info_num].start = -1;
    motion_info[motion_info_num].end = -1;
    motion_info[motion_info_num].speed = -1.0f;
    motion->motion_info = motion_info;
    motion->state.time = static_cast<float>(motion->motion_info->start);
    motion->state.blend_step = 0.1f;
    motion->state.motion_no = 0;
    motion->state.playing_no = 0;
    motion->state.blending = false;

    if (shadow_motion != 0) {
        tagMOTION_TYPE *shadow_motion_type = character->shadow_motion[now_motion_data];
        shadow_motion_type->motion_info = motion_info;
        shadow_motion_type->state.time = static_cast<float>(shadow_motion_type->motion_info->start);
        shadow_motion_type->state.blend_step = 0.1f;
        shadow_motion_type->state.motion_no = 0;
        shadow_motion_type->state.playing_no = 0;
        shadow_motion_type->state.blending = false;
    }

    character->motion_end[now_motion_data] = key_start + key_no;
}

void CommandCLOTH(void **argv) {
    int    size = 0;
    u_int *cloth_file = GetPackFile(pack_data, static_cast<char *>(argv[0]), &size);

    if (cloth_file != nullptr) {
        input_str input;
        input.data = reinterpret_cast<char *>(cloth_file);
        input.size = size;
        input.pos = 0;
        character->cloth[cloth_cnt] = InitCloth(static_cast<CFrameVu1 *>(character->frame), input, buffer);
        cloth_cnt++;
    }
}

void CommandBODY_SIZE(void **argv) {
    character->body_height = *static_cast<float *>(argv[0]);
    character->body_width = *static_cast<float *>(argv[1]);
    character->body_depth = *static_cast<float *>(argv[2]);
}

void CommandALLOC_MDT(void **argv) {
    if (alloc_mdt_list < ALLOC_NAME_MAX) {
        alloc_mdt[alloc_mdt_list++] = static_cast<char *>(argv[0]);
    }
}

void CommandALLOC_DBUFF(void **argv) {
    if (alloc_dbuff_list < ALLOC_NAME_MAX) {
        alloc_dbuff[alloc_dbuff_list++] = static_cast<char *>(argv[0]);
    }
}

void CommandALLOC_SHADOW_MDT(void **argv) {
    if (alloc_smdt_list < ALLOC_NAME_MAX) {
        alloc_smdt[alloc_smdt_list++] = static_cast<char *>(argv[0]);
    }
}

void CommandALLOC_SHADOW_DBUFF(void **argv) {
    if (alloc_sdbuff_list < ALLOC_NAME_MAX) {
        alloc_sdbuff[alloc_sdbuff_list++] = static_cast<char *>(argv[0]);
    }
}

} // namespace

void CommandIMG(void **argv) {
    if (img_buffer == nullptr) {
        return;
    }

    const int slot = *static_cast<int *>(argv[0]);
    if (slot < 0 || slot >= std::ssize(load_img)) {
        return;
    }

    int    size = 0;
    u_int *image_file = GetPackFile(pack_data, static_cast<char *>(argv[1]), &size);
    if (image_file == nullptr) {
        return;
    }

    DataReadSource source;
    const bool     has_source = size > 0 && DataReadSourceRange(image_file, static_cast<std::size_t>(size), source);
    load_img[slot] = reinterpret_cast<u_int *>(img_buffer->Alloc((size >> 4) + 1));
    std::memcpy(load_img[slot], image_file, static_cast<std::size_t>(size));

    if (has_source) {
        DataReadRegisterCopiedSource(load_img[slot], static_cast<std::size_t>(size), img_buffer, source);
    }
}

namespace {

void CommandIMG_END(void ** /*argv*/) {
    // One more entry than there are images, for the terminator.
    std::array<LOADTEXTURE_INFO2, 5> textures{};
    int                              texture_count = 0;

    if (character->images[0] != nullptr && load_img[0] == nullptr) {
        load_img[0] = character->images[0];
    }

    for (int i = 0; i < 4; i++) {
        character->images[i] = load_img[i];

        if (load_img[i] != nullptr) {
            textures[texture_count].block_no = texture_block;
            textures[texture_count].mipmap = 0;
            textures[texture_count].name = reinterpret_cast<char *>(load_img[i]);
            texture_count++;
        }
    }

    textures[texture_count].block_no = 0;
    textures[texture_count].mipmap = 0;
    textures[texture_count].name = nullptr;

    if (texture_count > 0) {
        if (dont_delete_texb == 0) {
            TexManager.DeleteTextureBlock(texture_block);
        }

        TexManager.LoadTextureBlockEX(texture_block, textures.data());
    }

    if (config_file != nullptr) {
        character->tex_anime.LoadCFGFile(config_file, config_file_size);
    }
}

void CommandFOOT(void **argv) {
    character->SetFootSound(*static_cast<float *>(argv[0]), *static_cast<float *>(argv[1]), now_motion_data);
}

void CommandEVENT(void **argv) {
    if (*static_cast<int *>(argv[1]) >= 0) {
        character->SetEvent(*static_cast<float *>(argv[0]), *static_cast<int *>(argv[1]), *static_cast<int *>(argv[2]), now_motion_data);
    }
}

bool IsDigit(char letter) {
    return letter >= '0' && letter <= '9';
}

/**
 * Reads the arguments of one command into argv, converting each to its type.
 * Returns 1 on success, 0 at the end of the text and -1 on a malformed argument.
 */
int GetArg(input_str &input, const int *arg_types, void **argv) {
    if (arg_types[0] < 0) {
        return 1;
    }

    if (!SkipSpace(input)) {
        return 0;
    }

    int arg_count = 0;

    while (arg_types[arg_count] >= 0) {
        arg_count++;
    }

    std::string word;

    for (int i = 0; i < arg_count; i++) {
        if (!SkipSpace(input)) {
            return 0;
        }

        word.clear();
        int ch = 0;

        while (true) {
            if (input.get(&ch) == 0) {
                return 0;
            }

            if (ch == ',' || !CheckChar(static_cast<char>(ch))) {
                break;
            }

            word.push_back(static_cast<char>(ch));
        }

        // A NUL byte ends the word, as it did when words were C strings.
        const std::string_view text = std::string_view(word).substr(0, word.find('\0'));

        switch (arg_types[i]) {
            case SCRIPT_ARGUMENT_FLOAT:
                if (!std::ranges::all_of(text, [](char letter) { return IsDigit(letter) || letter == '.' || letter == '-'; })) {
                    return -1;
                }

                *static_cast<float *>(argv[i]) = static_cast<float>(std::atof(word.c_str()));
                break;
            case SCRIPT_ARGUMENT_INTEGER:
                if (!std::ranges::all_of(text, IsDigit)) {
                    return -1;
                }

                *static_cast<int *>(argv[i]) = std::atoi(word.c_str());
                break;
            case SCRIPT_ARGUMENT_STRING: {
                if (!text.starts_with('"')) {
                    return -1;
                }

                const std::size_t close = text.find('"', 1);

                if (close == std::string_view::npos) {
                    return -1;
                }

                const std::string_view contents = text.substr(1, std::min(close - 1, ARGUMENT_SIZE - 1));
                char                  *destination = static_cast<char *>(argv[i]);
                destination[contents.copy(destination, contents.size())] = '\0';
                break;
            }
            default:
                return -1;
        }
    }

    return 1;
}

bool SearchCommand(input_str &input, int *command) {
    if (!SkipSpace(input)) {
        return false;
    }

    std::string word;
    int         ch = 0;

    while (true) {
        // A word already started keeps reading past the end of the text until it meets a space.
        if (input.get(&ch) == 0 && word.empty()) {
            return false;
        }

        if (!CheckChar(static_cast<char>(ch))) {
            break;
        }

        word.push_back(static_cast<char>(ch));
    }

    // A NUL byte ends the word, as it did when words were C strings.
    const std::string_view text = std::string_view(word).substr(0, word.find('\0'));

    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (text == Command[i].name) {
            *command = i;
            return true;
        }
    }

    *command = UNKNOWN_COMMAND;
    return true;
}

bool SkipSpace(input_str &input) {
    while (input.pos < input.size && !CheckChar(input.data[input.pos])) {
        input.pos++;
    }

    return input.pos < input.size;
}

/**
 * Returns whether ch is not a space, tab, line feed or carriage return.
 */
bool CheckChar(char ch) {
    return ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r';
}

/**
 * Blanks out // and block comments in place.
 */
void PreProcess(input_str &input) {
    char *text = input.data;
    int   pos = 0;

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

} // namespace

void CCharacter::StopCloth(int /*unused*/) {
    for (int i = 0; i < 4; i++) {
        if (cloth[i] != nullptr) {
            cloth[i]->stop = true;
        }
    }
}

void StretchBind2(float *point_a, float *point_b, float *spring) {
    float delta[3] = {point_a[0] - point_b[0], point_a[1] - point_b[1], point_a[2] - point_b[2]};
    float length_squared = delta[0] * delta[0];
    length_squared = length_squared + delta[1] * delta[1];
    length_squared = length_squared + delta[2] * delta[2];
    float inverse_scale = vu0::Rsqrt(spring[0], length_squared);
    float pull_a = spring[1];
    float pull_b = spring[2];

    for (int i = 0; i < 3; i++) {
        float slack = delta[i] - vu0::Mul(delta[i], inverse_scale);
        point_a[i] = point_a[i] - slack * pull_a;
        point_b[i] = point_b[i] + slack * pull_b;
    }
}

float vuabs(float *vector) {
    return vu0::Sqrt(vu0::LengthSquared(vector));
}

} // namespace dc::game
