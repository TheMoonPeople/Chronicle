#include "itemdata.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dranmapfield.hpp"
#include "dun/gameloop.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "dungeonparts.hpp"
#include "frame.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "objanime.hpp"
#include "texture.hpp"
#include "textureanime.hpp"
#include "userstatus.hpp"

namespace dc::game {

using namespace dc::data;

namespace {

/** Parsed string argument for each item-definition command. */
char argStrBuff[640][36];

/** Command identifier and numeric arguments for each parsed definition. */
float argValBuff[640][36];

/** Number of item-definition commands parsed so far. */
int argLevel;

/** Command number and argument kinds of the GRD_IMG directive. */
int TEIGI_GRD_IMG[] = {0, 2, 0, 1};

/** Command number and argument kinds of the FIRE_IMG directive. */
int TEIGI_FIRE_IMG[] = {1, 1, 0};

} // namespace

char          pathName[256];
OBJ_ANIME_SEQ FrameObjAnim[48];
int           FrameObjAnimCnt;
int           VERSION_VOL;
float         run_speed = 1.0f;

int TEIGI_SET_PATH[] = {9, 1, 0};
int TEIGI_MINIMAP_IMG[] = {21, 1, 0};

namespace {

/** A model or collision file already loaded, keyed by its resource path. */
struct LoadedFrame {
    std::string path;  /**< Resource path the frame was loaded from. */
    CFrame     *frame; /**< The loaded frame. */
};

/** Dungeon-part models already loaded. */
std::vector<LoadedFrame> loadedModels;

/** Dungeon-part collision models already loaded. */
std::vector<LoadedFrame> loadedCollisions;

/** Parser integrity marker changed by malformed definition data. */
int errFlag2;

/** Bytes of the item definition file that were read. */
int teigiFileSize;

/** Number of dungeon-part model directives loaded so far. */
int nowPartsCnt;

/** Debug mode requested by the DebugFlag directive. */
int debugModeFlag = 1;

// Each format starts with the command number and the argument count, followed by one ArgumentKind per argument.
constexpr int TEIGI_GRD[] = {2, 7, 0, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_FOG[] = {3, 7, 2, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_AMBIENT[] = {4, 3, 2, 1, 1};
constexpr int TEIGI_LIGHT_C[] = {5, 7, 2, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_BG_COL[] = {6, 3, 2, 1, 1};
constexpr int TEIGI_RUN_SPEED[] = {7, 1, 2};
constexpr int TEIGI_DebugFlag[] = {8, 1, 2};
constexpr int TEIGI_VIEWLEVEL[] = {10, 1, 2};
constexpr int TEIGI_DRANS_PARTS[] = {15, 1, 0};
constexpr int TEIGI_DRANS_COLS[] = {16, 1, 0};
constexpr int TEIGI_URA_AMBIENT[] = {17, 3, 2, 1, 1};
constexpr int TEIGI_URA_LIGHT_C[] = {18, 7, 2, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_URA_FOG[] = {19, 7, 2, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_URA_BG_COL[] = {20, 3, 2, 1, 1};
constexpr int TEIGI_PT_MARKER[] = {30, 7, 0, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_PT_NPC[] = {31, 10, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_PT_ROT[] = {32, 7, 0, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_PT_HEAL_ZONE[] = {33, 5, 2, 1, 1, 1, 1};
constexpr int TEIGI_PT_HIT_MARKER[] = {34, 7, 0, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_PT_SCALE[] = {35, 10, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_DEF_PATS[] = {50, 0};
constexpr int TEIGI_DEF_ENDS[] = {51, 0};
constexpr int TEIGI_PT_BASE[] = {52, 6, 0, 1, 1, 1, 1, 1};
constexpr int TEIGI_PT_COLS[] = {54, 2, 0, 1};
constexpr int TEIGI_PT_CAM[] = {55, 2, 0, 1};
constexpr int TEIGI_PT_FIRE[] = {56, 3, 2, 1, 1};
constexpr int TEIGI_PT_LIGHT[] = {57, 3, 2, 1, 1};
constexpr int TEIGI_PT_GLIGHT[] = {58, 1, 0};
constexpr int TEIGI_PT_WATER[] = {59, 9, 2, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr int TEIGI_BG_MODEL[] = {60, 1, 0};
constexpr int TEIGI_DUMMY_MODEL[] = {61, 1, 0};
constexpr int TEIGI_PT_TAKI00[] = {62, 1, 2};
constexpr int TEIGI_PT_DRAW_FLAG[] = {63, 2, 0, 1};
constexpr int TEIGI_DRAW_FLAG[] = {64, 2, 0, 1};

/** How checkArg reads one argument of an item-definition command. */
enum class ArgumentKind : int {
    String = 0,      /**< A double-quoted string of at most 31 characters. */
    CommaNumber = 1, /**< A comma followed by a number, ON or OFF. */
    Number = 2,      /**< A number, ON or OFF. */
};

/** An item-definition command name and its argument format. */
struct TeigiCommand {
    std::string_view name;   /**< Name the command starts with in the file. */
    const int       *format; /**< Command number and argument kinds. */
};

// Names are matched as prefixes in this order, so a name must come after any longer name it starts.
constexpr auto TEIGI_TABLE = std::to_array<TeigiCommand>({
    {"GRD_IMG",       TEIGI_GRD_IMG      },
    {"FIRE_IMG",      TEIGI_FIRE_IMG     },
    {"GRD",           TEIGI_GRD          },
    {"FOG",           TEIGI_FOG          },
    {"AMBIENT",       TEIGI_AMBIENT      },
    {"LIGHT_C",       TEIGI_LIGHT_C      },
    {"BG_COL",        TEIGI_BG_COL       },
    {"RUN_SPEED",     TEIGI_RUN_SPEED    },
    {"DebugFlag",     TEIGI_DebugFlag    },
    {"SET_PATH",      TEIGI_SET_PATH     },
    {"VIEWLEVEL",     TEIGI_VIEWLEVEL    },
    {"DEF_PATS",      TEIGI_DEF_PATS     },
    {"DEF_ENDS",      TEIGI_DEF_ENDS     },
    {"PT_BASE",       TEIGI_PT_BASE      },
    {"PT_COLS",       TEIGI_PT_COLS      },
    {"PT_CAM",        TEIGI_PT_CAM       },
    {"PT_FIRE",       TEIGI_PT_FIRE      },
    {"PT_LIGHT",      TEIGI_PT_LIGHT     },
    {"PT_GLIGHT",     TEIGI_PT_GLIGHT    },
    {"PT_WATER",      TEIGI_PT_WATER     },
    {"PT_TAKI00",     TEIGI_PT_TAKI00    },
    {"BG_MODEL",      TEIGI_BG_MODEL     },
    {"DUMMY_MODEL",   TEIGI_DUMMY_MODEL  },
    {"DRANS_PARTS",   TEIGI_DRANS_PARTS  },
    {"DRANS_COLS",    TEIGI_DRANS_COLS   },
    {"PT_DRAW_FLAG",  TEIGI_PT_DRAW_FLAG },
    {"PT_MARKER",     TEIGI_PT_MARKER    },
    {"DRAW_FLAG",     TEIGI_DRAW_FLAG    },
    {"URA_AMBIENT",   TEIGI_URA_AMBIENT  },
    {"URA_LIGHT_C",   TEIGI_URA_LIGHT_C  },
    {"URA_FOG",       TEIGI_URA_FOG      },
    {"URA_BG_COL",    TEIGI_URA_BG_COL   },
    {"MINIMAP_IMG",   TEIGI_MINIMAP_IMG  },
    {"PT_NPC",        TEIGI_PT_NPC       },
    {"PT_ROT",        TEIGI_PT_ROT       },
    {"PT_SCALE",      TEIGI_PT_SCALE     },
    {"PT_HEAL_ZONE",  TEIGI_PT_HEAL_ZONE },
    {"PT_HIT_MARKER", TEIGI_PT_HIT_MARKER},
});

bool StartsWith(const char *text, std::string_view prefix) {
    return std::string_view(text, prefix.size()) == prefix;
}

bool IsDigit(char character) {
    return character >= '0' && character <= '9';
}

const LoadedFrame *FindLoaded(const std::vector<LoadedFrame> &loaded, std::string_view path) {
    const auto entry = std::ranges::find(loaded, path, &LoadedFrame::path);
    return entry != loaded.end() ? &*entry : nullptr;
}

/**
 * Steps the item definition file past whitespace and comments.
 */
int skipSpace(const char *text, int pos) {
    for (; pos < teigiFileSize; pos++) {
        bool skipped = false;

        // The file is Shift-JIS, so a space can be the full-width one.
        if (StartsWith(&text[pos], "\x81\x40")) {
            pos++;
            skipped = true;
        }

        const char character = text[pos];

        if (character == ' ' || character == '\t') {
            skipped = true;
        }

        if (character == '\0') {
            pos++;
            skipped = true;
        }

        if (StartsWith(&text[pos], "//")) {
            while (text[pos] != '\0') {
                pos++;
            }

            pos++;
            skipped = true;
        }

        if (!skipped) {
            return pos;
        }
    }

    return teigiFileSize;
}

/**
 * Reads the arguments of one command out of the item definition file.
 */
int checkArg(const char *text, int pos, const int *format) {
    int cursor = pos;

    if (format[1] == 0) {
        return pos;
    }

    for (int argument = 0; argument < format[1]; argument++) {
        argValBuff[argLevel][0] = static_cast<float>(format[0]);

        const auto store_value = [argument](float value) {
            argValBuff[argLevel][argument + 1] = value;

            if (argument >= 32) {
                std::printf("************TAG OVER!!\n");
            }
        };

        switch (static_cast<ArgumentKind>(format[argument + 2])) {
            case ArgumentKind::String: {
                if (text[cursor] != '"') {
                    return -1;
                }

                cursor++;
                int length = 0;

                for (; length < 32; length++) {
                    if (text[cursor] == '"') {
                        argStrBuff[argLevel][length] = '\0';
                        cursor++;
                        break;
                    }

                    argStrBuff[argLevel][length] = text[cursor++];
                }

                if (length == 32) {
                    return -1;
                }

                cursor = skipSpace(text, cursor);
                break;
            }
            case ArgumentKind::CommaNumber:
                if (text[cursor] != ',') {
                    return -1;
                }

                cursor = skipSpace(text, cursor + 1);
                [[fallthrough]];
            case ArgumentKind::Number:
                if (StartsWith(&text[cursor], "ON")) {
                    store_value(1.0f);
                    cursor += 2;
                } else if (StartsWith(&text[cursor], "OFF")) {
                    store_value(0.0f);
                    cursor += 3;
                } else {
                    if (text[cursor] != '-' && !IsDigit(text[cursor])) {
                        return -1;
                    }

                    store_value(static_cast<float>(std::atof(&text[cursor])));
                    int length = 0;

                    for (; length < 32; length++) {
                        bool consumed = false;

                        if (text[cursor] == '-') {
                            cursor++;
                            consumed = true;
                        }

                        if (IsDigit(text[cursor])) {
                            cursor++;
                            consumed = true;
                        }

                        if (text[cursor] == '.') {
                            cursor++;
                            consumed = true;
                        }

                        if (!consumed) {
                            break;
                        }
                    }

                    if (length == 32) {
                        return -1;
                    }
                }

                cursor = skipSpace(text, cursor);
                break;
        }
    }

    return cursor;
}

} // namespace

int TEIGIAnalyz(char *path) {
    argLevel = 0;
    errFlag2 = 0x7E;
    BtRubyDoorKey = static_cast<int>(static_cast<float>(std::rand()) * 5.0f / 2.1474836e9f);

    if (BtRubyDoorKey < 0 || BtRubyDoorKey >= 5) {
        BtRubyDoorKey = 0;
    }

    if (selectMapNo == DUNGEON_SHIPWRECK && UserStatus->cur_floor == 8) {
        BtRubyDoorKey = 0;
    }

    MapModelBuffer.Align64();
    char *text = reinterpret_cast<char *>(&MapModelBuffer.base[static_cast<std::ptrdiff_t>(MapModelBuffer.used) * 16]);

    if (LoadFile(path, text, &teigiFileSize) == 0) {
        return 0;
    }

    wait_now_loading_vsync();
    MapModelBuffer.Alloc((teigiFileSize >> 4) + 1);
    MapModelBuffer.Align64();

    // The parser treats each source line as a null-terminated record.
    for (int i = 0; i < teigiFileSize; i++) {
        if (text[i] == '\r' && text[i + 1] == '\n') {
            text[i + 1] = '\0';
            text[i] = '\0';
        }
    }

    int pos = 0;

    while (pos < teigiFileSize) {
        pos = skipSpace(text, pos);
        const auto *const command = std::ranges::find_if(TEIGI_TABLE, [&](const TeigiCommand &entry) { return StartsWith(&text[pos], entry.name); });

        if (command == TEIGI_TABLE.end()) {
            std::printf("TAG SyntaxError!! >>%s\n", &text[pos]);
            exit__2(-1);
        }

        pos = skipSpace(text, pos + static_cast<int>(command->name.size()));

        if (command->format == TEIGI_DEF_PATS || command->format == TEIGI_DEF_ENDS) {
            argValBuff[argLevel][0] = static_cast<float>(command->format[0]);
        } else {
            pos = checkArg(text, pos, command->format);

            if (pos == -1) {
                std::printf("def Error:%.*s\n", static_cast<int>(command->name.size()), command->name.data());
                exit__2(-1);
            }

            pos = skipSpace(text, pos);
        }

        argLevel++;

        if (argLevel >= 640) {
            std::printf("arg line level err!!\n");

            for (;;) {
            }
        }

        pos = skipSpace(text, pos);
    }

    if (errFlag2 != 0x7E) {
        std::printf("TAG MEM ERR2 !!\n");

        for (;;) {
        }
    }

    return teigiFileSize;
}

void TEIGIImgLoad(u_int *pack, CDataAlloc2<1> *arena) {
    int ground_count = 0;

    pathName[0] = '\0';
    LOADTEXTURE_INFO2 ground_images[5] = {
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0}
    };
    LOADTEXTURE_INFO2 fire_images[3] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 14, .mipmap = 0},
        {.name = "",                                          .block_no = 14, .mipmap = 0},
        {.name = "",                                          .block_no = 0,  .mipmap = 0}
    };
    LOADTEXTURE_INFO2 minimap_images[2] = {
        {.name = "", .block_no = 31, .mipmap = 0},
        {.name = "", .block_no = 0,  .mipmap = 0}
    };
    int size = 0;

    for (int i = 0; i < 4; i++) {
        TexManager.DeleteTextureBlock(i + 64);
    }
    TexManager.CleanUpTextureList();
    TexManager.CleanUpBuffer();

    for (int i = 0; i < argLevel; i++) {
        if (TEIGI_SET_PATH[0] == static_cast<int>(argValBuff[i][0])) {
            const std::string_view path_arg = argStrBuff[i];
            std::ranges::copy(path_arg, pathName);
            pathName[path_arg.size()] = '\0';
        }

        if (TEIGI_GRD_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            const std::string full_path = std::string(pathName) + argStrBuff[i];
            std::memcpy(argStrBuff[i], full_path.c_str(), full_path.size() + 1);

            u_char        *destination = &arena->base[static_cast<std::ptrdiff_t>(arena->used) * 16];
            u_int         *image = GetPackFile(pack, argStrBuff[i], &size);
            DataReadSource source;
            const bool     has_source = image != nullptr && size > 0 &&
                                        DataReadSourceRange(image, static_cast<std::size_t>(size), source);
            arena->Alloc((((size >> 6) + 1) << 6) >> 4);
            std::memcpy(destination, image, static_cast<std::size_t>(size));
            if (has_source) {
                DataReadRegisterCopiedSource(destination, static_cast<std::size_t>(size), arena, source);
            }
            ground_images[ground_count].name = reinterpret_cast<char *>(destination);
            ground_images[ground_count].block_no = 3;
            ground_images[ground_count].mipmap = static_cast<int>(argValBuff[i][2]);
            ground_count++;
            ground_images[ground_count].name = nullptr;

            TexManager.DeleteTextureBlock(3);
            TexManager.CleanUpTextureList();
            TexManager.CleanUpBuffer();
            TexManager.LoadTextureBlockEX(-1, ground_images);

            BtTexAnime.Initialize(BtTexAnimeData, 96);
            char *config = reinterpret_cast<char *>(GetPackFile(pack, const_cast<char *>("texanime.cfg"), &size));
            if (config != nullptr) {
                for (int j = 0; j < 96; j++) {
                    BtTexAnimeData[j].Initialize();
                }
                BtTexAnime.LoadCFGFile(config, size);
            }
        }

        if (TEIGI_FIRE_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            std::println("{}", std::string_view(argStrBuff[i]));
            fire_images[1].name = reinterpret_cast<char *>(GetPackFile(pack, const_cast<char *>("fire.img"), &size));
            if (fire_images[1].name == nullptr) {
                exit__2(-1);
            }
            TexManager.DeleteTextureBlock(14);
            TexManager.CleanUpTextureList();
            TexManager.CleanUpBuffer();
            TexManager.LoadTextureBlock(-1, fire_images);
        }

        if (TEIGI_MINIMAP_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            std::string full_path = std::string(pathName) + argStrBuff[i];
            minimap_images[0].name = reinterpret_cast<char *>(GetPackFile(pack, full_path.data(), &size));
            if (minimap_images[0].name == nullptr) {
                exit__2(-1);
            }
            TexManager.DeleteTextureBlock(31);
            TexManager.CleanUpTextureList();
            TexManager.CleanUpBuffer();
            TexManager.LoadTextureBlock(-1, minimap_images);
        }
    }
}

void TEIGIMdsLoad(u_int *pack, int reuse_only) {
    CFrame     *current_collision = nullptr;
    CFrame     *current_model = nullptr;
    CFrameAttr  attr;
    std::string full_path;
    int         part_frame = 0;

    attr.clip_depth = 20.0f;
    attr.clip_enable = true;
    attr.program_option = 0;
    FrameObjAnimCnt = 0;

    for (OBJ_ANIME_SEQ &sequence : FrameObjAnim) {
        sequence.property = OBJ_ANIME_PROPERTY_NONE;
    }

    nowPartsCnt = 0;

    if (reuse_only == 0) {
        loadedModels.clear();
        loadedCollisions.clear();
    }

    pathName[0] = '\0';

    const auto add_fire = [](const float (&args)[36], int param) {
        CDungeonParts &part = NowDngMap->parts[nowPartsCnt];
        const float    z = args[3];
        const float    y = args[2];
        const float    x = args[1];

        if (part.fire_num < 6) {
            part.fire_pos[part.fire_num][0] = x;
            part.fire_pos[part.fire_num][1] = y;
            part.fire_pos[part.fire_num][2] = z;
            part.fire_param[part.fire_num] = static_cast<s8>(param);
            part.fire_num++;
        }
    };

    for (int line = 0; line < argLevel; line++) {
        const float (&args)[36] = argValBuff[line];
        char (&string_arg)[36] = argStrBuff[line];
        const int command = static_cast<int>(args[0]);

        if (TEIGI_DebugFlag[0] == command) {
            debugModeFlag = static_cast<int>(args[1]);
            std::printf("debug = %d\n", debugModeFlag);
        }

        if (TEIGI_RUN_SPEED[0] == command) {
            run_speed = args[1];
        }

        if (TEIGI_LIGHT_C[0] == command) {
            const int light_no = static_cast<int>(args[7]);
            float     direction[4];
            direction[0] = args[1];
            direction[1] = args[2];
            direction[2] = args[3];
            sceVu0Normalize(direction, direction);
            main_light[0][light_no - 1] = direction[0];
            main_light[1][light_no - 1] = direction[1];
            main_light[2][light_no - 1] = direction[2];
            main_lightcolor[light_no - 1][0] = args[4];
            main_lightcolor[light_no - 1][1] = args[5];
            main_lightcolor[light_no - 1][2] = args[6];
        }

        if (TEIGI_AMBIENT[0] == command) {
            main_ambientlight[0] = args[1];
            main_ambientlight[1] = args[2];
            main_ambientlight[2] = args[3];
        }

        if (TEIGI_URA_LIGHT_C[0] == command) {
            const int light_no = static_cast<int>(args[7]);
            float     direction[4];
            direction[0] = args[1];
            direction[1] = args[2];
            direction[2] = args[3];
            sceVu0Normalize(direction, direction);
            sub_light[0][light_no - 1] = direction[0];
            sub_light[1][light_no - 1] = direction[1];
            sub_light[2][light_no - 1] = direction[2];
            sub_lightcolor[light_no - 1][0] = args[4];
            sub_lightcolor[light_no - 1][1] = args[5];
            sub_lightcolor[light_no - 1][2] = args[6];
        }

        if (TEIGI_URA_AMBIENT[0] == command) {
            sub_ambientlight[0] = args[1];
            sub_ambientlight[1] = args[2];
            sub_ambientlight[2] = args[3];
        }

        if (TEIGI_FOG[0] == command) {
            main_fogRate[0] = args[1];
            main_fogRate[1] = args[2];
            main_fogRate[2] = args[6];
            main_fogRate[3] = args[7];
            main_fogColor[0] = static_cast<u8>(args[3]);
            main_fogColor[1] = static_cast<u8>(args[4]);
            main_fogColor[2] = static_cast<u8>(args[5]);
            attr.fog_enable = true;
        }

        if (TEIGI_BG_COL[0] == command) {
            main_bgColor[0] = static_cast<u8>(args[1]);
            main_bgColor[1] = static_cast<u8>(args[2]);
            main_bgColor[2] = static_cast<u8>(args[3]);
            MGSetBGColor(static_cast<float>(main_bgColor[0]), static_cast<float>(main_bgColor[1]), static_cast<float>(main_bgColor[2]), 128.0f);
        }

        if (TEIGI_URA_BG_COL[0] == command) {
            sub_bgColor[0] = static_cast<u8>(args[1]);
            sub_bgColor[1] = static_cast<u8>(args[2]);
            sub_bgColor[2] = static_cast<u8>(args[3]);
        }

        if (TEIGI_URA_FOG[0] == command) {
            sub_fogRate[0] = args[1];
            sub_fogRate[1] = args[2];
            sub_fogRate[2] = args[6];
            sub_fogRate[3] = args[7];
            sub_fogColor[0] = static_cast<u8>(args[3]);
            sub_fogColor[1] = static_cast<u8>(args[4]);
            sub_fogColor[2] = static_cast<u8>(args[5]);
            attr.fog_enable = true;
        }

        if (TEIGI_SET_PATH[0] == command) {
            std::strcpy(pathName, string_arg);
        }

        if (TEIGI_VIEWLEVEL[0] == command) {
            NowDngMap->draw_dist_scale = args[1];
        }

        if (TEIGI_BG_MODEL[0] == command) {
            int    size = 0;
            u_int *data = GetPackFile(pack, string_arg, &size);

            if (data == nullptr) {
                std::printf("ERR = %s\n", full_path.c_str());
            }

            CFrame      *frame = LoadMDSFile(data, &MapModelBuffer, 0, nullptr, nullptr);
            CDungeonMap *map = NowDngMap;

            if (map->bg_model_num < 6) {
                map->bg_model[map->bg_model_num] = frame;
                map->bg_model_num++;
            }
        }

        if (TEIGI_DRAW_FLAG[0] == command) {
            CFrame *target = NowDngMap->GetFrameSearch(string_arg);

            if (target == nullptr) {
                std::printf("**** DFLAG:%s NotFound ****\n", string_arg);

                while (true) {
                }
            }

            const int draw_on = static_cast<int>(args[2]) != 0 ? 1 : 2;
            target->attr.draw_on = static_cast<short>(draw_on);
            target->flags = draw_on;
        }

        if (TEIGI_DUMMY_MODEL[0] == command) {
            int    size = 0;
            u_int *data = GetPackFile(pack, string_arg, &size);

            if (data == nullptr) {
                std::printf("ERR = %s\n", full_path.c_str());
            }

            CFrame *frame = LoadMDSFile(data, &MapModelBuffer, 0, nullptr, nullptr);
            frame->SetAttr(attr, 1, 0x40);
            SetFrameAttr(frame, 1);
            CDungeonMap *map = NowDngMap;

            if (map->dummy_frame_num < 3) {
                map->dummy_frame[map->dummy_frame_num] = frame;
                map->dummy_frame_num++;
            }
        }

        if (TEIGI_DRANS_PARTS[0] == command) {
            int    size = 0;
            u_int *data = GetPackFile(pack, string_arg, &size);

            if (data == nullptr) {
                std::printf("ERR = %s\n", full_path.c_str());
            }

            std::printf("load dran %s\n", string_arg);
            NowDranMapField->LoadModel(data, &MapModelBuffer);
        }

        if (TEIGI_DRANS_COLS[0] == command) {
            int    size = 0;
            u_int *data = GetPackFile(pack, string_arg, &size);

            if (data == nullptr) {
                std::printf("ERR = %s\n", full_path.c_str());
            }

            NowDranMapField->LoadCollision(data, &MapModelBuffer);
        }

        if (TEIGI_DEF_PATS[0] == command) {
            part_frame = 0;
        }

        if (TEIGI_DEF_ENDS[0] == command) {
            nowPartsCnt++;

            if (nowPartsCnt >= 72) {
                std::printf("parts cnt over\n");

                while (true) {
                }
            }
        }

        if (TEIGI_PT_BASE[0] == command) {
            if (part_frame >= 6) {
                std::printf("[%d]parts num over !!\n", nowPartsCnt);

                while (true) {
                }
            }

            if (loadedModels.size() >= 72) {
                std::printf("filePathNumOver = %d\n", static_cast<int>(loadedModels.size()));

                while (true) {
                }
            }

            full_path.assign(pathName);
            full_path += string_arg;

            if (const LoadedFrame *loaded = FindLoaded(loadedModels, full_path); loaded != nullptr) {
                current_model = loaded->frame;
            } else {
                if (reuse_only != 0) {
                    std::printf("MISS DATA %s\n", full_path.c_str());
                    exit__2(-1);
                }

                if (pack != nullptr) {
                    int    size = 0;
                    u_int *data = GetPackFile(pack, full_path.data(), &size);

                    if (data == nullptr) {
                        std::printf("ERR = %s\n", full_path.c_str());
                    }

                    current_model = LoadMDSFile(data, &MapModelBuffer, 0, nullptr, nullptr);
                } else {
                    LoadFile(full_path.data(), read_buffer, nullptr);
                    wait_now_loading_vsync();
                    current_model = LoadMDSFile(read_buffer, &MapModelBuffer, 0, nullptr, nullptr);
                }

                current_model->SetAttr(attr, 1, 0x40);
                SetFrameAttr(current_model, 1);
                loadedModels.push_back({full_path, current_model});
            }

            const int   turn = static_cast<int>(args[6]);
            const float z = args[5];
            const float y = args[4];
            const float x = args[3];
            const int   direction = static_cast<int>(args[2]);
            const int   parts_no = nowPartsCnt;

            if (parts_no < 72) {
                CDungeonParts &part = NowDngMap->parts[parts_no];
                part.frame[part_frame] = current_model;
                part.frame_offset[part_frame][0] = 10.0f * x;
                part.frame_offset[part_frame][1] = 10.0f * y;
                part.frame_offset[part_frame][2] = 10.0f * z;
                part.frame_offset[part_frame][3] = 1.0f;
                part.frame_turn[part_frame] = static_cast<float>(turn);
                part.direction_offset = static_cast<s16>(direction);
            }

            part_frame++;
        }

        if (TEIGI_PT_COLS[0] == command) {
            if (loadedCollisions.size() >= 72) {
                std::printf("colNumOver = %d\n", static_cast<int>(loadedCollisions.size()));

                while (true) {
                }
            }

            full_path.assign(pathName);
            full_path += string_arg;

            if (const LoadedFrame *loaded = FindLoaded(loadedCollisions, full_path); loaded != nullptr) {
                current_collision = loaded->frame;
            } else {
                if (reuse_only != 0) {
                    std::printf("MISS COL DATA %s\n", full_path.c_str());
                    exit__2(-1);
                }

                if (pack != nullptr) {
                    int    size = 0;
                    u_int *data = GetPackFile(pack, full_path.data(), &size);

                    if (data == nullptr) {
                        std::printf("ERR = %s\n", full_path.c_str());
                    }

                    current_collision = LoadCollisionFile(data, &MapModelBuffer);
                } else {
                    LoadFile(full_path.data(), read_buffer, nullptr);
                    wait_now_loading_vsync();
                    current_collision = LoadCollisionFile(read_buffer, &MapModelBuffer);
                }

                current_collision->flags = 1;

                if (current_collision->SearchFrame(const_cast<char *>("cdoor_al")) != nullptr) {
                    std::printf("****** cdoor_al\n");
                }

                loadedCollisions.push_back({full_path, current_collision});
            }

            const int      turn = static_cast<int>(args[2]);
            CDungeonParts &part = NowDngMap->parts[nowPartsCnt];
            part.collision = current_collision;
            part.collision_turn = static_cast<s16>(turn);
        }

        if (TEIGI_PT_CAM[0] == command) {
            CFrame *collision = nullptr;

            if (loadedCollisions.size() >= 72) {
                std::printf("colNumOver = %d\n", static_cast<int>(loadedCollisions.size()));

                while (true) {
                }
            }

            full_path.assign(pathName);
            full_path += string_arg;

            if (const LoadedFrame *loaded = FindLoaded(loadedCollisions, full_path); loaded != nullptr) {
                collision = loaded->frame;
            } else {
                if (reuse_only != 0) {
                    std::printf("MISS CAM DATA %s\n", full_path.c_str());
                    exit__2(-1);
                }

                if (pack != nullptr) {
                    int    size = 0;
                    u_int *data = GetPackFile(pack, full_path.data(), &size);

                    if (data == nullptr) {
                        std::printf("ERR = %s\n", full_path.c_str());

                        while (true) {
                        }
                    }

                    collision = LoadCollisionFile(data, &MapModelBuffer);
                } else {
                    LoadFile(full_path.data(), read_buffer, nullptr);
                    wait_now_loading_vsync();
                    collision = LoadCollisionFile(read_buffer, &MapModelBuffer);
                }

                collision->flags = 1;
                loadedCollisions.push_back({full_path, collision});
            }

            const int      turn = static_cast<int>(args[2]);
            CDungeonParts &part = NowDngMap->parts[nowPartsCnt];
            part.camera_collision = collision;
            part.camera_collision_turn = static_cast<s16>(turn);
        }

        if (TEIGI_PT_DRAW_FLAG[0] == command) {
            CFrame *target = current_model->SearchFrame(string_arg);

            if (target == nullptr) {
                std::printf("**** DFLAG:%s NotFound ****\n", string_arg);

                while (true) {
                }
            }

            const int draw_on = static_cast<int>(args[2]) != 0 ? 1 : 2;
            target->attr.draw_on = static_cast<short>(draw_on);
            target->flags = draw_on;
        }

        if (TEIGI_PT_GLIGHT[0] == command) {
            sceVu0FVECTOR range = {80.0f, 80.0f, 80.0f, 80.0f};
            sceVu0FVECTOR speed = {128.0f, 128.0f, 128.0f, 128.0f};

            if (FrameObjAnimCnt >= 48) {
                std::printf("over frame anim!\n");
            } else {
                OBJ_ANIME_SEQ &sequence = FrameObjAnim[FrameObjAnimCnt];
                std::strcpy(sequence.frame_name, string_arg);
                sequence.property = OBJ_ANIME_PROPERTY_COLOR;
                sequence.mode = OBJ_ANIME_MODE_RANDOM_UNIFORM;
                sceVu0CopyVector(sequence.from, range);
                sceVu0CopyVector(sequence.to, speed);
                InitObjAnime(current_model, &sequence);
                FrameObjAnimCnt++;
            }
        }

        if (TEIGI_PT_LIGHT[0] == command) {
            add_fire(args, 2);
        }

        if (TEIGI_PT_HEAL_ZONE[0] == command) {
            float position[4];
            position[0] = 10.0f * args[1];
            position[1] = 10.0f * args[2];
            position[2] = 10.0f * args[3];
            const float radius = 10.0f * args[4];
            const float height = 10.0f * args[5];
            NowDngMap->parts[nowPartsCnt].SetHealZone(position, radius, height);
        }

        if (TEIGI_PT_WATER[0] == command) {
            sceVu0FVECTOR vertex0;
            sceVu0FVECTOR vertex1;
            sceVu0FVECTOR vertex2;
            sceVu0FVECTOR vertex3;
            vertex2[0] = 10.0f * args[1];
            vertex0[0] = 10.0f * args[1];
            vertex3[0] = 10.0f * args[4];
            vertex1[0] = 10.0f * args[4];
            vertex0[1] = vertex1[1] = vertex2[1] = vertex3[1] = 10.0f * args[2];
            vertex0[2] = vertex1[2] = 10.0f * args[3];
            vertex2[2] = vertex3[2] = 10.0f * args[6];
            const u8       red = static_cast<u8>(args[7]);
            const u8       green = static_cast<u8>(args[8]);
            const u8       blue = static_cast<u8>(args[9]);
            CDungeonParts &part = NowDngMap->parts[nowPartsCnt];
            part.water.used = 1;
            sceVu0CopyVector(part.water.vertex[0], vertex0);
            sceVu0CopyVector(part.water.vertex[1], vertex1);
            sceVu0CopyVector(part.water.vertex[2], vertex2);
            sceVu0CopyVector(part.water.vertex[3], vertex3);
            part.water.red = red;
            part.water.green = green;
            part.water.blue = blue;
            part.water.has_fall = 0;
        }

        // A marker inside a loaded collision model must name one of its frames; otherwise the slot stays free.
        const auto place_event = [&](CDungeonEvent *event) {
            if (current_collision != nullptr) {
                event->placement_frame = current_collision;
                event->trigger_frame = current_collision->SearchFrame(string_arg);

                if (event->trigger_frame == nullptr) {
                    std::printf("*********** not fount null == %s\n", string_arg);
                    return false;
                }
            }

            event->parts_id = nowPartsCnt;
            std::strcpy(event->name, string_arg);
            event->radius = args[2];
            event->script_no = static_cast<int>(args[3]);
            return true;
        };

        if (TEIGI_PT_MARKER[0] == command) {
            const int      marker_param = static_cast<int>(args[4]);
            const int      fade = static_cast<int>(args[5]);
            const int      extra_param = static_cast<int>(args[6]);
            const int      ext_memory = static_cast<int>(args[7]);
            CDungeonEvent *event = NowEventMan->SearchSlot();

            if (event != nullptr && place_event(event)) {
                event->hold = marker_param;
                event->fade = fade;
                event->chara_no = -1;
                event->key_id = -1;
                event->switch_on = extra_param;
                event->ext_mem = ext_memory;
                event->enabled = true;
            }
        }

        if (TEIGI_PT_HIT_MARKER[0] == command) {
            const int chara_no = static_cast<int>(args[4]);
            int       key = static_cast<int>(args[5]);

            if (key == -2) {
                key = BtRubyDoorKey;
            }

            const int      extra_param = static_cast<int>(args[6]);
            const int      ext_memory = static_cast<int>(args[7]);
            CDungeonEvent *event = NowEventMan->SearchSlot();

            if (event != nullptr && place_event(event)) {
                event->hold = 0;
                event->fade = 0;
                event->chara_no = chara_no;
                event->key_id = key;
                event->switch_on = extra_param;
                event->ext_mem = ext_memory;
                event->enabled = true;
            }
        }

        if (TEIGI_PT_ROT[0] == command) {
            if (FrameObjAnimCnt >= 48) {
                std::printf("over frame anim!\n");
            } else {
                OBJ_ANIME_SEQ &sequence = FrameObjAnim[FrameObjAnimCnt];
                sequence.Initialize();
                sequence.property = OBJ_ANIME_PROPERTY_ROTATION;
                sequence.mode = OBJ_ANIME_MODE_LINEAR;
                sequence.from[0] = args[2];
                sequence.from[1] = args[3];
                sequence.from[2] = args[4];
                sequence.step[0] = args[5];
                sequence.step[1] = args[6];
                sequence.step[2] = args[7];
                std::strcpy(sequence.frame_name, string_arg);
                InitObjAnime(current_model, &sequence);
                FrameObjAnimCnt++;
            }
        }

        if (TEIGI_PT_SCALE[0] == command) {
            if (FrameObjAnimCnt >= 48) {
                std::printf("over frame anim!\n");
            } else {
                OBJ_ANIME_SEQ &sequence = FrameObjAnim[FrameObjAnimCnt];
                sequence.Initialize();
                sequence.property = OBJ_ANIME_PROPERTY_SCALE;
                sequence.mode = OBJ_ANIME_MODE_PING_PONG;
                sequence.from[0] = args[2];
                sequence.from[1] = args[3];
                sequence.from[2] = args[4];
                sequence.to[0] = args[5];
                sequence.to[1] = args[6];
                sequence.to[2] = args[7];
                sequence.step[0] = args[8];
                sequence.step[1] = args[9];
                sequence.step[2] = args[10];
                std::strcpy(sequence.frame_name, string_arg);
                InitObjAnime(current_model, &sequence);
                FrameObjAnimCnt++;
            }
        }

        if (TEIGI_PT_NPC[0] == command) {
            const int     npc_no = static_cast<int>(args[2]);
            sceVu0FVECTOR position;
            sceVu0FVECTOR rotation;
            position[0] = args[3];
            position[1] = args[4];
            position[2] = args[5];
            position[3] = 1.0f;
            rotation[0] = args[6];
            rotation[1] = args[7];
            rotation[2] = args[8];

            for (int i = 0; i < 3; i++) {
                rotation[i] = (PI_SHORT / 180.0f) * rotation[i];

                if (rotation[i] > PI_SHORT) {
                    rotation[i] -= TWO_PI_SHORT;
                }

                if (rotation[i] < -PI_SHORT) {
                    rotation[i] += TWO_PI_SHORT;
                }
            }

            const int visible = static_cast<int>(args[9]);
            const int motion_no = static_cast<int>(args[10]);
            int       pack_size = 0;
            u_int    *npc_file = GetPackFile(pack, string_arg, &pack_size);
            NowDngMap->SetNPC(npc_no, npc_file, nowPartsCnt, position, rotation, visible, motion_no, &MapModelBuffer);
        }

        if (TEIGI_PT_FIRE[0] == command) {
            add_fire(args, 3);
        }
    }
}

/*
 * Weapon, attachment, item and item-id tables, indexed by the accessors below.
 */
WEAPON_DATA WeaponList[120] = {
    {48, 1,   0,  70, 0,   CHARA_TOAN,   {0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           32,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 25,  24,  0,  0}, /* Dagger[broken1] */
    {48, 6,   30, 70, 2,   CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 4, 0},      {0, 0, 0, 0, 0, 4, 0, 0, 0, 0},           32,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 25,  24,  1,  0}, /* Dagger */
    {40, 8,   20, 60, 4,   CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {6, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 10, 0, 0, 0},          40,  9,  0, 0, 0x0000, 0, 0x00002000, 0x00004000, 26,  25,  2,  0}, /* Baselard */
    {45, 10,  32, 60, 6,   CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 5},      {0, 15, 0, 0, 0, 0, 0, 0, 0, 0},          40,  10, 0, 0, 0x0000, 0, 0x00000000, 0x00000240, 37,  36,  3,  0}, /* Gladius */
    {50, 30,  22, 70, 12,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 16, 0},     {0, 0, 0, 0, 12, 12, 0, 0, 0, 12},        65,  10, 0, 0, 0x0000, 0, 0x00008000, 0x00000000, 55,  50,  4,  0}, /* Wise Owl Sword */
    {55, 12,  25, 70, 8,   CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 8},      {0, 5, 0, 0, 0, 5, 5, 0, 0, 0},           45,  8,  0, 0, 0x0000, 0, 0x00000000, 0x000000C0, 40,  38,  5,  0}, /* Crysknife */
    {58, 37,  39, 50, 25,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {6, 0, 0, 0, 20},     {20, 8, 0, 10, 0, 0, 0, 0, 10, 0},        50,  15, 0, 0, 0x0040, 0, 0x00020000, 0x00000000, 77,  47,  6,  0}, /* Antique Sword */
    {57, 24,  40, 50, 0,   CHARA_TOAN,   {2, 1, 1, 1, 0, 0}, 4, {0, 0, 0, 0, 0},      {0, 10, 0, 0, 0, 10, 0, 10, 0, 10},       60,  15, 0, 0, 0x0000, 0, 0x00000000, 0x00000800, 65,  35,  7,  0}, /* Buster Sword */
    {30, 5,   22, 80, 0,   CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 8, 0, 0, 0},      {0, 0, 33, 0, 0, 0, 0, 0, 0, 0},          50,  8,  0, 0, 0x0008, 0, 0x00000000, 0x00004200, 30,  15,  8,  0}, /* Kitchen Knife */
    {40, 40,  20, 70, 34,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 10, 20, 10, 0},   {0, 0, 0, 0, 0, 8, 0, 20, 0, 8},          68,  13, 0, 0, 0x0000, 0, 0x00004000, 0x00000000, 68,  70,  9,  0}, /* Tsukikage */
    {50, 50,  40, 60, 30,  CHARA_TOAN,   {2, 1, 1, 1, 0, 0}, 4, {30, 0, 35, 0, 0},    {30, 30, 0, 30, 0, 0, 0, 15, 0, 10},      90,  15, 0, 0, 0x0000, 0, 0x00040000, 0x00000000, 110, 58,  10, 0}, /* Sun Sword */
    {45, 22,  48, 60, 8,   CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 14, 12, 0},    {21, 0, 12, 0, 23, 24, 23, 0, 0, 0},      50,  14, 0, 0, 0x0000, 0, 0x00000200, 0x00000020, 70,  55,  11, 0}, /* Serpent Sword */
    {53, 43,  52, 50, 20,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 0, 8, 0, 12, 0, 0, 35, 0},         70,  15, 0, 0, 0x0000, 0, 0x00000000, 0x00020010, 68,  54,  12, 0}, /* Macho Sword */
    {32, 20,  30, 70, 6,   CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 8, 0, 0},      {0, 0, 0, 0, 0, 0, 10, 0, 0, 4},          70,  14, 0, 0, 0x0000, 0, 0x00000000, 0x00010000, 60,  39,  13, 0}, /* Shamshir */
    {60, 65,  50, 80, 40,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 30, 38, 38, 12},  {30, 0, 0, 0, 0, 30, 0, 35, 0, 0},        90,  18, 0, 0, 0x0000, 0, 0x00010000, 0x00000000, 102, 85,  14, 0}, /* Heaven's Cloud */
    {65, 45,  44, 70, 20,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 20, 0, 20},    {0, 0, 0, 0, 30, 35, 35, 0, 0, 0},        92,  20, 0, 0, 0x0000, 0, 0x00080000, 0x00000000, 80,  60,  15, 0}, /* Lamb's Sword */
    {60, 75,  55, 75, 45,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 50, 45, 0, 0},    {30, 0, 0, 0, 0, 0, 0, 30, 30, 0},        93,  24, 0, 0, 0x0000, 0, 0x00000000, 0x00040000, 145, 99,  16, 0}, /* Dark Cloud */
    {70, 60,  75, 80, 20,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {30, 0, 0, 0, 40},    {0, 34, 0, 38, 0, 0, 30, 25, 0, 18},      85,  18, 0, 0, 0x0000, 0, 0x00010000, 0x00000000, 95,  60,  17, 0}, /* Brave Ark */
    {70, 80,  77, 60, 58,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {50, 30, 55, 30, 30}, {35, 35, 0, 45, 0, 0, 0, 40, 0, 40},      80,  24, 0, 0, 0x0000, 0, 0x00000000, 0x00080000, 170, 95,  18, 0}, /* Big Bang */
    {70, 70,  80, 80, 60,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {25, 40, 25, 40, 20}, {30, 20, 10, 20, 10, 10, 10, 30, 40, 20}, 70,  25, 0, 0, 0x0000, 0, 0x00000000, 0x00100000, 155, 101, 19, 0}, /* Atlamillia Sword */
    {55, 12,  25, 70, 8,   CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 8},      {0, 5, 0, 0, 0, 5, 5, 0, 0, 0},           45,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 40,  45,  20, 0}, /* weapon No.277 */
    {58, 50,  50, 50, 50,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {5, 5, 5, 5, 5},      {10, 8, 10, 10, 0, 0, 0, 0, 10, 0},       60,  20, 0, 0, 0x0000, 0, 0x00000000, 0x00000004, 70,  65,  21, 0}, /* Mardan Eins */
    {55, 50,  50, 50, 50,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {5, 5, 5, 5, 5},      {10, 8, 10, 10, 0, 0, 0, 0, 10, 0},       45,  20, 0, 0, 0x0000, 0, 0x00000000, 0x00000008, 99,  99,  22, 0}, /* Mardan Twei */
    {55, 50,  50, 50, 50,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {5, 5, 5, 5, 5},      {10, 8, 10, 10, 0, 0, 0, 0, 10, 0},       45,  20, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 250, 120, 23, 0}, /* Arise Mardan */
    {58, 60,  60, 50, 30,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {6, 0, 0, 0, 8},      {20, 23, 20, 20, 23, 20, 20, 20, 10, 18}, 50,  13, 0, 0, 0x0000, 0, 0x00080000, 0x00000000, 95,  80,  24, 0}, /* Aga's Sword */
    {57, 43,  40, 50, 38,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 20, 10, 0, 0},    {0, 10, 0, 0, 0, 10, 0, 10, 0, 10},       60,  12, 0, 0, 0x0000, 0, 0x00000000, 0x00000100, 80,  75,  25, 0}, /* Evilcise */
    {30, 20,  33, 80, 36,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 8, 10, 10, 0},    {0, 0, 33, 0, 0, 0, 0, 0, 0, 0},          70,  10, 0, 0, 0x0000, 0, 0x00000200, 0x00000000, 60,  60,  26, 0}, /* Small Sword */
    {40, 28,  20, 70, 14,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 10, 0, 10, 12},   {0, 0, 0, 0, 0, 8, 0, 10, 0, 8},          68,  13, 0, 0, 0x0010, 0, 0x00000040, 0x00000000, 68,  45,  27, 0}, /* Sand Breaker */
    {50, 50,  40, 60, 55,  CHARA_TOAN,   {2, 1, 1, 1, 0, 0}, 4, {30, 30, 35, 30, 0},  {0, 0, 0, 30, 0, 30, 25, 0, 0, 10},       90,  20, 0, 0, 0x0400, 0, 0x00010000, 0x00000000, 99,  85,  28, 0}, /* Drain Seeker */
    {57, 30,  40, 50, 15,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 10, 0, 0},     {0, 10, 0, 0, 0, 10, 0, 10, 0, 10},       60,  12, 0, 0, 0x0000, 0, 0x00000000, 0x00010400, 65,  43,  29, 0}, /* Chopper */
    {30, 39,  44, 80, 28,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 8, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 20},          50,  13, 0, 0, 0x0000, 0, 0x00004000, 0x00001000, 74,  55,  30, 0}, /* Choora */
    {40, 55,  50, 70, 20,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 10, 0, 10, 12},   {0, 0, 0, 15, 0, 8, 10, 10, 10, 8},       68,  13, 0, 0, 0x0000, 0, 0x00000000, 0x00020000, 68,  45,  31, 0}, /* Claymore */
    {57, 55,  40, 50, 55,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 10, 0, 0},     {0, 10, 0, 0, 0, 10, 0, 10, 0, 55},       60,  12, 0, 0, 0x0000, 0, 0x00080000, 0x00000000, 99,  63,  32, 0}, /* Maneater */
    {30, 15,  20, 80, 30,  CHARA_TOAN,   {2, 1, 0, 0, 0, 0}, 2, {0, 5, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           50,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000020, 55,  62,  33, 0}, /* Bone Rapier */
    {57, 26,  40, 50, 15,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 10, 0, 0},     {0, 0, 0, 0, 10, 0, 0, 0, 10, 0},         60,  12, 0, 0, 0x0000, 0, 0x00000000, 0x00010000, 65,  41,  34, 0}, /* Sax */
    {30, 65,  40, 80, 35,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {20, 20, 20, 20, 20}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           50,  16, 0, 0, 0x0000, 0, 0x00080000, 0x00000000, 99,  65,  35, 0}, /* 7Branch Sword */
    {40, 38,  40, 70, 29,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {0, 20, 0, 20, 12},   {0, 0, 0, 0, 0, 8, 0, 10, 0, 8},          68,  14, 0, 0, 0x0000, 0, 0x00020000, 0x00008000, 78,  45,  36, 0}, /* Dusack */
    {50, 68,  40, 60, 21,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {10, 10, 10, 0, 30},  {30, 0, 0, 35, 0, 0, 0, 15, 20, 0},       90,  15, 0, 0, 0x0000, 0, 0x00040000, 0x00000000, 110, 60,  37, 0}, /* Cross Hinder */
    {40, 100, 40, 70, 95,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {80, 10, 85, 10, 12}, {0, 88, 0, 0, 0, 8, 0, 10, 0, 8},         68,  27, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 240, 110, 38, 0}, /* 7thHeaven */
    {50, 100, 77, 60, 95,  CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {70, 70, 70, 70, 70}, {30, 30, 33, 30, 29, 54, 22, 49, 22, 60}, 90,  24, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 199, 176, 39, 0}, /* Sword Of Zeus */
    {40, 120, 80, 80, 101, CHARA_TOAN,   {2, 1, 1, 0, 0, 0}, 3, {55, 55, 90, 55, 90}, {60, 60, 60, 60, 60, 60, 60, 60, 60, 60}, 125, 32, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 260, 150, 40, 0}, /* Chronicle Sword */
    {99, 199, 99, 89, 110, CHARA_TOAN,   {2, 1, 1, 1, 1, 0}, 5, {50, 50, 50, 50, 50}, {50, 50, 50, 50, 50, 50, 50, 50, 50, 50}, 125, 30, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 350, 200, 41, 0}, /* Chronicle2 */
    {48, 1,   0,  40, 0,   CHARA_XIAO,   {0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           40,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 28,  10,  0,  0}, /* WoodenSlingshot[broken1] */
    {48, 4,   40, 40, 0,   CHARA_XIAO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 8, 0, 0, 0},           40,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 28,  10,  1,  0}, /* Wooden Slingshot */
    {48, 14,  40, 43, 0,   CHARA_XIAO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 0},      {0, 0, 0, 5, 0, 0, 5, 5, 0, 0},           40,  10, 0, 0, 0x0000, 0, 0x00000040, 0x00000000, 35,  10,  2,  0}, /* Steel Slingshot */
    {46, 30,  38, 48, 0,   CHARA_XIAO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 12, 0},     {0, 5, 0, 0, 0, 0, 12, 0, 0, 5},          40,  10, 0, 0, 0x0080, 0, 0x000000C0, 0x00000000, 50,  10,  3,  0}, /* Bandit Slingshot */
    {50, 16,  48, 50, 0,   CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {11, 0, 0, 0, 0},     {0, 0, 0, 30, 0, 0, 0, 0, 20, 0},         58,  20, 0, 0, 0x0000, 0, 0x00002000, 0x00000000, 60,  30,  4,  0}, /* Steve */
    {40, 8,   20, 58, 0,   CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 12, 0, 0, 0, 0, 5, 0, 0, 0},          40,  12, 0, 0, 0x0000, 0, 0x00000800, 0x00000000, 35,  10,  5,  0}, /* Bone Slingshot */
    {50, 35,  50, 70, 0,   CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {5, 0, 0, 20, 0, 5, 12, 20, 0, 0},        65,  15, 0, 0, 0x0000, 0, 0x00001080, 0x00000000, 70,  18,  6,  0}, /* Hardshooter */
    {45, 40,  50, 60, 0,   CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 8, 0, 0, 0, 12, 0, 0, 12},         65,  15, 0, 0, 0x0000, 0, 0x00000200, 0x00000000, 88,  30,  7,  0}, /* Double Impact */
    {60, 55,  60, 65, 10,  CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {20, 0, 0, 0, 0},     {25, 0, 20, 0, 20, 0, 0, 0, 0, 0},        65,  18, 0, 0, 0x0000, 0, 0x00000200, 0x00000000, 85,  55,  8,  0}, /* Dragon's Y */
    {70, 60,  70, 80, 18,  CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {20, 0, 0, 20, 0},    {30, 0, 10, 0, 20, 50, 28, 0, 0, 0},      80,  25, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 140, 70,  9,  0}, /* DivineBeastTitle */
    {70, 99,  60, 65, 50,  CHARA_XIAO,   {2, 1, 1, 1, 0, 0}, 4, {20, 40, 0, 40, 0},   {25, 40, 20, 20, 20, 0, 0, 0, 50, 0},     80,  25, 0, 0, 0x0800, 0, 0x00004000, 0x00000000, 182, 99,  10, 0}, /* Angel Shooter */
    {45, 30,  50, 60, 0,   CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 20, 12, 0, 0, 20},        65,  15, 0, 0, 0x0000, 0, 0x00000100, 0x00000000, 55,  18,  11, 0}, /* Flamingo */
    {45, 42,  50, 60, 0,   CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {30, 0, 10, 0, 0},    {30, 0, 8, 0, 0, 0, 12, 0, 0, 30},        65,  15, 0, 0, 0x0000, 0, 0x00000200, 0x00000000, 90,  30,  12, 0}, /* Matador */
    {60, 50,  65, 77, 30,  CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 50, 0, 50},    {0, 35, 0, 30, 0, 0, 0, 30, 20, 35},      90,  26, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 130, 80,  13, 0}, /* Super Steve */
    {45, 150, 50, 70, 99,  CHARA_XIAO,   {2, 1, 1, 0, 0, 0}, 3, {40, 50, 30, 50, 60}, {5, 78, 0, 0, 0, 5, 69, 12, 0, 0},        92,  30, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 256, 103, 14, 0}, /* Ange Gear */
    {41, 1,   0,  30, 0,   CHARA_GORO,   {0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           40,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 35,  10,  0,  0}, /* Mallet[broken1] */
    {41, 20,  30, 30, 0,   CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 0},      {0, 0, 0, 5, 0, 0, 0, 0, 5, 0},           40,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 35,  10,  1,  0}, /* Mallet */
    {50, 25,  40, 20, 0,   CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 0},      {0, 0, 0, 12, 0, 0, 0, 10, 10, 0},        40,  8,  0, 0, 0x0000, 0, 0x00002000, 0x00000000, 65,  14,  2,  0}, /* Steel Hammer */
    {55, 62,  34, 45, 25,  CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {30, 20, 15, 5, 30},  {0, 0, 0, 5, 0, 0, 0, 0, 18, 20},         60,  9,  0, 0, 0x0000, 0, 0x00000300, 0x00000000, 120, 80,  3,  0}, /* Magical Hammer */
    {55, 65,  45, 45, 0,   CHARA_GORO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 0, 15, 0, 0, 0, 0, 30, 30},        65,  12, 0, 0, 0x0000, 0, 0x00000800, 0x00000000, 125, 55,  4,  0}, /* Battle Ax */
    {55, 36,  40, 45, 0,   CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {0, 30, 0, 0, 0},     {0, 0, 0, 0, 0, 0, 0, 0, 18, 0},          58,  12, 0, 0, 0x0000, 0, 0x00000018, 0x00000000, 78,  25,  5,  0}, /* Turtle Shell */
    {55, 40,  35, 30, 14,  CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 15, 0},          58,  15, 0, 0, 0x0002, 0, 0x00000108, 0x00000000, 80,  30,  6,  0}, /* Big Bucks Hammer */
    {80, 22,  20, 40, 10,  CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {0, 20, 0, 0, 0},     {0, 0, 18, 0, 0, 0, 0, 0, 0, 0},          40,  15, 0, 0, 0x0000, 0, 0x00000020, 0x00000000, 56,  30,  7,  0}, /* Frozen Tuna */
    {65, 77,  10, 48, 25,  CHARA_GORO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 10, 0},     {0, 0, 0, 50, 0, 0, 0, 0, 20, 0},         65,  20, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 165, 90,  8,  0}, /* Gaia Hammer */
    {65, 99,  45, 48, 55,  CHARA_GORO,   {2, 1, 1, 1, 0, 0}, 4, {10, 10, 10, 10, 10}, {0, 45, 0, 0, 0, 0, 0, 20, 25, 20},       65,  22, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 150, 85,  9,  0}, /* Last Judgement */
    {88, 128, 50, 55, 85,  CHARA_GORO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 55, 0, 0},     {20, 0, 20, 0, 0, 0, 0, 0, 30, 20},       78,  24, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 260, 200, 10, 0}, /* Tall Hammer */
    {88, 125, 60, 65, 30,  CHARA_GORO,   {2, 1, 1, 0, 0, 0}, 3, {55, 0, 0, 0, 0},     {0, 20, 0, 20, 20, 20, 0, 20, 55, 20},    78,  24, 0, 0, 0x0000, 0, 0x00008000, 0x00000000, 199, 222, 11, 0}, /* Satan's Ax */
    {5,  5,   5,  5,  5,   CHARA_GORO,   {2, 1, 1, 1, 1, 0}, 5, {5, 5, 5, 5, 5},      {5, 5, 5, 5, 5, 5, 5, 5, 5, 5},           5,   5,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 5,   5,   12, 0}, /* 12 */
    {55, 42,  35, 30, 14,  CHARA_GORO,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 15, 0},          58,  15, 0, 0, 0x0000, 0, 0x00000008, 0x00000000, 89,  30,  13, 0}, /* Plate Hammer */
    {65, 20,  10, 48, 25,  CHARA_GORO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 0, 20, 0, 0, 0, 0, 20, 0},         65,  12, 0, 0, 0x2004, 0, 0x00000100, 0x00000000, 77,  40,  14, 0}, /* Trial Hammer */
    {65, 199, 45, 48, 128, CHARA_GORO,   {2, 1, 1, 1, 0, 0}, 4, {90, 0, 0, 0, 0},     {80, 0, 0, 60, 0, 0, 0, 58, 25, 20},      99,  30, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 350, 300, 15, 0}, /* Inferno */
    {50, 40,  10, 48, 25,  CHARA_GORO,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 0, 20, 0, 0, 0, 0, 20, 0},         50,  50, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 65,  40,  16, 0}, /* no name */
    {45, 1,   40, 60, 1,   CHARA_RUBY,   {0, 0, 0, 0, 0, 0}, 0, {1, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           40,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 35,  43,  0,  0}, /* Gold Ring[broken1] */
    {45, 10,  40, 65, 20,  CHARA_RUBY,   {2, 1, 0, 0, 0, 0}, 2, {15, 0, 0, 0, 0},     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           40,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 35,  43,  1,  0}, /* Gold Ring */
    {48, 12,  40, 67, 25,  CHARA_RUBY,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 18, 0},     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           40,  10, 0, 0, 0x0080, 0, 0x00001008, 0x00000000, 38,  55,  2,  0}, /* Bandit's Ring */
    {53, 38,  40, 64, 55,  CHARA_RUBY,   {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 0, 20},     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           56,  20, 0, 0, 0x0000, 0, 0x00000120, 0x00000000, 80,  88,  3,  0}, /* Crystal Ring */
    {55, 17,  40, 66, 40,  CHARA_RUBY,   {2, 1, 0, 0, 0, 0}, 2, {0, 20, 0, 0, 0},     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           56,  12, 0, 0, 0x0000, 0, 0x00000048, 0x00000000, 50,  65,  4,  0}, /* Platinum Ring */
    {62, 38,  40, 58, 60,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {10, 10, 26, 10, 55}, {0, 30, 0, 0, 0, 0, 0, 0, 0, 30},         67,  22, 0, 0, 0x0000, 0, 0x00000200, 0x00000000, 95,  128, 5,  0}, /* Goddess Ring */
    {64, 35,  40, 69, 65,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 15, 20},    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           67,  20, 0, 0, 0x0000, 0, 0x00000080, 0x00000000, 66,  90,  6,  0}, /* Fairy's Ring */
    {67, 50,  40, 65, 65,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {30, 30, 30, 30, 10}, {0, 0, 0, 30, 0, 0, 0, 30, 0, 0},         67,  22, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 99,  99,  7,  0}, /* Destruction Ring */
    {70, 40,  40, 44, 60,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {55, 25, 18, 18, 0},  {30, 0, 0, 0, 0, 0, 0, 0, 30, 0},         67,  22, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 95,  128, 8,  0}, /* Satan's Ring */
    {81, 60,  60, 60, 99,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {15, 20, 40, 10, 20}, {0, 50, 0, 0, 0, 20, 0, 0, 0, 0},         78,  24, 0, 0, 0x0000, 0, 0x00004000, 0x00000000, 128, 220, 9,  0}, /* Athena's Armlet */
    {89, 66,  70, 88, 99,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {0, 35, 45, 60, 50},  {50, 0, 0, 0, 0, 0, 0, 0, 50, 0},         78,  24, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 99,  250, 10, 0}, /* Mobius Ring */
    {5,  5,   5,  5,  5,   CHARA_RUBY,   {2, 1, 1, 1, 1, 0}, 5, {5, 5, 5, 5, 5},      {5, 55, 5, 5, 5, 5, 5, 5, 5, 5},          5,   5,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 5,   5,   11, 0}, /* 11 */
    {50, 35,  40, 69, 55,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 10, 20},    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           50,  15, 0, 0, 0x0000, 0, 0x00000040, 0x00000000, 66,  70,  12, 0}, /* Pocklekul */
    {67, 50,  40, 65, 35,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {15, 10, 10, 10, 10}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           67,  11, 0, 0, 0x0000, 0, 0x00000010, 0x00000000, 60,  46,  13, 0}, /* Thorn Armlet */
    {67, 99,  40, 65, 210, CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {77, 66, 50, 70, 80}, {25, 25, 25, 25, 25, 25, 25, 25, 25, 25}, 67,  30, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 155, 300, 14, 0}, /* Secret Armlet */
    {50, 50,  40, 65, 65,  CHARA_RUBY,   {2, 1, 1, 0, 0, 0}, 3, {15, 10, 10, 10, 10}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           50,  50, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 60,  85,  15, 0}, /* no name */
    {48, 1,   0,  60, 0,   CHARA_UNGAGA, {0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 1, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           45,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 55,  20,  0,  0}, /* Fighting Stick[broken1] */
    {48, 35,  20, 65, 0,   CHARA_UNGAGA, {2, 1, 0, 0, 0, 0}, 2, {0, 0, 0, 5, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           45,  12, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 55,  20,  1,  0}, /* Fighting Stick */
    {52, 38,  25, 70, 0,   CHARA_UNGAGA, {2, 1, 0, 0, 0, 0}, 2, {8, 0, 0, 12, 0},     {0, 0, 0, 5, 8, 0, 0, 0, 0, 0},           45,  16, 0, 0, 0x0000, 0, 0x00000050, 0x00000000, 85,  20,  2,  0}, /* Javelin */
    {52, 44,  28, 75, 0,   CHARA_UNGAGA, {2, 1, 0, 0, 0, 0}, 2, {0, 10, 0, 15, 0},    {0, 0, 0, 0, 0, 8, 5, 8, 0, 0},           45,  16, 0, 0, 0x0000, 0, 0x00000020, 0x00000000, 80,  20,  3,  0}, /* Halbert */
    {52, 46,  35, 74, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 25, 17, 0},    {0, 0, 25, 0, 0, 0, 0, 0, 0, 8},          45,  18, 0, 0, 0x0000, 0, 0x00002000, 0x00000000, 110, 20,  4,  0}, /* De Sanga */
    {58, 50,  35, 78, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {10, 0, 0, 21, 0},    {12, 8, 8, 0, 0, 8, 8, 12, 0, 0},         56,  20, 0, 0, 0x0020, 0, 0x00002080, 0x00000000, 99,  30,  5,  0}, /* Scorpion */
    {60, 55,  45, 90, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 10, 18, 0},    {0, 0, 0, 8, 8, 0, 0, 0, 12, 8},          56,  20, 0, 0, 0x0000, 0, 0x00002000, 0x00000000, 95,  30,  6,  0}, /* Partisan */
    {72, 63,  45, 77, 10,  CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {45, 0, 0, 20, 0},    {16, 16, 16, 0, 0, 16, 16, 16, 0, 0},     70,  24, 0, 0, 0x0000, 0, 0x00000300, 0x00000000, 150, 40,  7,  0}, /* Mirage */
    {74, 120, 50, 75, 22,  CHARA_UNGAGA, {2, 1, 1, 1, 0, 0}, 4, {30, 20, 30, 30, 2},  {0, 0, 0, 16, 16, 0, 0, 0, 16, 16},       70,  34, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 220, 66,  8,  0}, /* Terra Sword */
    {81, 110, 70, 99, 20,  CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 24, 28, 54},   {24, 24, 24, 0, 0, 45, 24, 24, 0, 0},     80,  26, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 256, 50,  9,  0}, /* Hercules' Wrath */
    {87, 212, 70, 99, 50,  CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 88, 30, 32},   {55, 0, 0, 24, 24, 0, 0, 0, 24, 24},      80,  30, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 321, 80,  10, 0}, /* Babel's Spear */
    {5,  5,   5,  5,  5,   CHARA_UNGAGA, {2, 1, 1, 1, 1, 0}, 5, {5, 5, 5, 5, 5},      {5, 5, 5, 5, 5, 5, 5, 5, 5, 5},           5,   5,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 5,   5,   11, 0}, /* 11 */
    {60, 40,  45, 90, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 10, 18, 0},    {0, 0, 0, 8, 5, 0, 0, 0, 8, 5},           56,  20, 0, 0, 0x1000, 0, 0x00000060, 0x00000000, 79,  30,  12, 0}, /* 5 Foot Nail */
    {60, 70,  45, 90, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 35, 0, 35, 0},    {0, 0, 0, 8, 8, 0, 0, 0, 8, 8},           56,  20, 0, 0, 0x0000, 0, 0x00000100, 0x00000000, 155, 30,  13, 0}, /* Cactus */
    {72, 63,  45, 77, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {10, 0, 0, 20, 0},    {16, 16, 16, 0, 0, 16, 16, 16, 0, 0},     70,  24, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 83,  40,  14, 0}, /* no name */
    {60, 55,  45, 90, 0,   CHARA_UNGAGA, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 10, 18, 0},    {0, 0, 0, 8, 8, 0, 0, 0, 8, 8},           56,  20, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 75,  30,  15, 0}, /* no name */
    {50, 1,   0,  40, 0,   CHARA_OSMOND, {0, 0, 0, 0, 0, 0}, 0, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           38,  8,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 32,  15,  0,  0}, /* Machine Gun[broken1] */
    {50, 22,  30, 55, 0,   CHARA_OSMOND, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           38,  14, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 32,  15,  1,  0}, /* Machine Gun */
    {58, 30,  35, 65, 8,   CHARA_OSMOND, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},           58,  16, 0, 0, 0x0000, 0, 0x00001020, 0x00000000, 55,  25,  2,  0}, /* Jackal */
    {0,  20,  40, 65, 0,   CHARA_OSMOND, {2, 0, 0, 0, 0, 0}, 1, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           0,   0,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 40,  20,  3,  0}, /* Launcher */
    {0,  30,  47, 65, 0,   CHARA_OSMOND, {2, 0, 0, 0, 0, 0}, 1, {0, 0, 0, 0, 0},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           0,   0,  0, 0, 0x0000, 0, 0x00000000, 0x00000000, 60,  20,  4,  0}, /* LauncherV2 */
    {45, 34,  50, 60, 10,  CHARA_OSMOND, {2, 1, 1, 1, 0, 0}, 4, {15, 15, 15, 15, 15}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           38,  18, 0, 0, 0x0000, 0, 0x00000040, 0x00000000, 56,  30,  5,  0}, /* Blessing Gun */
    {55, 48,  55, 66, 20,  CHARA_OSMOND, {2, 1, 1, 1, 0, 0}, 4, {25, 25, 25, 25, 25}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           58,  20, 0, 0, 0x0000, 0, 0x00000100, 0x00000000, 72,  55,  6,  0}, /* Skunk */
    {68, 50,  60, 60, 25,  CHARA_OSMOND, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {8, 8, 8, 30, 8, 8, 8, 35, 20, 8},        69,  21, 0, 0, 0x0000, 0, 0x00000200, 0x00000000, 99,  35,  7,  0}, /* G CRUSHER */
    {68, 64,  40, 77, 55,  CHARA_OSMOND, {2, 1, 1, 1, 0, 0}, 4, {39, 39, 39, 39, 39}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           69,  25, 0, 0, 0x0000, 0, 0x00000400, 0x00000000, 128, 90,  8,  0}, /* Hexa Blaster */
    {80, 70,  80, 70, 30,  CHARA_OSMOND, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {20, 20, 20, 65, 20, 20, 20, 45, 30, 20}, 80,  28, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 120, 45,  9,  0}, /* Star Breaker */
    {84, 128, 98, 88, 30,  CHARA_OSMOND, {2, 1, 1, 0, 0, 0}, 3, {60, 60, 60, 60, 60}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           90,  34, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 256, 128, 10, 0}, /* Supernova */
    {45, 34,  50, 60, 10,  CHARA_OSMOND, {2, 1, 1, 1, 0, 0}, 4, {5, 5, 5, 5, 5},      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           38,  18, 0, 0, 0x0000, 0, 0x00000120, 0x00000000, 65,  55,  11, 0}, /* Snail */
    {55, 48,  55, 66, 20,  CHARA_OSMOND, {2, 1, 1, 0, 0, 0}, 3, {0, 0, 0, 0, 0},      {6, 6, 6, 15, 6, 6, 6, 6, 12, 6},         58,  18, 0, 0, 0x0000, 0, 0x00000080, 0x00000000, 72,  30,  12, 0}, /* Swallow */
    {55, 48,  55, 66, 20,  CHARA_OSMOND, {2, 1, 1, 1, 0, 0}, 4, {15, 15, 15, 15, 15}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},           58,  18, 0, 0, 0x0000, 0, 0x00000000, 0x00000000, 72,  30,  13, 0}, /* no name */
};

