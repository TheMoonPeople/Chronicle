#include "title/script.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "dataread.hpp"

namespace dc::game {

namespace {

constexpr int kCommandCount = static_cast<int>(TitleScriptCommand::Count);

/* The command table the parser matches a line against, in the order it matches them: a name is
   compared by the length the row carries rather than by its own, so a name that is another's prefix
   has to stand in front of it ([title-script.md](../../docs/formats/title-script.md)). */
constexpr CSCRIPT_COMMAND Command[kCommandCount] = {
    {"OBJ_MOTION2", 11, 3, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                                            },
    {"OBJ_MOTION",  10, 3, {TSARG_VALUE, TSARG_COMMA_VALUE, 0}                                                                                            },
    {"OBJ_MOVE",    8,  2, {TSARG_VALUE, TSARG_COMMA_VALUE}                                                                                               },
    {"OBJ_TALK",    8,  7, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}},
    {"OBJ_EYE",     7,  3, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                                            },
    {"OBJ_MOUTH",   9,  3, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                                            },
    {"OBJ_DISP",    8,  2, {TSARG_VALUE, TSARG_COMMA_VALUE}                                                                                               },
    {"OBJ_STEP",    8,  2, {TSARG_VALUE, TSARG_COMMA_VALUE}                                                                                               },
    {"CAMERA",      6,  4, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                         },
    {"SE_STOP",     7,  3, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                                            },
    {"SE",          2,  4, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                         },
    {"SOUND_FADE",  10, 3, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                                            },
    {"FADE_IN",     7,  2, {TSARG_VALUE, TSARG_COMMA_VALUE}                                                                                               },
    {"FADE_OUT",    8,  2, {TSARG_VALUE, TSARG_COMMA_VALUE}                                                                                               },
    {"MESSAGE",     7,  6, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                   },
    {"NEXT_MES",    8,  1, {TSARG_VALUE}                                                                                                                  },
    {"MES_CLR",     7,  0, {0}                                                                                                                            },
    {"LOAD_OBJ",    8,  2, {TSARG_VALUE, TSARG_COMMA_VALUE}                                                                                               },
    {"SPRITE",      6,  1, {TSARG_VALUE}                                                                                                                  },
    {"BOM",         3,  4, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                                                         },
    {"BEEM",        4,  6, {TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE}                   },
    {"SCENE_LOAD",  10, 1, {TSARG_VALUE}                                                                                                                  },
    {"SCENE",       5,  1, {TSARG_VALUE}                                                                                                                  },
    {"END",         3,  0, {0}                                                                                                                            },
    {"WAIT_KEY",    8,  1, {TSARG_VALUE}                                                                                                                  },
    {"WAIT",        4,  1, {TSARG_VALUE}                                                                                                                  },
};

/* Reads ON, OFF or a number standing at the cursor into value and returns the cursor past it, or -1
   when none of the three stands there or the number runs past 32 characters. */
int ReadValue(const char *buffer, int cursor, float &value) {
    if (std::memcmp(&buffer[cursor], "ON", 2) == 0) {
        value = 1.0f;
        return cursor + 2;
    }

    if (std::memcmp(&buffer[cursor], "OFF", 3) == 0) {
        value = 0.0f;
        return cursor + 3;
    }

    if (buffer[cursor] != '-' && (buffer[cursor] < '0' || buffer[cursor] > '9')) {
        return -1;
    }

    value = static_cast<float>(std::strtod(&buffer[cursor], nullptr));

    int char_count = 0;

    for (; char_count < 32; char_count++) {
        bool accepted = false;

        if (buffer[cursor] == '-') {
            cursor++;
            accepted = true;
        }

        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
            cursor++;
            accepted = true;
        }

        if (buffer[cursor] == '.') {
            cursor++;
            accepted = true;
        }

        if (!accepted) {
            break;
        }
    }

    if (char_count == 32) {
        return -1;
    }

    return cursor;
}

} // namespace

CScript CScript__2;

/* Nothing is reset when the load fails, so a missing file leaves the previous script's state
   standing rather than an empty one. */
void CScript::Load(const char *name) {
    p = data;

    if (LoadFile(const_cast<char *>(name), p, &size) != 0) {
        wait = 0;
        mes_no = 0;
        mes_page_wait = 40;
        pos = 0;
        camera_start = -1;
        camera_no = -1;
        fade = 0;
        sprite = 0;
        end = 0;
        scene = 0;
        load_no = -1;
        mes_wait = 0;
        bom_no = 0;
        beem_no = 0;
        reset_flag = 0;
        se_no = 0;
        se_voice = 0;
        se_stop = 0;
        bgm_fade = 0;

        std::ranges::fill(arg, 0.0f);

        for (SCRIPT_OBJ &object : obj) {
            object.disp = 1;
            object.motion = 0;
            object.motion_end = -1;
            object.move = 0;
            object.move_req = 0;
            object.eye = 0;
            object.eye_time = 0;
            object.mouth = 0;
            object.mouth_time = 0;
            object.talk = 0;
            object.load = -1;
            object.load_step = TSLOAD_NONE;
            object.step = 0.05f;
        }
    }
}

