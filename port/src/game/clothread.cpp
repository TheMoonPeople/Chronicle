#include "common.h"

#include <libpkt.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <numbers>
#include <print>
#include <string>
#include <string_view>

#include "bound.hpp"
#include "btsysscript.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "framevu1.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/texture_registry.hpp"
#include "savedata.hpp"
#include "scriptinterpreter.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "sysmes.hpp"
#include "visualvu1.hpp"

namespace dc::game {

using namespace dc::render;

CCloth *pCloth;

namespace {

/**
 * Raw bytes of one parsed command argument: a NUL-terminated string, an int or a float.
 */
using ArgumentSlot = std::array<char, 256>;

/**
 * Argument slots shared by every command of one cloth configuration.
 *
 * Slots keep their bytes between commands, so a command whose arguments failed to parse
 * sees whatever the previous command left there.
 */
using ArgumentList = std::array<ArgumentSlot, 16>;

/**
 * Last exclusion bound attached while reading one cloth configuration.
 */
CBound *pBound;

/**
 * Model frame whose cloth configuration is being read.
 */
CFrameVu1 *ParentFrame;

/**
 * Allocator used for the cloth currently being initialized.
 */
CDataAlloc2<1> *DataBuffer;

int ReadInteger(const ArgumentSlot &slot) {
    int value = 0;
    std::memcpy(&value, slot.data(), sizeof(value));
    return value;
}

float ReadFloat(const ArgumentSlot &slot) {
    float value = 0.0f;
    std::memcpy(&value, slot.data(), sizeof(value));
    return value;
}

void WriteInteger(ArgumentSlot &slot, int value) {
    std::memcpy(slot.data(), &value, sizeof(value));
}

void WriteFloat(ArgumentSlot &slot, float value) {
    std::memcpy(slot.data(), &value, sizeof(value));
}

/**
 * Sets and clamps the cloth grid dimensions.
 */
void CommandSIZE(ArgumentList &arguments) {
    pCloth->num_i = std::clamp(ReadInteger(arguments[0]), 1, 16);
    pCloth->num_j = std::clamp(ReadInteger(arguments[1]), 1, 16);
}

/**
 * Attaches the cloth to the frame named by the command.
 */
void CommandFRAME(ArgumentList &arguments) {
    auto *frame = static_cast<CFrameVu1 *>(ParentFrame->SearchFrame(arguments[0].data()));

    pCloth->frame = frame;

    if (frame != nullptr) {
        frame->attr.draw_on = 2;
        CVisualVu1 *visual = frame->GetVisual();
        pCloth->Initialize(reinterpret_cast<MDT_HEADER *>(visual->GetMDTDataAddress()), DataBuffer);
    }
}

/**
 * Sets the scale used when calculating cloth normals.
 */
void CommandNORMAL(ArgumentList &arguments) {
    pCloth->normal_scale = ReadFloat(arguments[0]);
}

/**
 * Sets the cloth follow vector.
 */
void CommandFOLLOW(ArgumentList &arguments) {
    pCloth->follow[0] = ReadFloat(arguments[0]);
    pCloth->follow[1] = ReadFloat(arguments[1]);
    pCloth->follow[2] = ReadFloat(arguments[2]);
}

/**
 * Sets the cloth stiffness vector.
 */
void CommandK(ArgumentList &arguments) {
    pCloth->stiffness[0] = ReadFloat(arguments[0]);
    pCloth->stiffness[1] = ReadFloat(arguments[1]);
    pCloth->stiffness[2] = ReadFloat(arguments[2]);
}

/**
 * Sets the cloth response to wind.
 */
void CommandWINDEFFECT(ArgumentList &arguments) {
    pCloth->wind_effect = ReadFloat(arguments[0]);
}

/**
 * Sets the cloth gravity vector.
 */
void CommandGRAVITY(ArgumentList &arguments) {
    pCloth->gravity[0] = ReadFloat(arguments[0]);
    pCloth->gravity[1] = ReadFloat(arguments[1]);
    pCloth->gravity[2] = ReadFloat(arguments[2]);
}

/**
 * Selects which cloth rows divide their polygons.
 */
void CommandPOLYDIVE(ArgumentList &arguments) {
    const ArgumentSlot &flags = arguments[0];

    for (int row = 0; row < 16 && flags[row] != 0; row++) {
        pCloth->polygon_divide[row] = (flags[row] != '0');
    }
}

/**
 * Creates and attaches an exclusion bound for the cloth.
 */
void CommandBOUND(ArgumentList &arguments) {
    sceVu0FVECTOR box_vectors[4];
    auto         *bound = new (reinterpret_cast<u_long128 *>(DataBuffer->Alloc(0x14))) CBound(1.0f, 1.0f, 1.0f);

    if (bound == nullptr) {
        return;
    }

    CFrame *frame = ParentFrame->SearchFrame(arguments[0].data());

    if (frame == nullptr) {
        return;
    }

    int arg_index = 1;

    for (auto &box_vector : box_vectors) {
        box_vector[0] = ReadFloat(arguments[arg_index]);
        box_vector[1] = ReadFloat(arguments[arg_index + 1]);
        box_vector[2] = ReadFloat(arguments[arg_index + 2]);
        arg_index += 3;
        box_vector[3] = 1.0f;
    }

    box_vectors[0][3] = 0.0f;
    bound->SetDir(frame, box_vectors[1], box_vectors[2], box_vectors[0], box_vectors[3][0], box_vectors[3][1]);
    const float half_x = box_vectors[3][0];
    const float half_y = box_vectors[3][1];
    const float half_z = box_vectors[3][2];
    bound->extent[0] = half_x;
    bound->extent[1] = half_y;
    bound->extent[2] = half_z;

    if (bound->extent[0] > 0.0f) {
        bound->reciprocal[0] = 1.0f / half_x;
    }

    if (bound->extent[1] > 0.0f) {
        bound->reciprocal[1] = 1.0f / half_y;
    }

    if (bound->extent[2] > 0.0f) {
        bound->reciprocal[2] = 1.0f / half_z;
    }

    bound->friction = ReadFloat(arguments[arg_index]);

    if (pBound == nullptr) {
        pCloth->bound = bound;
    } else {
        pBound->next = bound;
    }

    pBound = bound;
}

/**
 * Names one keyword of the cloth configuration file.
 *
 * The argument list gives each argument's SCRIPT_ARGUMENT_TYPE and ends at -1.
 */
struct COMMAND_INFO {
    std::string_view    name;      /**< Keyword the line starts with. */
    std::array<int, 16> arg_types; /**< Kind of each argument, ended by -1. */
};

constexpr std::array<COMMAND_INFO, 9> Command = {
    {
     {.name = "SIZE", .arg_types = {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}},
     {.name = "FRAME", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "NORMAL", .arg_types = {SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "FOLLOW", .arg_types = {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "K", .arg_types = {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "WINDEFFECT", .arg_types = {SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "GRAVITY", .arg_types = {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
     {.name = "POLYDIV", .arg_types = {SCRIPT_ARGUMENT_STRING, -1}},
     {.name = "BOUND",
         .arg_types = {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT,
                       SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT,
                       SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT,
                       SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
     }
};

constexpr std::array<void (*)(ArgumentList &), 9> CommandExe = {
    CommandSIZE,
    CommandFRAME,
    CommandNORMAL,
    CommandFOLLOW,
    CommandK,
    CommandWINDEFFECT,
    CommandGRAVITY,
    CommandPOLYDIVE,
    CommandBOUND,
};

/**
 * Reports whether a character is not whitespace.
 */
bool CheckChar(char ch) {
    return ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r';
}

/**
 * Steps past whitespace and reports whether any text remains.
 */
bool SkipSpace(input_str &input) {
    int pos = input.pos;

    while (pos < input.size && !CheckChar(input.data[pos])) {
        pos++;
    }

    input.pos = pos;
    return pos < input.size;
}

/**
 * Reads the next word, ended by whitespace (or a comma when stop_at_comma is set).
 *
 * Returns false if the text runs out first.
 */
bool ReadWord(input_str &input, bool stop_at_comma, std::string &word) {
    int ch = 0;

    while (true) {
        if (input.get(&ch) == 0) {
            return false;
        }

        if ((stop_at_comma && ch == ',') || !CheckChar(static_cast<char>(ch))) {
            return true;
        }

        word.push_back(static_cast<char>(ch));
    }
}

/**
 * Reads the arguments of one command into the argument slots.
 *
 * Returns 1 on success, 0 when the text runs out, and -1 on a malformed argument.
 */
int GetArg(input_str &input, const std::array<int, 16> &arg_types, ArgumentList &arguments) {
    if (!SkipSpace(input)) {
        return 0;
    }

    int arg_count = 0;

    while (arg_types[arg_count++] >= 0) {
    }

    for (int i = 0; i < arg_count - 1; i++) {
        if (!SkipSpace(input)) {
            return 0;
        }

        std::string word;

        if (!ReadWord(input, true, word)) {
            return 0;
        }

        // A NUL byte in the text ends the word.
        const std::string_view text = std::string_view(word).substr(0, word.find('\0'));
        ArgumentSlot          &slot = arguments[i];

        switch (arg_types[i]) {
            case SCRIPT_ARGUMENT_STRING: {
                if (!text.starts_with('"')) {
                    return -1;
                }

                const std::size_t closing_quote = text.find('"', 1);

                if (closing_quote == std::string_view::npos) {
                    return -1;
                }

                const std::size_t length = std::min(closing_quote - 1, slot.size() - 1);
                text.copy(slot.data(), length, 1);
                slot[length] = 0;
                break;
            }
            case SCRIPT_ARGUMENT_INTEGER:
                if (!std::ranges::all_of(text, [](char letter) { return letter >= '0' && letter <= '9'; })) {
                    return -1;
                }

                WriteInteger(slot, std::atoi(word.c_str()));
                break;
            case SCRIPT_ARGUMENT_FLOAT:
                if (!std::ranges::all_of(text, [](char letter) {
                        return (letter >= '0' && letter <= '9') || letter == '.' || letter == '-';
                    })) {
                    return -1;
                }

                WriteFloat(slot, static_cast<float>(std::atof(word.c_str())));
                break;
            default:
                return -1;
        }
    }

    return 1;
}

/**
 * Reads the next keyword and looks it up in the command table.
 *
 * An unknown keyword yields an index past the table. Returns false when the text runs out.
 */
bool SearchCommand(input_str &input, int &command) {
    if (!SkipSpace(input)) {
        return false;
    }

    std::string word;

    if (!ReadWord(input, false, word)) {
        return false;
    }

    const std::string_view text = std::string_view(word).substr(0, word.find('\0'));
    const auto *const      found = std::ranges::find(Command, text, &COMMAND_INFO::name);

    command = found != Command.end() ? static_cast<int>(found - Command.begin()) : 10;
    return true;
}

} // namespace

CCloth *InitCloth(CFrameVu1 *frame, input_str &input, CDataAlloc2<1> *alloc) {
    DataBuffer = alloc;
    pCloth = new (reinterpret_cast<u_long128 *>(alloc->Alloc(0x856))) CCloth(16, 16, 1.0f);
    ParentFrame = frame;
    pBound = nullptr;

    if (frame == nullptr) {
        return nullptr;
    }

    if (pCloth == nullptr) {
        return nullptr;
    }

    SkipSpace(input);

    const auto arguments = std::make_unique<ArgumentList>();
    int        command = 0;

    while (SearchCommand(input, command)) {
        if (command >= 9 || command < 0) {
            std::println("unknown command!!");
            continue;
        }

        const int arg_status = GetArg(input, Command[command].arg_types, *arguments);

        if (arg_status == 0) {
            break;
        }

        if (arg_status < 0) {
            std::println("error!!");
        }

        CommandExe[command](*arguments);
    }

    return pCloth;
}

int keyCtrl(float stick_x, float stick_y, MOTION_INFO *motion) {
    int move_state = 0;

    if (stick_y != 0.0f || stick_x != 0.0f) {
        move_state = 2;

        if (stick_x < 0.0f) {
            stick_x *= -1.0f;
        }

        if (stick_y < 0.0f) {
            stick_y *= -1.0f;
        }

        const float tilt = stick_x >= stick_y ? stick_x : stick_y;
        const float speed = 0.8f * (0.1f + tilt);
        motion[2].speed = speed;

        if (speed >= 0.7f) {
            motion[2].speed = 0.7f;
        }

        if (stick_x + stick_y >= 0.85f) {
            move_state = 1;
        }
    }

    return move_state;
}

// A local-to-local GS transfer between two images the registry knows by base pointer. The direction
// only orders overlapping transfers within one buffer, which the renderer's copy does not need.
void MoveImageTest(sceVif1Packet * /*packet*/, int src_base, int /*src_width*/, int src_format, const CRect_i_ &rect,
                   int dst_base, int /*dst_width*/, int dst_format, int dst_x, int dst_y, int /*direction*/) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    PortMoveImage(static_cast<unsigned>(src_base), static_cast<unsigned>(src_format), rect.x, rect.y, rect.width,
                  rect.height, static_cast<unsigned>(dst_base), static_cast<unsigned>(dst_format), dst_x, dst_y);
}

float unitRotation(CFrameVu1 *frame, float heading) {
    std::array<float, 4> rotation{};
    float                abs_delta = 0.0f;

    frame->GetRotation(rotation.data());
    float delta = heading - rotation[1];

    if (delta <= 0.0f) {
        abs_delta = -1.0f * delta;
    } else {
        abs_delta = delta;
    }

    // The comparisons and the wrap-around are done in double precision.
    if (abs_delta <= std::numbers::pi) {
        if (abs_delta <= TWELFTH_PI_D) {
            delta = 0.0f;
        }
    } else {
        abs_delta = TWO_PI_D - abs_delta;

        if (abs_delta <= TWELFTH_PI_D) {
            delta = 0.0f;
        }
    }

    if (delta > 0.0f) {
        if (delta <= std::numbers::pi) {
            rotation[1] += TWELFTH_PI;
        } else {
            rotation[1] -= TWELFTH_PI;
        }
    }

    if (delta < 0.0f) {
        if (delta >= -std::numbers::pi) {
            rotation[1] -= TWELFTH_PI;
        } else {
            rotation[1] += TWELFTH_PI;
        }
    }

    if (0.0f == delta) {
        rotation[1] = heading;
    }

    if (rotation[1] <= -std::numbers::pi) {
        rotation[1] += TWO_PI;
    }

    if (rotation[1] >= std::numbers::pi) {
        rotation[1] -= TWO_PI;
    }

    return rotation[1];
}

} // namespace dc::game