ATTACH_DATA AttachList[50] = {
    {ITEM_ATTACH_FIRE,          0, 0, 0, 0,  0,  0,  0,
     3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_ICE,           0, 0, 0, 0,  0,  0,  0,
     0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_THUNDER,       0, 0, 0, 0,  0,  0,  0,
     0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_WIND,          0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_HOLY,          0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {86,                        0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 86, no name */
    {87,                        0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 87, no name */
    {88,                        0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 88, no name */
    {89,                        0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 89, no name */
    {90,                        0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 90, no name */
    {ITEM_ATTACH_ATTACK,        0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_ENDURANCE,     0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_SPEED,         0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_MAGICAL_POWER, 0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_GARNET,        0, 0, 0, 5,  0,  0,  0,
     10, 0,  0,  0,  0,  0,  0,  0,  10, 0,  10, 0,  0,  0,  0,  0},
    {ITEM_ATTACH_AMETHYST,      0, 0, 0, 0,  0,  0,  0,
     0,  10, 0,  0,  0,  10, 0,  0,  0,  0,  0,  0,  0,  0,  10, 0},
    {ITEM_ATTACH_AQUAMARINE,    0, 0, 0, 0,  0,  0,  0,
     0,  10, 0,  0,  0,  0,  0,  10, 0,  0,  0,  10, 0,  0,  0,  0},
    {ITEM_ATTACH_DIAMOND,       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  5,  5,  5,  5,  5,  5,  5,  10, 5,  5,  0},
    {ITEM_ATTACH_EMERALD,       0, 0, 0, 0,  0,  0,  10,
     0,  0,  0,  0,  0,  10, 0,  0,  0,  10, 0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_PEARL,         0, 0, 0, 0,  10, 0,  0,
     0,  0,  10, 0,  0,  0,  10, 0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_RUBY,          0, 0, 0, 0,  0,  10, 0,
     10, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  10, 0,  0},
    {ITEM_ATTACH_PERIDOT,       0, 0, 0, 5,  0,  0,  0,
     0,  0,  0,  0,  10, 0,  0,  0,  0,  10, 10, 0,  0,  0,  0,  0},
    {ITEM_ATTACH_SAPPHIRE,      0, 0, 0, 0,  0,  0,  10,
     0,  0,  0,  10, 0,  0,  0,  0,  0,  0,  0,  10, 0,  0,  0,  0},
    {ITEM_ATTACH_OPAL,          0, 0, 0, 0,  10, 0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  10, 0,  10, 0},
    {ITEM_ATTACH_TOPAZ,         0, 0, 0, 5,  0,  10, 0,
     0,  0,  0,  0,  0,  0,  10, 0,  0,  0,  0,  10, 0,  0,  0,  0},
    {ITEM_ATTACH_TURQUOISE,     0, 0, 0, 0,  0,  0,  0,
     0,  10, 0,  0,  0,  0,  0,  0,  10, 0,  0,  0,  0,  10, 0,  0},
    {ITEM_ATTACH_SUN,           0, 0, 0, 10, 0,  0,  0,
     10, 10, 10, 10, 10, 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  0},
    {108,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 108, no name */
    {109,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 109, no name */
    {110,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 110, no name */
    {ITEM_ATTACH_DINOSLAYER,    0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_UNDEAD_BUSTER, 0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_SEA_KILLER,    0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_STONE_BREAKER, 0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_PLANT_BUSTER,  0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_BEAST_BUSTER,  0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0},
    {ITEM_ATTACH_SKY_HUNTER,    0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0},
    {ITEM_ATTACH_METALBREAKER,  0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0},
    {ITEM_ATTACH_MIMIC_BREAKER, 0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0},
    {ITEM_ATTACH_MAGE_SLAYER,   0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0},
    {121,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 121, no name */
    {122,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 122, no name */
    {123,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 123, no name */
    {124,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 124, no name */
    {125,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 125, no name */
    {126,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 126, no name */
    {127,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 127, no name */
    {128,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 128, no name */
    {129,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 129, no name */
    {130,                       0, 0, 0, 0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* id 130, no name */
};

ITEM_DATA ITEM_LIST[175] = {
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_MULTI }, /* attach slot */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 3,   5,  0, ITEM_STACK_SINGLE}, /* Anti Freeze Amulet */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 3,   5,  0, ITEM_STACK_SINGLE}, /* AntiCurseAmulet */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 3,   5,  0, ITEM_STACK_SINGLE}, /* Antigoo Amulet */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 3,   5,  0, ITEM_STACK_SINGLE}, /* Antidote Amulet */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0030, 5,   3,  0, ITEM_STACK_NEVER }, /* Fluffy Doughnut */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0030, 5,   3,  0, ITEM_STACK_NEVER }, /* Fish Candy */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0030, 5,   3,  0, ITEM_STACK_NEVER }, /* Grass Cake */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0030, 5,   3,  0, ITEM_STACK_NEVER }, /* Witch Parfait */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0030, 5,   3,  0, ITEM_STACK_NEVER }, /* Scorpion Jerky */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0030, 5,   3,  0, ITEM_STACK_NEVER }, /* Carrot Cookie */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Dummy */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_FOOD,     0, 0x00000080, 0x0004, 30,  0,  0, ITEM_STACK_MULTI }, /* Regular Water */
    {ITEM_SORT_FOOD,     0, 0x00000080, 0x0004, 50,  0,  0, ITEM_STACK_MULTI }, /* Tasty Water */
    {ITEM_SORT_FOOD,     0, 0x00000080, 0x0004, 100, 0,  0, ITEM_STACK_MULTI }, /* Premium Water */
    {ITEM_SORT_FOOD,     0, 0x00000040, 0x0004, 50,  0,  0, ITEM_STACK_MULTI }, /* Bread */
    {ITEM_SORT_FOOD,     0, 0x00000040, 0x0004, 200, 0,  0, ITEM_STACK_MULTI }, /* Premium Chicken */
    {ITEM_SORT_FOOD,     0, 0x00001000, 0x0001, 0,   0,  0, ITEM_STACK_MULTI }, /* Stamina Drink */
    {ITEM_SORT_FOOD,     0, 0x00008000, 0x0001, 0,   0,  0, ITEM_STACK_MULTI }, /* Antidote Drink */
    {ITEM_SORT_FOOD,     0, 0x00010010, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Holy Water */
    {ITEM_SORT_FOOD,     0, 0x00020000, 0x0001, 0,   0,  0, ITEM_STACK_MULTI }, /* Soap */
    {ITEM_SORT_FOOD,     0, 0x0003C000, 0x0001, 0,   0,  0, ITEM_STACK_MULTI }, /* Mighty Healing */
    {ITEM_SORT_FOOD,     0, 0x00000040, 0x0004, 80,  0,  0, ITEM_STACK_MULTI }, /* Cheese */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_THROW,    0, 0x00000000, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Bomb */
    {ITEM_SORT_THROW,    0, 0x00000000, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Stone */
    {ITEM_SORT_THROW,    0, 0x00000001, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Fire Gem */
    {ITEM_SORT_THROW,    0, 0x00000002, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Ice Gem */
    {ITEM_SORT_THROW,    0, 0x00000004, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Thunder Gem */
    {ITEM_SORT_THROW,    0, 0x00000008, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Wind Gem */
    {ITEM_SORT_THROW,    0, 0x00000010, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Holy Gem */
    {ITEM_SORT_THROW,    0, 0x00000100, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Throbbing Cherry */
    {ITEM_SORT_THROW,    0, 0x00000800, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Gooey Peach */
    {ITEM_SORT_THROW,    0, 0x00000000, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Bomb Nuts */
    {ITEM_SORT_THROW,    0, 0x00000200, 0x0003, 0,   0,  0, ITEM_STACK_MULTI }, /* Poisonous Apple */
    {ITEM_SORT_THROW,    0, 0x00000040, 0x0005, 200, 30, 0, ITEM_STACK_MULTI }, /* Mellow Banana */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Medusa Powder */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Hardening Powder */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Warp Powder */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Stand-in Powder */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Escape Powder */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_SINGLE}, /* Revival Powder */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Repair Powder */
    {ITEM_SORT_POWDER,   0, 0x00000020, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Powerup Powder */
    {ITEM_SORT_FOOD,     0, 0x00000020, 0x0011, 10,  0,  0, ITEM_STACK_NEVER }, /* Pocket */
    {ITEM_SORT_FOOD,     0, 0x00000060, 0x0011, 10,  0,  0, ITEM_STACK_NEVER }, /* Fruit of Eden */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Treasure Key */
    {ITEM_SORT_FOOD,     0, 0x000000A0, 0x0011, 10,  0,  0, ITEM_STACK_NEVER }, /* Gourd */
    {ITEM_SORT_POWDER,   0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_SINGLE}, /* Auto Repair Powder */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Fishing Rod */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Carrot */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Potato cake */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Minon */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Battan */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Petite Fish */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Saving Book */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* Gold Bullion */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Evy */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Dran's Crest */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Shiny Stone */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Mimi */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Red Berry */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0001, 0,   0,  0, ITEM_STACK_NEVER }, /* Prickly */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Candy */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Hook */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* King's Slate */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Gun Powder */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Clock Hands */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Pointy Chestnut */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Black Knight Crest */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Horned Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Moon Grass Seed */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Music Box Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Sun Signet */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Moon Signet */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Admission Ticket */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Sun Sword */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Bone Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Moustache Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Shipcabin Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Stone Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Handle */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Pitchdark Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Silver Key */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_NEVER }, /* Tram Oil */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_NEVER }, /* Sun Dew */
    {ITEM_SORT_FISH,     0, 0x00000000, 0x0009, 600, 0,  0, ITEM_STACK_NEVER }, /* Flapping Fish */
    {ITEM_SORT_ROTTEN,   0, 0x00000000, 0x0009, 0,   0,  0, ITEM_STACK_NEVER }, /* Rotten Fish */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_NEVER }, /* Secret Path Key */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_NEVER }, /* Bravery Launch */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_NEVER }, /* Flapping Duster */
    {ITEM_SORT_QUEST,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_NEVER }, /* Crystal Eyeball */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0000, 0,   0,  0, ITEM_STACK_NEVER }, /* no name */
    {ITEM_SORT_NONE,     0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Map */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Magical Crystal */
    {ITEM_SORT_GOODS,    0, 0x00000000, 0x0008, 0,   0,  0, ITEM_STACK_SINGLE}, /* Dran's Feather */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Cave Key */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Changing Potion */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Worldmap */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Bone Pendant */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Odd Tone Flute */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Magical Lamp */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Moon Orb */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Shell Ring */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Search Warrant */
    {ITEM_SORT_FISH,     0, 0x00000000, 0x0001, 200, 0,  0, ITEM_STACK_NEVER }, /* Ice Block */
    {ITEM_SORT_FISH,     0, 0x00000000, 0x0001, 200, 0,  0, ITEM_STACK_NEVER }, /* Small Ice */
    {ITEM_SORT_FISH,     0, 0x00000000, 0x0001, 200, 0,  0, ITEM_STACK_NEVER }, /* Tiny Ice */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Flame Key */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Hunter's Earring */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Ointment Leaf */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Foundation */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Clay Doll */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Manual */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Sun Sphere */
    {ITEM_SORT_KEY_ITEM, 0, 0x00000000, 0x0018, 0,   0,  0, ITEM_STACK_NEVER }, /* Almighty Pass */
};

COM_ITEM_INFO ComItemInfo[296] = {
    {ITEMKIND_ATTACH, 0, 0,   120, 81 }, /* id  81  Supernova */
    {ITEMKIND_ATTACH, 0, 1,   121, 82 }, /* id  82  Fire */
    {ITEMKIND_ATTACH, 0, 2,   122, 83 }, /* id  83  Ice */
    {ITEMKIND_ATTACH, 0, 3,   123, 84 }, /* id  84  Thunder */
    {ITEMKIND_ATTACH, 0, 4,   124, 85 }, /* id  85  Wind */
    {ITEMKIND_ATTACH, 0, 5,   125, 86 }, /* id  86  Holy */
    {ITEMKIND_ATTACH, 0, 6,   126, 87 }, /* id  87 */
    {ITEMKIND_ATTACH, 0, 7,   127, 88 }, /* id  88 */
    {ITEMKIND_ATTACH, 0, 8,   128, 89 }, /* id  89 */
    {ITEMKIND_ATTACH, 0, 9,   129, 90 }, /* id  90 */
    {ITEMKIND_ATTACH, 0, 10,  130, 91 }, /* id  91 */
    {ITEMKIND_ATTACH, 0, 11,  131, 92 }, /* id  92  Attack */
    {ITEMKIND_ATTACH, 0, 12,  132, 93 }, /* id  93  Endurance */
    {ITEMKIND_ATTACH, 0, 13,  133, 94 }, /* id  94  Speed */
    {ITEMKIND_ATTACH, 0, 14,  134, 95 }, /* id  95  Magical power */
    {ITEMKIND_ATTACH, 0, 15,  135, 96 }, /* id  96  Garnet */
    {ITEMKIND_ATTACH, 0, 16,  136, 97 }, /* id  97  Amethyst */
    {ITEMKIND_ATTACH, 0, 17,  137, 98 }, /* id  98  Aquamarine */
    {ITEMKIND_ATTACH, 0, 18,  138, 99 }, /* id  99  Diamond */
    {ITEMKIND_ATTACH, 0, 19,  139, 100}, /* id 100  Emerald */
    {ITEMKIND_ATTACH, 0, 20,  140, 101}, /* id 101  Pearl */
    {ITEMKIND_ATTACH, 0, 21,  141, 102}, /* id 102  Ruby */
    {ITEMKIND_ATTACH, 0, 22,  142, 103}, /* id 103  Peridot */
    {ITEMKIND_ATTACH, 0, 23,  143, 104}, /* id 104  Sapphire */
    {ITEMKIND_ATTACH, 0, 24,  144, 105}, /* id 105  Opal */
    {ITEMKIND_ATTACH, 0, 25,  145, 106}, /* id 106  Topaz */
    {ITEMKIND_ATTACH, 0, 26,  146, 107}, /* id 107  Turquoise */
    {ITEMKIND_ATTACH, 0, 27,  147, 108}, /* id 108  Sun */
    {ITEMKIND_ATTACH, 0, 28,  148, 109}, /* id 109 */
    {ITEMKIND_ATTACH, 0, 29,  149, 110}, /* id 110 */
    {ITEMKIND_ATTACH, 0, 30,  150, 111}, /* id 111 */
    {ITEMKIND_ATTACH, 0, 31,  151, 112}, /* id 112  Dinoslayer */
    {ITEMKIND_ATTACH, 0, 32,  152, 113}, /* id 113  Undead Buster */
    {ITEMKIND_ATTACH, 0, 33,  153, 114}, /* id 114  Sea Killer */
    {ITEMKIND_ATTACH, 0, 34,  154, 115}, /* id 115  Stone Breaker */
    {ITEMKIND_ATTACH, 0, 35,  155, 116}, /* id 116  Plant Buster */
    {ITEMKIND_ATTACH, 0, 36,  156, 117}, /* id 117  Beast Buster */
    {ITEMKIND_ATTACH, 0, 37,  157, 118}, /* id 118  Sky Hunter */
    {ITEMKIND_ATTACH, 0, 38,  158, 119}, /* id 119  MetalBreaker */
    {ITEMKIND_ATTACH, 0, 39,  159, 120}, /* id 120  Mimic Breaker */
    {ITEMKIND_ATTACH, 0, 40,  160, 121}, /* id 121  Mage Slayer */
    {ITEMKIND_ATTACH, 0, 41,  161, 122}, /* id 122 */
    {ITEMKIND_ATTACH, 0, 42,  162, 123}, /* id 123 */
    {ITEMKIND_ATTACH, 0, 43,  163, 124}, /* id 124 */
    {ITEMKIND_ATTACH, 0, 44,  164, 125}, /* id 125 */
    {ITEMKIND_ATTACH, 0, 45,  165, 126}, /* id 126 */
    {ITEMKIND_ATTACH, 0, 46,  166, 127}, /* id 127 */
    {ITEMKIND_ATTACH, 0, 47,  167, 128}, /* id 128 */
    {ITEMKIND_ATTACH, 0, 48,  0,   129}, /* id 129 */
    {ITEMKIND_ATTACH, 0, 49,  1,   130}, /* id 130 */
    {ITEMKIND_ATTACH, 0, 50,  2,   131}, /* id 131 */
    {ITEMKIND_ITEM,   0, 51,  3,   132}, /* id 132 */
    {ITEMKIND_ITEM,   0, 52,  4,   133}, /* id 133  Anti Freeze Amulet */
    {ITEMKIND_ITEM,   0, 53,  5,   134}, /* id 134  AntiCurseAmulet */
    {ITEMKIND_ITEM,   0, 54,  6,   135}, /* id 135  Antigoo Amulet */
    {ITEMKIND_ITEM,   0, 55,  7,   136}, /* id 136  Antidote Amulet */
    {ITEMKIND_ITEM,   0, 56,  8,   137}, /* id 137  Fluffy Doughnut */
    {ITEMKIND_ITEM,   0, 57,  9,   138}, /* id 138  Fish Candy */
    {ITEMKIND_ITEM,   0, 58,  10,  139}, /* id 139  Grass Cake */
    {ITEMKIND_ITEM,   0, 59,  11,  140}, /* id 140  Witch Parfait */
    {ITEMKIND_ITEM,   0, 60,  12,  141}, /* id 141  Scorpion Jerky */
    {ITEMKIND_ITEM,   0, 61,  13,  142}, /* id 142  Carrot Cookie */
    {ITEMKIND_ITEM,   0, 62,  14,  143}, /* id 143  Dummy */
    {ITEMKIND_ITEM,   0, 63,  15,  144}, /* id 144 */
    {ITEMKIND_ITEM,   0, 64,  16,  145}, /* id 145 */
    {ITEMKIND_ITEM,   0, 65,  17,  146}, /* id 146  Regular Water */
    {ITEMKIND_ITEM,   0, 66,  18,  147}, /* id 147  Tasty Water */
    {ITEMKIND_ITEM,   0, 67,  19,  148}, /* id 148  Premium Water */
    {ITEMKIND_ITEM,   0, 68,  20,  149}, /* id 149  Bread */
    {ITEMKIND_ITEM,   0, 69,  21,  150}, /* id 150  Premium Chicken */
    {ITEMKIND_ITEM,   0, 70,  22,  151}, /* id 151  Stamina Drink */
    {ITEMKIND_ITEM,   0, 71,  23,  152}, /* id 152  Antidote Drink */
    {ITEMKIND_ITEM,   0, 72,  24,  153}, /* id 153  Holy Water */
    {ITEMKIND_ITEM,   0, 73,  25,  154}, /* id 154  Soap */
    {ITEMKIND_ITEM,   0, 74,  26,  155}, /* id 155  Mighty Healing */
    {ITEMKIND_ITEM,   0, 75,  27,  156}, /* id 156  Cheese */
    {ITEMKIND_ITEM,   0, 76,  28,  157}, /* id 157 */
    {ITEMKIND_ITEM,   0, 77,  29,  158}, /* id 158 */
    {ITEMKIND_ITEM,   0, 78,  30,  159}, /* id 159 */
    {ITEMKIND_ITEM,   0, 79,  31,  160}, /* id 160  Bomb */
    {ITEMKIND_ITEM,   0, 80,  32,  161}, /* id 161  Stone */
    {ITEMKIND_ITEM,   0, 81,  33,  162}, /* id 162  Fire Gem */
    {ITEMKIND_ITEM,   0, 82,  34,  163}, /* id 163  Ice Gem */
    {ITEMKIND_ITEM,   0, 83,  35,  164}, /* id 164  Thunder Gem */
    {ITEMKIND_ITEM,   0, 84,  36,  165}, /* id 165  Wind Gem */
    {ITEMKIND_ITEM,   0, 85,  37,  166}, /* id 166  Holy Gem */
    {ITEMKIND_ITEM,   0, 86,  38,  167}, /* id 167  Throbbing Cherry */
    {ITEMKIND_ITEM,   0, 87,  39,  168}, /* id 168  Gooey Peach */
    {ITEMKIND_ITEM,   0, 88,  40,  169}, /* id 169  Bomb Nuts */
    {ITEMKIND_ITEM,   0, 89,  41,  170}, /* id 170  Poisonous Apple */
    {ITEMKIND_ITEM,   0, 90,  42,  171}, /* id 171  Mellow Banana */
    {ITEMKIND_ITEM,   0, 91,  43,  172}, /* id 172  Medusa Powder */
    {ITEMKIND_ITEM,   0, 92,  44,  173}, /* id 173  Hardening Powder */
    {ITEMKIND_ITEM,   0, 93,  45,  174}, /* id 174  Warp Powder */
    {ITEMKIND_ITEM,   0, 94,  46,  175}, /* id 175  Stand-in Powder */
    {ITEMKIND_ITEM,   0, 95,  47,  176}, /* id 176  Escape Powder */
    {ITEMKIND_ITEM,   0, 96,  48,  177}, /* id 177  Revival Powder */
    {ITEMKIND_ITEM,   0, 97,  49,  178}, /* id 178  Repair Powder */
    {ITEMKIND_ITEM,   0, 98,  50,  179}, /* id 179  Powerup Powder */
    {ITEMKIND_ITEM,   0, 99,  51,  180}, /* id 180  Pocket */
    {ITEMKIND_ITEM,   0, 100, 52,  181}, /* id 181  Fruit of Eden */
    {ITEMKIND_ITEM,   0, 101, 53,  182}, /* id 182  Treasure Key */
    {ITEMKIND_ITEM,   0, 102, 54,  183}, /* id 183  Gourd */
    {ITEMKIND_ITEM,   0, 103, 55,  184}, /* id 184  Auto Repair Powder */
    {ITEMKIND_ITEM,   0, 104, 56,  185}, /* id 185 */
    {ITEMKIND_ITEM,   0, 105, 57,  186}, /* id 186  Fishing Rod */
    {ITEMKIND_ITEM,   0, 106, 58,  187}, /* id 187  Carrot */
    {ITEMKIND_ITEM,   0, 107, 59,  188}, /* id 188  Potato cake */
    {ITEMKIND_ITEM,   0, 108, 60,  189}, /* id 189  Minon */
    {ITEMKIND_ITEM,   0, 109, 61,  190}, /* id 190  Battan */
    {ITEMKIND_ITEM,   0, 110, 62,  191}, /* id 191  Petite Fish */
    {ITEMKIND_ITEM,   0, 111, 63,  192}, /* id 192  Saving Book */
    {ITEMKIND_ITEM,   0, 112, 64,  193}, /* id 193  Gold Bullion */
    {ITEMKIND_ITEM,   0, 113, 65,  194}, /* id 194  Evy */
    {ITEMKIND_ITEM,   0, 114, 66,  195}, /* id 195 */
    {ITEMKIND_ITEM,   0, 115, 67,  196}, /* id 196  Dran's Crest */
    {ITEMKIND_ITEM,   0, 116, 68,  197}, /* id 197  Shiny Stone */
    {ITEMKIND_ITEM,   0, 117, 69,  198}, /* id 198  Mimi */
    {ITEMKIND_ITEM,   0, 118, 70,  199}, /* id 199  Red Berry */
    {ITEMKIND_ITEM,   0, 119, 71,  200}, /* id 200  Prickly */
    {ITEMKIND_ITEM,   0, 120, 72,  201}, /* id 201  Candy */
    {ITEMKIND_ITEM,   0, 121, 73,  202}, /* id 202  Hook */
    {ITEMKIND_ITEM,   0, 122, 74,  203}, /* id 203  King's Slate */
    {ITEMKIND_ITEM,   0, 123, 75,  204}, /* id 204  Gun Powder */
    {ITEMKIND_ITEM,   0, 124, 76,  205}, /* id 205  Clock Hands */
    {ITEMKIND_ITEM,   0, 125, 77,  206}, /* id 206  Pointy Chestnut */
    {ITEMKIND_ITEM,   0, 126, 78,  207}, /* id 207  Black Knight Crest */
    {ITEMKIND_ITEM,   0, 127, 79,  208}, /* id 208  Horned Key */
    {ITEMKIND_ITEM,   0, 128, 80,  209}, /* id 209  Moon Grass Seed */
    {ITEMKIND_ITEM,   0, 129, 81,  210}, /* id 210  Music Box Key */
    {ITEMKIND_ITEM,   0, 130, 82,  211}, /* id 211  Sun Signet */
    {ITEMKIND_ITEM,   0, 131, 83,  212}, /* id 212  Moon Signet */
    {ITEMKIND_ITEM,   0, 132, 84,  213}, /* id 213  Admission Ticket */
    {ITEMKIND_ITEM,   0, 133, 85,  214}, /* id 214  Sun Sword */
    {ITEMKIND_ITEM,   0, 134, 86,  215}, /* id 215 */
    {ITEMKIND_ITEM,   0, 135, 87,  216}, /* id 216 */
    {ITEMKIND_ITEM,   0, 136, 88,  217}, /* id 217  Bone Key */
    {ITEMKIND_ITEM,   0, 137, 89,  218}, /* id 218  Moustache Key */
    {ITEMKIND_ITEM,   0, 138, 90,  219}, /* id 219  Shipcabin Key */
    {ITEMKIND_ITEM,   0, 139, 91,  220}, /* id 220  Stone Key */
    {ITEMKIND_ITEM,   0, 140, 92,  221}, /* id 221  Handle */
    {ITEMKIND_ITEM,   0, 141, 93,  222}, /* id 222  Pitchdark Key */
    {ITEMKIND_ITEM,   0, 142, 94,  223}, /* id 223  Silver Key */
    {ITEMKIND_ITEM,   0, 143, 95,  224}, /* id 224 */
    {ITEMKIND_ITEM,   0, 144, 96,  225}, /* id 225  Tram Oil */
    {ITEMKIND_ITEM,   0, 145, 97,  226}, /* id 226  Sun Dew */
    {ITEMKIND_ITEM,   0, 146, 98,  227}, /* id 227  Flapping Fish */
    {ITEMKIND_ITEM,   0, 147, 99,  228}, /* id 228  Rotten Fish */
    {ITEMKIND_ITEM,   0, 148, 100, 229}, /* id 229  Secret Path Key */
    {ITEMKIND_ITEM,   0, 149, 101, 230}, /* id 230  Bravery Launch */
    {ITEMKIND_ITEM,   0, 150, 102, 231}, /* id 231  Flapping Duster */
    {ITEMKIND_ITEM,   0, 151, 103, 232}, /* id 232  Crystal Eyeball */
    {ITEMKIND_ITEM,   0, 152, 104, 233}, /* id 233 */
    {ITEMKIND_ITEM,   0, 153, 105, 234}, /* id 234  Map */
    {ITEMKIND_ITEM,   0, 154, 106, 235}, /* id 235  Magical Crystal */
    {ITEMKIND_ITEM,   0, 155, 107, 236}, /* id 236  Dran's Feather */
    {ITEMKIND_ITEM,   0, 156, 108, 237}, /* id 237  Cave Key */
    {ITEMKIND_ITEM,   0, 157, 109, 238}, /* id 238  Changing Potion */
    {ITEMKIND_ITEM,   0, 158, 110, 239}, /* id 239  Worldmap */
    {ITEMKIND_ITEM,   0, 159, 111, 240}, /* id 240  Bone Pendant */
    {ITEMKIND_ITEM,   0, 160, 112, 241}, /* id 241  Odd Tone Flute */
    {ITEMKIND_ITEM,   0, 161, 113, 242}, /* id 242  Magical Lamp */
    {ITEMKIND_ITEM,   0, 162, 114, 243}, /* id 243  Moon Orb */
    {ITEMKIND_ITEM,   0, 163, 115, 244}, /* id 244  Shell Ring */
    {ITEMKIND_ITEM,   0, 164, 116, 245}, /* id 245  Search Warrant */
    {ITEMKIND_ITEM,   0, 165, 117, 246}, /* id 246  Ice Block */
    {ITEMKIND_ITEM,   0, 166, 118, 247}, /* id 247  Small Ice */
    {ITEMKIND_ITEM,   0, 167, 119, 248}, /* id 248  Tiny Ice */
    {ITEMKIND_ITEM,   0, 168, 120, 249}, /* id 249  Flame Key */
    {ITEMKIND_ITEM,   0, 169, 121, 250}, /* id 250  Hunter's Earring */
    {ITEMKIND_ITEM,   0, 170, 122, 251}, /* id 251  Ointment Leaf */
    {ITEMKIND_ITEM,   0, 171, 123, 252}, /* id 252  Foundation */
    {ITEMKIND_ITEM,   0, 172, 124, 253}, /* id 253  Clay Doll */
    {ITEMKIND_ITEM,   0, 173, 125, 254}, /* id 254  Manual */
    {ITEMKIND_ITEM,   0, 174, 126, 255}, /* id 255  Sun Sphere */
    {ITEMKIND_ITEM,   0, 175, 127, -1 }, /* id 256 */
    {ITEMKIND_WEAPON, 0, 0,   0,   1  }, /* id 257 */
    {ITEMKIND_WEAPON, 0, 1,   1,   2  }, /* id 258  Dagger[broken1] */
    {ITEMKIND_WEAPON, 0, 2,   2,   3  }, /* id 259  Dagger */
    {ITEMKIND_WEAPON, 0, 3,   3,   4  }, /* id 260  Baselard */
    {ITEMKIND_WEAPON, 0, 4,   4,   5  }, /* id 261  Gladius */
    {ITEMKIND_WEAPON, 0, 5,   5,   6  }, /* id 262  Wise Owl Sword */
    {ITEMKIND_WEAPON, 0, 6,   6,   7  }, /* id 263  Crysknife */
    {ITEMKIND_WEAPON, 0, 7,   7,   8  }, /* id 264  Antique Sword */
    {ITEMKIND_WEAPON, 0, 8,   8,   9  }, /* id 265  Buster Sword */
    {ITEMKIND_WEAPON, 0, 9,   9,   10 }, /* id 266  Kitchen Knife */
    {ITEMKIND_WEAPON, 0, 10,  10,  11 }, /* id 267  Tsukikage */
    {ITEMKIND_WEAPON, 0, 11,  11,  12 }, /* id 268  Sun Sword */
    {ITEMKIND_WEAPON, 0, 12,  12,  13 }, /* id 269  Serpent Sword */
    {ITEMKIND_WEAPON, 0, 13,  13,  14 }, /* id 270  Macho Sword */
    {ITEMKIND_WEAPON, 0, 14,  14,  15 }, /* id 271  Shamshir */
    {ITEMKIND_WEAPON, 0, 15,  15,  16 }, /* id 272  Heaven's Cloud */
    {ITEMKIND_WEAPON, 0, 16,  16,  17 }, /* id 273  Lamb's Sword */
    {ITEMKIND_WEAPON, 0, 17,  17,  18 }, /* id 274  Dark Cloud */
    {ITEMKIND_WEAPON, 0, 18,  18,  19 }, /* id 275  Brave Ark */
    {ITEMKIND_WEAPON, 0, 19,  19,  20 }, /* id 276  Big Bang */
    {ITEMKIND_WEAPON, 0, 20,  20,  256}, /* id 277  Almighty Pass */
    {ITEMKIND_WEAPON, 0, 21,  21,  257}, /* id 278  weapon No.277 */
    {ITEMKIND_WEAPON, 0, 22,  22,  258}, /* id 279  Mardan Eins */
    {ITEMKIND_WEAPON, 0, 23,  23,  259}, /* id 280  Mardan Twei */
    {ITEMKIND_WEAPON, 0, 24,  24,  260}, /* id 281  Arise Mardan */
    {ITEMKIND_WEAPON, 0, 25,  25,  261}, /* id 282  Aga's Sword */
    {ITEMKIND_WEAPON, 0, 26,  26,  262}, /* id 283  Evilcise */
    {ITEMKIND_WEAPON, 0, 27,  27,  263}, /* id 284  Small Sword */
    {ITEMKIND_WEAPON, 0, 28,  28,  264}, /* id 285  Sand Breaker */
    {ITEMKIND_WEAPON, 0, 29,  29,  265}, /* id 286  Drain Seeker */
    {ITEMKIND_WEAPON, 0, 30,  30,  266}, /* id 287  Chopper */
    {ITEMKIND_WEAPON, 0, 31,  31,  267}, /* id 288  Choora */
    {ITEMKIND_WEAPON, 0, 32,  32,  268}, /* id 289  Claymore */
    {ITEMKIND_WEAPON, 0, 33,  33,  269}, /* id 290  Maneater */
    {ITEMKIND_WEAPON, 0, 34,  34,  270}, /* id 291  Bone Rapier */
    {ITEMKIND_WEAPON, 0, 35,  35,  271}, /* id 292  Sax */
    {ITEMKIND_WEAPON, 0, 36,  36,  272}, /* id 293  7Branch Sword */
    {ITEMKIND_WEAPON, 0, 37,  37,  273}, /* id 294  Dusack */
    {ITEMKIND_WEAPON, 0, 38,  38,  284}, /* id 295  Swallow */
    {ITEMKIND_WEAPON, 0, 39,  39,  285}, /* id 296  7thHeaven */
    {ITEMKIND_WEAPON, 0, 40,  40,  286}, /* id 297  Sword Of Zeus */
    {ITEMKIND_WEAPON, 0, 41,  41,  287}, /* id 298  Chronicle Sword */
    {ITEMKIND_WEAPON, 0, 42,  42,  21 }, /* id 299  Atlamillia Sword */
    {ITEMKIND_WEAPON, 0, 43,  43,  22 }, /* id 300  WoodenSlingshot[broken1] */
    {ITEMKIND_WEAPON, 0, 44,  44,  23 }, /* id 301  Wooden Slingshot */
    {ITEMKIND_WEAPON, 0, 45,  45,  24 }, /* id 302  Steel Slingshot */
    {ITEMKIND_WEAPON, 0, 46,  46,  25 }, /* id 303  Bandit Slingshot */
    {ITEMKIND_WEAPON, 0, 47,  47,  26 }, /* id 304  Steve */
    {ITEMKIND_WEAPON, 0, 48,  48,  27 }, /* id 305  Bone Slingshot */
    {ITEMKIND_WEAPON, 0, 49,  49,  28 }, /* id 306  Hardshooter */
    {ITEMKIND_WEAPON, 0, 50,  50,  29 }, /* id 307  Double Impact */
    {ITEMKIND_WEAPON, 0, 51,  51,  30 }, /* id 308  Dragon's Y */
    {ITEMKIND_WEAPON, 0, 52,  52,  31 }, /* id 309  DivineBeastTitle */
    {ITEMKIND_WEAPON, 0, 53,  53,  274}, /* id 310  Cross Hinder */
    {ITEMKIND_WEAPON, 0, 54,  54,  275}, /* id 311  Flamingo */
    {ITEMKIND_WEAPON, 0, 55,  55,  288}, /* id 312  Chronicle2 */
    {ITEMKIND_WEAPON, 0, 56,  56,  289}, /* id 313  Super Steve */
    {ITEMKIND_WEAPON, 0, 57,  57,  33 }, /* id 314  11 */
    {ITEMKIND_WEAPON, 0, 58,  58,  34 }, /* id 315  Mallet[broken1] */
    {ITEMKIND_WEAPON, 0, 59,  59,  35 }, /* id 316  Mallet */
    {ITEMKIND_WEAPON, 0, 60,  60,  36 }, /* id 317  Steel Hammer */
    {ITEMKIND_WEAPON, 0, 61,  61,  37 }, /* id 318  Magical Hammer */
    {ITEMKIND_WEAPON, 0, 62,  62,  38 }, /* id 319  Battle Ax */
    {ITEMKIND_WEAPON, 0, 63,  63,  39 }, /* id 320  Turtle Shell */
    {ITEMKIND_WEAPON, 0, 64,  64,  40 }, /* id 321  Big Bucks Hammer */
    {ITEMKIND_WEAPON, 0, 65,  65,  41 }, /* id 322  Frozen Tuna */
    {ITEMKIND_WEAPON, 0, 66,  66,  42 }, /* id 323  Gaia Hammer */
    {ITEMKIND_WEAPON, 0, 67,  67,  43 }, /* id 324  Last Judgement */
    {ITEMKIND_WEAPON, 0, 68,  68,  44 }, /* id 325  Tall Hammer */
    {ITEMKIND_WEAPON, 0, 69,  69,  45 }, /* id 326  Satan's Ax */
    {ITEMKIND_WEAPON, 0, 70,  70,  276}, /* id 327  Matador */
    {ITEMKIND_WEAPON, 0, 71,  71,  277}, /* id 328  Plate Hammer */
    {ITEMKIND_WEAPON, 0, 72,  72,  290}, /* id 329  Ange Gear */
    {ITEMKIND_WEAPON, 0, 73,  73,  0  }, /* id 330 */
    {ITEMKIND_WEAPON, 0, 74,  74,  46 }, /* id 331  12 */
    {ITEMKIND_WEAPON, 0, 75,  75,  47 }, /* id 332  Gold Ring[broken1] */
    {ITEMKIND_WEAPON, 0, 76,  76,  48 }, /* id 333  Gold Ring */
    {ITEMKIND_WEAPON, 0, 77,  77,  49 }, /* id 334  Bandit's Ring */
    {ITEMKIND_WEAPON, 0, 78,  78,  50 }, /* id 335  Crystal Ring */
    {ITEMKIND_WEAPON, 0, 79,  79,  51 }, /* id 336  Platinum Ring */
    {ITEMKIND_WEAPON, 0, 80,  80,  52 }, /* id 337  Goddess Ring */
    {ITEMKIND_WEAPON, 0, 81,  81,  53 }, /* id 338  Fairy's Ring */
    {ITEMKIND_WEAPON, 0, 82,  82,  54 }, /* id 339  Destruction Ring */
    {ITEMKIND_WEAPON, 0, 83,  83,  55 }, /* id 340  Satan's Ring */
    {ITEMKIND_WEAPON, 0, 84,  84,  56 }, /* id 341  Athena's Armlet */
    {ITEMKIND_WEAPON, 0, 85,  85,  57 }, /* id 342  Mobius Ring */
    {ITEMKIND_WEAPON, 0, 86,  86,  278}, /* id 343  Trial Hammer */
    {ITEMKIND_WEAPON, 0, 87,  87,  279}, /* id 344  Pocklekul */
    {ITEMKIND_WEAPON, 0, 88,  88,  291}, /* id 345  Inferno */
    {ITEMKIND_WEAPON, 0, 89,  89,  0  }, /* id 346 */
    {ITEMKIND_WEAPON, 0, 90,  90,  58 }, /* id 347  11 */
    {ITEMKIND_WEAPON, 0, 91,  91,  59 }, /* id 348  Fighting Stick[broken1] */
    {ITEMKIND_WEAPON, 0, 92,  92,  60 }, /* id 349  Fighting Stick */
    {ITEMKIND_WEAPON, 0, 93,  93,  61 }, /* id 350  Javelin */
    {ITEMKIND_WEAPON, 0, 94,  94,  62 }, /* id 351  Halbert */
    {ITEMKIND_WEAPON, 0, 95,  95,  63 }, /* id 352  De Sanga */
    {ITEMKIND_WEAPON, 0, 96,  96,  64 }, /* id 353  Scorpion */
    {ITEMKIND_WEAPON, 0, 97,  97,  65 }, /* id 354  Partisan */
    {ITEMKIND_WEAPON, 0, 98,  98,  66 }, /* id 355  Mirage */
    {ITEMKIND_WEAPON, 0, 99,  99,  67 }, /* id 356  Terra Sword */
    {ITEMKIND_WEAPON, 0, 100, 100, 68 }, /* id 357  Hercules' Wrath */
    {ITEMKIND_WEAPON, 0, 101, 101, 69 }, /* id 358  Babel's Spear */
    {ITEMKIND_WEAPON, 0, 102, 102, 280}, /* id 359  Thorn Armlet */
    {ITEMKIND_WEAPON, 0, 103, 103, 281}, /* id 360  5 Foot Nail */
    {ITEMKIND_WEAPON, 0, 104, 104, 0  }, /* id 361 */
    {ITEMKIND_WEAPON, 0, 105, 105, 0  }, /* id 362 */
    {ITEMKIND_WEAPON, 0, 106, 106, 70 }, /* id 363  11 */
    {ITEMKIND_WEAPON, 0, 107, 107, 71 }, /* id 364  Machine Gun[broken1] */
    {ITEMKIND_WEAPON, 0, 108, 108, 72 }, /* id 365  Machine Gun */
    {ITEMKIND_WEAPON, 0, 109, 109, 73 }, /* id 366  Jackal */
    {ITEMKIND_WEAPON, 0, 110, 110, 74 }, /* id 367  Launcher */
    {ITEMKIND_WEAPON, 0, 111, 111, 75 }, /* id 368  LauncherV2 */
    {ITEMKIND_WEAPON, 0, 112, 112, 76 }, /* id 369  Blessing Gun */
    {ITEMKIND_WEAPON, 0, 113, 113, 77 }, /* id 370  Skunk */
    {ITEMKIND_WEAPON, 0, 114, 114, 78 }, /* id 371  G CRUSHER */
    {ITEMKIND_WEAPON, 0, 115, 115, 79 }, /* id 372  Hexa Blaster */
    {ITEMKIND_WEAPON, 0, 116, 116, 80 }, /* id 373  Star Breaker */
    {ITEMKIND_WEAPON, 0, 117, 117, 282}, /* id 374  Cactus */
    {ITEMKIND_WEAPON, 0, 118, 118, 283}, /* id 375  Snail */
    {ITEMKIND_WEAPON, 0, 119, 119, 0  }, /* id 376 */
};

WEAPON_DATA *GetWeaponDataInfo(int weapon_no) {
    if (weapon_no < 0) {
        return nullptr;
    }

    return &WeaponList[weapon_no];
}

// ComItemInfo holds items 81 to 376; PresetSmallItemNo_Get asks about weapons up to 379, so
// anything outside the table yields nullptr.
COM_ITEM_INFO *GetCommonItemInfo(int item_no) {
    if (item_no <= 0) {
        return nullptr;
    }

    // Items before slot 81 function as an alias for weapons in the info table.
    if (0 < item_no && item_no < ITEM_ATTACH_START) {
        item_no += ITEM_WEAPON_START - 1 - ITEM_ATTACH_START;
    } else {
        item_no -= ITEM_ATTACH_START;
    }

    if (std::cmp_greater_equal(item_no, std::size(ComItemInfo))) {
        return nullptr;
    }

    return &ComItemInfo[item_no];
}

// The kind and index checks stop an invalid ID or corrupt table entry from addressing a
// neighbouring item table.
int GetItemTypeInfo(int item_no, s8 *kind) {
    if (kind == nullptr) {
        return -1;
    }

    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr) {
        *kind = -1;
        return -1;
    }

    *kind = info->kind;
    return info->index;
}

ITEM_DATA *GetItemData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_ITEM || info->index < 0 || std::cmp_greater_equal(info->index, std::size(ITEM_LIST))) {
        return nullptr;
    }

    return &ITEM_LIST[info->index];
}

ATTACH_DATA *GetAttachData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_ATTACH || info->index < 0 || std::cmp_greater_equal(info->index, std::size(AttachList))) {
        return nullptr;
    }

    return &AttachList[info->index];
}

WEAPON_DATA *GetWeaponData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_WEAPON || info->index < 0 || std::cmp_greater_equal(info->index, std::size(WeaponList))) {
        return nullptr;
    }

    return &WeaponList[info->index];
}

int GetCommonItemDataMsg(int item_no) {
    const COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    return info != nullptr ? info->msg : 0;
}

int GetCommonItemDataSystemMsg(int item_no) {
    return GetCommonItemDataMsg(item_no) + 100;
}

} // namespace dc::game