/* A line matching no command is fatal: nothing in the format says where a line ends, so there is no
   way to skip it. */
void CScript::Step() {
    if (end != 0) {
        return;
    }

    if (mes_wait != 0) {
        wait = 1.0f + motion_step;
    }

    // Negated rather than >= so a NaN wait counts down instead of parsing.
    if (!(wait < 1.0f)) {
        wait = wait - motion_step;
        return;
    }

    bool done = false;
    pos = SkipSpace(p, pos);

    do {
        bool unmatched = true;

        for (int command_no = 0; command_no < kCommandCount; command_no++) {
            const CSCRIPT_COMMAND &command = Command[command_no];

            if (std::memcmp(&p[pos], command.name, command.length) == 0) {
                const auto kind = static_cast<TitleScriptCommand>(command_no);

                unmatched = false;
                pos = SkipSpace(p, pos + command.length);
                pos = CheckScript(p, pos, &command, kind);

                if (pos != -1 &&
                    (kind == TitleScriptCommand::End || kind == TitleScriptCommand::WaitKey || kind == TitleScriptCommand::Wait)) {
                    done = true;
                }

                break;
            }
        }

        if (unmatched) {
            exit__2(-1);
        }
    } while (!done);
}

/* Every case only records a request; the overlay's own per-tick code acts on it
   ([title-script.md](../../docs/formats/title-script.md)). */
int CScript::CheckScript(const char *buffer, int position, const CSCRIPT_COMMAND *command, TitleScriptCommand command_no) {
    auto object = [this](float index) -> SCRIPT_OBJ & { return obj[static_cast<int>(index)]; };

    int cursor = position;

    if (command->arg_count != 0) {
        cursor = CheckArg(buffer, cursor, command);
    }

    switch (command_no) {
        case TitleScriptCommand::ObjMotion2:
            object(arg[0]).motion = static_cast<int>(arg[1]);
            object(arg[0]).motion_end = static_cast<int>(arg[2]);
            break;

        case TitleScriptCommand::ObjMotion:
            object(arg[0]).motion = static_cast<int>(arg[1]);
            object(arg[0]).motion_end = -1;
            break;

        case TitleScriptCommand::ObjMove:
            object(arg[0]).move = static_cast<int>(arg[1]);
            object(arg[0]).move_req = 1;
            break;

        case TitleScriptCommand::ObjTalk:
            object(arg[0]).talk = 1;
            mes_no = static_cast<int>(arg[1]);
            mes_timer = arg[2];
            object(arg[0]).mouth_time = arg[2];
            mes_x = static_cast<int>(arg[3]);
            mes_y = static_cast<int>(arg[4]);
            mes_talker = static_cast<int>(arg[0]);
            mes_tail_x = static_cast<int>(arg[5]);
            mes_tail_y = static_cast<int>(arg[6]);
            break;

        case TitleScriptCommand::ObjEye:
            object(arg[0]).eye = static_cast<char>(arg[1]);
            object(arg[0]).eye_time = arg[2];
            break;

        case TitleScriptCommand::ObjMouth:
            object(arg[0]).mouth = static_cast<char>(arg[1]);
            object(arg[0]).mouth_time = arg[2];
            break;

        case TitleScriptCommand::ObjDisp:
            object(arg[0]).disp = arg[1] == 1.0f ? 1 : 0;
            break;

        case TitleScriptCommand::ObjStep:
            object(arg[0]).step = arg[1];
            break;

        case TitleScriptCommand::Camera:
            camera_no = camera_start;
            camera_start = static_cast<int>(arg[0]);
            motion_start = static_cast<int>(arg[1]);
            motion_end = static_cast<int>(arg[2]);
            motion_step = arg[3];
            motion_req = 1;
            break;

        case TitleScriptCommand::SeStop:
            se_kind = static_cast<int>(arg[0]);
            se_no = static_cast<int>(arg[1]);
            se_voice = static_cast<int>(arg[2]);
            se_stop = 1;
            break;

        case TitleScriptCommand::Se:
            se_kind = static_cast<int>(arg[0]);
            se_no = static_cast<int>(arg[1]);
            se_voice = static_cast<int>(arg[2]);
            se_fade_time = static_cast<int>(arg[3]);
            break;

        case TitleScriptCommand::SoundFade:
            se_kind = static_cast<int>(arg[0]);
            se_fade_time = static_cast<int>(arg[1]);
            bgm_fade = static_cast<int>(arg[2]);
            break;

        case TitleScriptCommand::FadeIn:
            fade = arg[0] == 0.0f ? TSFADE_IN_BLACK : TSFADE_IN_WHITE;
            fade_speed = arg[1];
            break;

        case TitleScriptCommand::FadeOut:
            fade = arg[0] == 0.0f ? TSFADE_OUT_BLACK : TSFADE_OUT_WHITE;
            fade_speed = arg[1];
            break;

        case TitleScriptCommand::Message:
            mes_no = static_cast<int>(arg[0]);
            mes_timer = arg[1];
            mes_x = static_cast<int>(arg[2]);
            mes_y = static_cast<int>(arg[3]);
            mes_talker = -1;
            mes_tail_x = static_cast<int>(arg[4]);
            mes_tail_y = static_cast<int>(arg[5]);
            break;

        case TitleScriptCommand::NextMes:
            mes_page_wait = static_cast<int>(arg[0]);
            break;

        case TitleScriptCommand::MesClr:
            mes_no = 0;
            break;

        case TitleScriptCommand::LoadObj:
            object(arg[1]).load = static_cast<int>(arg[0]);
            object(arg[1]).load_step = TSLOAD_START;
            break;

        case TitleScriptCommand::Sprite:
            sprite = static_cast<char>(arg[0]);
            break;

        case TitleScriptCommand::Bom:
            bom_no++;

            if (bom_no > 2) {
                bom_no = 0;
            }

            bom_pos[bom_no][0] = arg[0];
            bom_pos[bom_no][1] = arg[1];
            bom_pos[bom_no][2] = arg[2];
            bom_size[bom_no] = arg[3];
            bom_req = 1;
            break;

        case TitleScriptCommand::Beem:
            beem_no++;

            if (beem_no > 2) {
                beem_no = 0;
            }

            if (arg[0] == -1.0f) {
                beem_end = 1;
            } else {
                beem_end = 0;
                beem_from[beem_no][0] = arg[0];
                beem_from[beem_no][1] = arg[1];
                beem_from[beem_no][2] = arg[2];
                beem_to[beem_no][0] = arg[3];
                beem_to[beem_no][1] = arg[4];
                beem_to[beem_no][2] = arg[5];
            }

            beem_req = 1;
            break;

        case TitleScriptCommand::SceneLoad:
            load_no = static_cast<int>(arg[0]);
            break;

        case TitleScriptCommand::Scene:
            scene = static_cast<int>(arg[0]);
            init_no = static_cast<int>(arg[0]);
            break;

        case TitleScriptCommand::Wait:
            wait = arg[0] - 1.0f;
            break;

        case TitleScriptCommand::WaitKey:
            mes_wait = static_cast<int>(1.0f + arg[0]);
            break;

        case TitleScriptCommand::End:
            end = 1;
            break;

        case TitleScriptCommand::Count:
            break;
    }

    return cursor;
}

