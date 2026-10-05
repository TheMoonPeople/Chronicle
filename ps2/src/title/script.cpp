#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 440

#include "title/script.hpp"

#include <cstdlib>
#include <cstring>

#include "dataread.hpp"

/* The command table the parser matches a line against, in the order it matches them: a name is
   compared by the length the row carries rather than by its own, so a name that is another's prefix
   has to stand in front of it ([title-script.md](../../docs/formats/title-script.md)). */
CSCRIPT_COMMAND Command[TSC_COMMAND_COUNT] = {
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

/* The one script the overlay runs. It is 58112 bytes because the file is held whole inside it and
   re-parsed from the cursor every tick. */
class CScript CScript__2;

/* Read the whole script in and put every actor back to a state nothing has asked anything of yet.
   Nothing is reset when the load fails, so a missing file leaves the previous script's state
   standing rather than an empty one. */
void CScript::Load(const char *name) {
    int i;

    p = data;

    if (LoadFile((char *) name, p, &size)) {
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
        mes_wait = false;
        bom_no = 0;
        beem_no = 0;
        reset_flag = 0;
        se_no = 0;
        se_voice = 0;
        se_stop = false;
        bgm_fade = 0;

        for (i = 0; i < 128; i++) {
            arg[i] = 0;
        }

        for (i = 0; i < 23; i++) {
            obj[i].disp = true;
            obj[i].motion = 0;
            obj[i].motion_end = -1;
            obj[i].move = 0;
            obj[i].move_req = false;
            obj[i].eye = 0;
            obj[i].eye_time = 0;
            obj[i].mouth = 0;
            obj[i].mouth_time = 0;
            obj[i].talk = false;
            obj[i].load = -1;
            obj[i].load_step = TSLOAD_NONE;
            obj[i].step = 0.05f;
        }
    }
}

/* One tick of the script: nothing happens while the wait is still running, and when it runs out
   the parser reads one command after another out of the text until it reaches one that ends the
   tick. A line matching no row is fatal - there is no way to skip it, because nothing in the format
   says where a line ends. */
void CScript::Step() {
    bool unmatched;
    bool done;
    int  i;

    if (this->end) {
        return;
    }

    if (mes_wait) {
        wait = 1.0f + motion_step;
    }

    if (wait < 1.0f) {
        done = false;
        pos = SkipSpace(p, pos);

        do {
            unmatched = true;

            for (i = 0; i < TSC_COMMAND_COUNT; i++) {
                if (memcmp(&p[pos], Command[i].name, Command[i].length) == 0) {
                    unmatched = false;
                    pos = SkipSpace(p, pos + Command[i].length);
                    pos = CheckScript(p, pos, &Command[i], i);

                    if (pos != -1) {
                        if (i == TSC_END || i == TSC_WAIT_KEY || i == TSC_WAIT) {
                            done = true;
                        }
                    }

                    break;
                }
            }

            if (unmatched) {
                exit__2(-1);
            }
        } while (!done);
    } else {
        wait = wait - motion_step;
    }
}

/* One command, once its arguments have been read: every case is a write into the object and
   nothing acts on it here, so a command is a request the overlay's own per-tick code picks up
   ([title-script.md](../../docs/formats/title-script.md)). */
int CScript::CheckScript(char *buffer, int position, CSCRIPT_COMMAND *command, int command_no) {
    int cursor;

    cursor = position;

    if (command->arg_count) {
        cursor = CheckArg(buffer, cursor, command);
    }

    switch (command_no) {
        case TSC_OBJ_MOTION2:
            obj[(int) arg[0]].motion = (int) arg[1];
            obj[(int) arg[0]].motion_end = (int) arg[2];
            break;

        case TSC_OBJ_MOTION:
            obj[(int) arg[0]].motion = (int) arg[1];
            obj[(int) arg[0]].motion_end = -1;
            break;

        case TSC_OBJ_MOVE:
            obj[(int) arg[0]].move = (int) arg[1];
            obj[(int) arg[0]].move_req = true;
            break;

        case TSC_OBJ_TALK:
            obj[(int) arg[0]].talk = true;
            mes_no = (int) arg[1];
            mes_timer = arg[2];
            obj[(int) arg[0]].mouth_time = arg[2];
            mes_x = (int) arg[3];
            mes_y = (int) arg[4];
            mes_talker = (int) arg[0];
            mes_tail_x = (int) arg[5];
            mes_tail_y = (int) arg[6];
            break;

        case TSC_OBJ_EYE:
            obj[(int) arg[0]].eye = (char) arg[1];
            obj[(int) arg[0]].eye_time = arg[2];
            break;

        case TSC_OBJ_MOUTH:
            obj[(int) arg[0]].mouth = (char) arg[1];
            obj[(int) arg[0]].mouth_time = arg[2];
            break;

        case TSC_OBJ_DISP:
            if (arg[1] == 1.0f) {
                obj[(int) arg[0]].disp = true;
            } else {
                obj[(int) arg[0]].disp = false;
            }

            break;

        case TSC_OBJ_STEP:
            obj[(int) arg[0]].step = arg[1];
            break;

        case TSC_CAMERA:
            camera_no = camera_start;
            camera_start = (int) arg[0];
            motion_start = (int) arg[1];
            motion_end = (int) arg[2];
#ifdef PAL_TIMING
            motion_step = 1.2f * arg[3];
#else
            motion_step = arg[3];
#endif
            motion_req = true;
            break;

        case TSC_SE_STOP:
            se_kind = (int) arg[0];
            se_no = (int) arg[1];
            se_voice = (int) arg[2];
            se_stop = true;
            break;

        case TSC_SE:
            se_kind = (int) arg[0];
            se_no = (int) arg[1];
            se_voice = (int) arg[2];
            se_fade_time = (int) arg[3];
            break;

        case TSC_SOUND_FADE:
            se_kind = (int) arg[0];
            se_fade_time = (int) arg[1];
            bgm_fade = (int) arg[2];
            break;

        case TSC_FADE_IN:
            if (arg[0] == 0.0f) {
                fade = TSFADE_IN_BLACK;
            } else {
                fade = TSFADE_IN_WHITE;
            }

            fade_speed = arg[1];
            break;

        case TSC_FADE_OUT:
            if (arg[0] == 0.0f) {
                fade = TSFADE_OUT_BLACK;
            } else {
                fade = TSFADE_OUT_WHITE;
            }

            fade_speed = arg[1];
            break;

        case TSC_MESSAGE:
            mes_no = (int) arg[0];
            mes_timer = arg[1];
            mes_x = (int) arg[2];
            mes_y = (int) arg[3];
            mes_talker = -1;
            mes_tail_x = (int) arg[4];
            mes_tail_y = (int) arg[5];
            break;

        case TSC_NEXT_MES:
            mes_page_wait = (int) arg[0];
            break;

        case TSC_MES_CLR:
            mes_no = 0;
            break;

        case TSC_LOAD_OBJ:
            obj[(int) arg[1]].load = (int) arg[0];
            obj[(int) arg[1]].load_step = TSLOAD_START;
            break;

        case TSC_SPRITE:
            sprite = (char) arg[0];
            break;

        case TSC_BOM:
            bom_no++;

            if (bom_no > 2) {
                bom_no = 0;
            }

            bom_pos[bom_no][0] = arg[0];
            bom_pos[bom_no][1] = arg[1];
            bom_pos[bom_no][2] = arg[2];
            bom_size[bom_no] = arg[3];
            bom_req = true;
            break;

        case TSC_BEEM:
            beem_no++;

            if (beem_no > 2) {
                beem_no = 0;
            }

            if (arg[0] == -1.0f) {
                beem_end = true;
            } else {
                beem_end = false;
                beem_from[beem_no][0] = arg[0];
                beem_from[beem_no][1] = arg[1];
                beem_from[beem_no][2] = arg[2];
                beem_to[beem_no][0] = arg[3];
                beem_to[beem_no][1] = arg[4];
                beem_to[beem_no][2] = arg[5];
            }

            beem_req = true;
            break;

        case TSC_SCENE_LOAD:
            load_no = (int) arg[0];
            break;

        case TSC_SCENE:
            scene = (int) arg[0];
            init_no = (int) arg[0];
            break;

        case TSC_WAIT:
            wait = arg[0] - 1.0f;
            break;

        case TSC_WAIT_KEY:
            mes_wait = (int) (1.0f + arg[0]);
            break;

        case TSC_END:
            end = 1;
            break;
    }

    return cursor;
}

/* One command's arguments. Both kinds read the same three forms and differ only in what stands
   before them: a kind-1 argument must be preceded by a comma and a kind-2 one stands where it is.
   Anything the three forms do not cover hands back -1, which the caller stores and then parses
   from ([title-script.md](../../docs/formats/title-script.md)). */
int CScript::CheckArg(char *buffer, int position, CSCRIPT_COMMAND *command) {
    int cursor;
    int i;
    int digit_count;
    int accepted;

    cursor = position;

    for (i = 0; i < command->arg_count; i++) {
        switch (command->arg_type[i]) {
            case TSARG_COMMA_VALUE:
                if (buffer[cursor] != ',') {
                    return -1;
                }

                cursor = SkipSpace(buffer, cursor + 1);

                if (memcmp(&buffer[cursor], "ON", 2) == 0) {
                    arg[i] = 1.0f;
                    cursor += 2;
                } else if (memcmp(&buffer[cursor], "OFF", 3) == 0) {
                    arg[i] = 0;
                    cursor += 3;
                } else {
                    accepted = 0;

                    if (buffer[cursor] == '-') {
                        accepted = 1;
                    }

                    if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                        accepted = 1;
                    }

                    if (!accepted) {
                        return -1;
                    }

                    arg[i] = (float) atof(&buffer[cursor]);

                    for (digit_count = 0; digit_count < 32; digit_count++) {
                        accepted = 0;

                        if (buffer[cursor] == '-') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] == '.') {
                            cursor++;
                            accepted = 1;
                        }

                        if (!accepted) {
                            break;
                        }
                    }

                    if (digit_count == 32) {
                        return -1;
                    }
                }

                cursor = SkipSpace(buffer, cursor);
                break;

            case TSARG_VALUE:
                if (memcmp(&buffer[cursor], "ON", 2) == 0) {
                    arg[i] = 1.0f;
                    cursor += 2;
                } else if (memcmp(&buffer[cursor], "OFF", 3) == 0) {
                    arg[i] = 0;
                    cursor += 3;
                } else {
                    accepted = 0;

                    if (buffer[cursor] == '-') {
                        accepted = 1;
                    }

                    if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                        accepted = 1;
                    }

                    if (!accepted) {
                        return -1;
                    }

                    arg[i] = (float) atof(&buffer[cursor]);

                    for (digit_count = 0; digit_count < 32; digit_count++) {
                        accepted = 0;

                        if (buffer[cursor] == '-') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] == '.') {
                            cursor++;
                            accepted = 1;
                        }

                        if (!accepted) {
                            break;
                        }
                    }

                    if (digit_count == 32) {
                        return -1;
                    }
                }

                cursor = SkipSpace(buffer, cursor);
                break;
        }
    }

    return cursor;
}

/* The run of separators standing before a token. Two of the five consume more than the byte they
   are found cursor - a comment runs to its line ending inclusive and the ideographic space is two bytes -
   which is why the skip is a loop over the whole file rather than a walk over one kind of byte
   ([title-script.md](../../docs/formats/title-script.md)). */
int CScript::SkipSpace(char *buffer, int position) {
    bool stop;

    while (position < size) {
        stop = true;

        if (memcmp(&buffer[position], "\x81\x40", 2) == 0) {
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

        if (memcmp(&buffer[position], "//", 2) == 0) {
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