/* A comma-value argument must be preceded by a comma; a plain value stands where it is. Anything
   else hands back -1, which the caller stores and then parses from
   ([title-script.md](../../docs/formats/title-script.md)). */
int CScript::CheckArg(const char *buffer, int position, const CSCRIPT_COMMAND *command) {
    int cursor = position;

    for (int i = 0; i < command->arg_count; i++) {
        const int type = command->arg_type[i];

        if (type != TSARG_COMMA_VALUE && type != TSARG_VALUE) {
            continue;
        }

        if (type == TSARG_COMMA_VALUE) {
            if (buffer[cursor] != ',') {
                return -1;
            }

            cursor = SkipSpace(buffer, cursor + 1);
        }

        cursor = ReadValue(buffer, cursor, arg[i]);

        if (cursor == -1) {
            return -1;
        }

        cursor = SkipSpace(buffer, cursor);
    }

    return cursor;
}

/* A comment runs to its line ending inclusive and the ideographic space is two bytes, so the skip
   walks the whole file rather than one kind of byte
   ([title-script.md](../../docs/formats/title-script.md)). */
int CScript::SkipSpace(const char *buffer, int position) {
    while (position < size) {
        bool stop = true;

        if (std::memcmp(&buffer[position], "\x81\x40", 2) == 0) {
            position++;
            stop = false;
        }

        if (buffer[position] == ' ') {
            stop = false;
        }

        if (buffer[position] == '\t') {
            stop = false;
        }

        if (buffer[position] == '\n') {
            position++;
            stop = false;
        }

        if (buffer[position] == '\r') {
            position++;
            stop = false;
        }

        if (std::memcmp(&buffer[position], "//", 2) == 0) {
            while (buffer[position] != '\n' && buffer[position] != '\r') {
                position++;
            }

            position++;
            stop = false;
        }

        if (stop) {
            return position;
        }

        position++;
    }

    return size;
}

} // namespace dc::game
