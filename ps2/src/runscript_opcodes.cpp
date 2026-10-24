#include "runscript_opcodes.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "btactstatus.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dun/gameloop.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "frame.hpp"
#include "mathutil.hpp"
#include "monstorunit.hpp"
#include "runscript.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "userstatus.hpp"

/**
 * Describes one opcode a monster script can call and the number that calls it.
 */
struct BT_EVENT_EXTERNAL_FUNCTION {
    int (*function)(RS_STACKDATA *, int); /**< Native function the opcode runs. */
    int operation;                        /**< Number the script calls the opcode by. */
};

int PUSH_INT_DATA[16][8];
int GL_INT[10];
int (*ext_func[256])(RS_STACKDATA *, int);

/**
 * Body collision sphere the last _SET_BODY_COL created, or -1 when it found no frame.
 */
static int bak_ColNo;

/**
 * Reads one script argument as an integer, converting it where the slot holds a float.
 */
static int GetStackInt(RS_STACKDATA *argument) {
    if (argument->type == RS_FLOAT) {
        return (int) argument->f;
    }

    return argument->i;
}

/**
 * Reads one script argument as a float, converting it where the slot holds an integer.
 */
static float GetStackFloat(RS_STACKDATA *argument) {
    if (argument->type == RS_INT) {
        return (float) argument->i;
    }

    return argument->f;
}

/**
 * Reads one script argument as a string.
 */
static char *GetStackString(RS_STACKDATA *argument) {
    return argument->s;
}

/**
 * Writes an integer back through a script argument that names a variable.
 */
static void SetStack(RS_STACKDATA *argument, int value) {
    if (argument->type == RS_PTR) {
        argument->p->i = value;
    }
}

/**
 * Writes a float back through a script argument that names a variable.
 */
static void SetStack(RS_STACKDATA *argument, float value) {
    if (argument->type == RS_PTR) {
        argument->p->f = value;
    }
}

int _SET_MOTION(RS_STACKDATA *stack, int argc) {
    int motion_id;
    int monster_no = NowMonstorUnit->current_monster;
    int half_speed;

    motion_id = GetStackInt(stack++);

    NowMonstorUnit->monster[monster_no].last_hit_damage = -1;
    NowMonstorUnit->monster[monster_no].requested_motion_speed = -1.0f;
    half_speed = 0;

    if (NowMonstorUnit->monster[monster_no].slow_timer > 0) {
        half_speed = 1;
    }

    if (argc == 1) {
        float speed = NowMonstorUnit->chara[monster_no][0].motion_type.motion_info[motion_id].speed;

        if (half_speed) {
            speed *= 0.5f;
        }

        NowMonstorUnit->chara[monster_no][0].SetMotion(motion_id, 0);
        NowMonstorUnit->chara[monster_no][0].SetMotionSpeed(speed);
        NowMonstorUnit->monster[monster_no].requested_motion = motion_id;
        NowMonstorUnit->monster[monster_no].requested_motion_flags = 0;
        NowMonstorUnit->monster[monster_no].requested_motion_speed = speed;

        for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
            NowMonstorUnit->chara[monster_no][i + 1].SetMotion(motion_id, 0);
            NowMonstorUnit->chara[monster_no][i + 1].SetMotionSpeed(speed);
        }
    }

    if (argc == 2) {
        float speed = GetStackFloat(stack++);

        if (half_speed) {
            speed *= 0.5f;
        }

        NowMonstorUnit->chara[monster_no][0].SetMotion(motion_id, 0);
        NowMonstorUnit->chara[monster_no][0].SetMotionSpeed(speed);
        NowMonstorUnit->monster[monster_no].requested_motion = motion_id;
        NowMonstorUnit->monster[monster_no].requested_motion_flags = 0;
        NowMonstorUnit->monster[monster_no].requested_motion_speed = speed;

        for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
            NowMonstorUnit->chara[monster_no][i + 1].SetMotion(motion_id, 0);
            NowMonstorUnit->chara[monster_no][i + 1].SetMotionSpeed(speed);
        }
    }

    if (argc == 3) {
        float speed = GetStackFloat(stack++);

        if (half_speed) {
            speed *= 0.5f;
        }

        int mode = GetStackInt(stack++);
        NowMonstorUnit->chara[monster_no][0].SetMotion(motion_id, mode);
        NowMonstorUnit->chara[monster_no][0].SetMotionSpeed(speed);
        NowMonstorUnit->monster[monster_no].requested_motion = motion_id;
        NowMonstorUnit->monster[monster_no].requested_motion_flags = mode;
        NowMonstorUnit->monster[monster_no].requested_motion_speed = speed;

        for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
            NowMonstorUnit->chara[monster_no][i + 1].SetMotion(motion_id, mode);
            NowMonstorUnit->chara[monster_no][i + 1].SetMotionSpeed(speed);
        }
    }

    return 1;
}

int _CHK_MOTION_FRM(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float current_frame = NowMonstorUnit->chara[monster_no][0].motion_type.state.time;
    int   motion_no = NowMonstorUnit->chara[monster_no][0].motion_no;
    float end_frame = (float) NowMonstorUnit->chara[monster_no][0].motion_type.motion_info[motion_no].end;
    int   done = 0;

    if (!(current_frame < end_frame - 1.0f) && current_frame < end_frame) {
        done = 1;
    }

    SetStack(stack++, done);

    if (argc == 2) {
        SetStack(stack, current_frame);
    }

    return 1;
}

int _GET_MOTION_FRM(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    SetStack(stack, NowMonstorUnit->chara[monster_no][0].motion_type.state.time);
    return 1;
}

int _SET_MOTION_FRM(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->chara[monster_no][0].motion_type.state.time = GetStackFloat(stack);
    return 1;
}

int _GET_DISTANCE(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float monster_position[4];
    float target_position[4];

    NowMonstorUnit->chara[monster_no][0].GetPosition(monster_position);

    if (argc == 1) {
        sceVu0CopyVector(target_position, CharaMain.pos);
    } else {
        target_position[0] = GetStackFloat(stack++);
        target_position[1] = GetStackFloat(stack++);
        target_position[2] = GetStackFloat(stack++);
    }

    SetStack(stack, DistVector(monster_position, target_position));
    return 1;
}

int _GET_POSITION(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    int   target = (int) GetStackFloat(stack++);
    float position[4];

    if (target == -1) {
        NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    }

    if (target == -2) {
        sceVu0CopyVector(position, CharaMain.pos);
    }

    SetStack(stack++, position[0]);
    SetStack(stack++, position[1]);
    SetStack(stack, position[2]);
    return 1;
}

int _SET_ROTATION(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].turn_target[0] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_target[1] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_target[2] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_speed = GetStackFloat(stack);

    if (NowMonstorUnit->monster[monster_no].turn_speed < 0.0f) {
        float position[4];
        float rotation[4];
        float direction[4];

        NowMonstorUnit->chara[monster_no][0].GetPosition(position);
        NowMonstorUnit->chara[monster_no][0].GetRotation(rotation);
        sceVu0SubVector(direction, NowMonstorUnit->monster[monster_no].turn_target, position);
        rotation[1] = atan2f(direction[0], direction[2]);
        NowMonstorUnit->chara[monster_no][0].SetRotation(rotation);
        NowMonstorUnit->monster[monster_no].turn_speed = 0.0f;
    }

    return 1;
}

int _CHK_ROTATION(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;
    int done = 0;

    if (NowMonstorUnit->monster[monster_no].turn_speed == 0.0f) {
        done = 1;
    }

    SetStack(stack, done);
    return 1;
}

int _CHK_MOVE(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;
    int done = 0;

    if (NowMonstorUnit->monster[monster_no].movement_speed == 0.0f) {
        done = 1;
    }

    SetStack(stack++, done);

    if (argc == 2) {
        float position[4];

        NowMonstorUnit->chara[monster_no][0].GetPosition(position);
        SetStack(stack, DistVector(NowMonstorUnit->monster[monster_no].movement, position));
    }

    return 1;
}

int _CHK_USER_INNER_PRODUCT(RS_STACKDATA *stack, int argc) {
    int   in_view;
    int   monster_no = NowMonstorUnit->current_monster;
    float min_cosine;
    float max_distance;
    float position[4];
    float rotation[4];
    float player[4];
    float away_from_player[4];

    in_view = 0;
    min_cosine = 1.0f - 0.011111111f * GetStackInt(stack++);
    max_distance = 100000;

    if (argc == 3) {
        max_distance = GetStackFloat(stack++);
    }

    sceVu0CopyVector(player, CharaMain.pos);
    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    NowMonstorUnit->chara[monster_no][0].GetRotation(rotation);
    float direction[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    float identity[4][4];
    float facing_matrix[4][4];

    sceVu0UnitMatrix(identity);
    sceVu0RotMatrixY(facing_matrix, identity, rotation[1]);
    sceVu0ApplyMatrix(direction, facing_matrix, direction);
    sceVu0Normalize(direction, direction);
    away_from_player[0] = position[0] - player[0];
    away_from_player[2] = position[2] - player[2];
    away_from_player[1] = 0.0f;
    away_from_player[3] = 1.0f;
    sceVu0Normalize(away_from_player, away_from_player);

    if (sceVu0InnerProduct(direction, away_from_player) >= min_cosine && DistVector(position, player) < max_distance) {
        in_view = 1;
    }

    SetStack(stack, in_view);
    PORT_RETURN(1);
}

int _GET_VECTOR(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float angle = 0.0f;
    float direction[4];
    float position[4];
    float turn_matrix[4][4];
    float identity[4][4];

    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    direction[0] = GetStackFloat(stack++);
    direction[1] = GetStackFloat(stack++);
    direction[2] = GetStackFloat(stack++);
    direction[3] = 1.0f;

    if (argc == 7) {
        angle = GetStackFloat(stack++);

        if (angle >= 180.0f) {
            angle -= 360.0f;
        }

        angle = DEG_TO_RAD * angle;

        if (angle > TWO_PI) {
            angle -= TWO_PI;
        }

        if (angle < -PI) {
            angle += TWO_PI;
        }
    }

    direction[0] -= position[0];
    direction[1] -= position[1];
    direction[2] -= position[2];
    sceVu0Normalize(direction, direction);

    if (argc == 7) {
        sceVu0UnitMatrix(identity);
        sceVu0RotMatrixY(turn_matrix, identity, angle);
        sceVu0ApplyMatrix(direction, turn_matrix, direction);
    }

    SetStack(stack++, direction[0]);
    SetStack(stack++, direction[1]);
    SetStack(stack, direction[2]);
    return 1;
}

int _GET_DIRECTION(RS_STACKDATA *stack, int argc) {
    float rotation[4];
    float facing_matrix[4][4];
    float identity[4][4];

    NowMonstorUnit->chara[NowMonstorUnit->current_monster][0].GetRotation(rotation);

    if (argc == 4) {
        float angle = GetStackFloat(stack++);

        rotation[1] += DEG_TO_RAD * angle;

        if (rotation[1] > PI) {
            rotation[1] -= TWO_PI;
        }

        if (rotation[1] < -PI) {
            rotation[1] += TWO_PI;
        }
    }

    float direction[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    sceVu0UnitMatrix(identity);
    sceVu0RotMatrixY(facing_matrix, identity, rotation[1]);
    sceVu0ApplyMatrix(direction, facing_matrix, direction);
    direction[3] = 1.0f;
    sceVu0Normalize(direction, direction);
    SetStack(stack++, direction[0]);
    SetStack(stack++, direction[1]);
    SetStack(stack, direction[2]);
    return 1;
}

int _SET_MOVE(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float direction[4];
    float position[4];
    float speed;

    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    direction[0] = GetStackFloat(stack++);
    direction[1] = GetStackFloat(stack++);
    direction[2] = GetStackFloat(stack++);
    direction[0] -= position[0];
    direction[1] -= position[1];
    direction[2] -= position[2];
    direction[3] = 1.0f;
    sceVu0Normalize(NowMonstorUnit->monster[monster_no].movement, direction);
    speed = GetStackFloat(stack);

    if (NowMonstorUnit->monster[monster_no].slow_timer > 0) {
        speed *= 0.5f;
    }

    NowMonstorUnit->monster[monster_no].movement_speed = speed;
    return 1;
}

int _CHK_MOVE_INFO(RS_STACKDATA *stack, int argc) {
    int   clear;
    int   monster_no = NowMonstorUnit->current_monster;
    float from[4];
    float to[4];
    float hit[4];

    clear = 1;
    NowMonstorUnit->chara[monster_no][0].GetPosition(from);
    to[0] = GetStackFloat(stack++);
    to[1] = GetStackFloat(stack++);
    to[2] = GetStackFloat(stack++);
    from[1] += 5.0f;
    to[1] += 5.0f;

    if (CheckHit(NowMonstorUnit->monster[monster_no].collision_poly, NowMonstorUnit->monster[monster_no].collision_poly_count, from, to, hit, 0, 0) >= 0) {
        clear = 0;
    }

    SetStack(stack, clear);
    return 1;
}

int _SET_MOVE_CANSEL(RS_STACKDATA *stack, int argc) {
    NowMonstorUnit->monster[NowMonstorUnit->current_monster].movement_speed = 0.0f;
    return 1;
}

int _SET_ROT_CANSEL(RS_STACKDATA *stack, int argc) {
    NowMonstorUnit->monster[NowMonstorUnit->current_monster].turn_speed = 0.0f;
    return 1;
}

int _SET_POSITION(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float x = GetStackFloat(stack++);
    float y = GetStackFloat(stack++);
    float z = GetStackFloat(stack);

    NowMonstorUnit->chara[monster_no][0].SetPosition(x, y, z);
    return 1;
}

int _STATUS_SET_FALL(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].falls = GetStackInt(stack);
    return 1;
}

int _STATUS_SET_MUTEKI(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;
    int invincible = GetStackInt(stack);

    if (NowMonstorUnit->monster[monster_no].hp > 0 && NowMonstorUnit->monster[monster_no].invincible_blocked != 0) {
        invincible = 0;
    }

    NowMonstorUnit->monster[monster_no].invincible_timer = invincible;
    return 1;
}

int _STATUS_SET_ALPHA(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].palette_alpha_step = GetStackFloat(stack++);

    if (argc == 2) {
        NowMonstorUnit->monster[monster_no].palette_alpha_step = GetStackInt(stack);
    } else {
        NowMonstorUnit->monster[monster_no].palette_delay = 0;
    }

    return 1;
}

int _STATUS_CHK_ALPHA(RS_STACKDATA *stack, int argc) {
    int done;
    int monster_no = NowMonstorUnit->current_monster;
    int fading_in;

    done = 0;
    fading_in = 0;

    if (argc == 2) {
        fading_in = GetStackInt(stack++);
    }

    if (fading_in == 0) {
        if (NowMonstorUnit->monster[monster_no].palette_alpha <= 0.0f) {
            done = 1;
        }
    } else if (NowMonstorUnit->monster[monster_no].palette_alpha >= 128.0f) {
        done = 1;
    }

    SetStack(stack, done);
    return 1;
}

int _STATUS_SET_DEAD(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].state = -1;
    NowMonstorUnit->alive_count--;
    return 1;
}

int _STATUS_SET_EVENT(RS_STACKDATA *stack, int argc) {
    NowMonstorUnit->requested_event = GetStackInt(stack);
    return 1;
}

int _RUN_SCRIPT(RS_STACKDATA *stack, int argc) {
    NowMonstorUnit->requested_event = GetStackInt(stack);
    printf("run script !!\n");
    return 1;
}

int _STATUS_SET_COL_OFF(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].collision_off_timer = GetStackInt(stack);
    return 1;
}

int _STATUS_GET_LIFE_RATE(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float max_hp = NowMonstorUnit->monster[monster_no].max_hp;
    float hp = NowMonstorUnit->monster[monster_no].hp;

    SetStack(stack, 100.0f * (hp / max_hp));
    return 1;
}

int _STATUS_GET_USER_VECTOR(RS_STACKDATA *stack, int argc) {
    int   normalize = 0;
    float angle;
    float vector[4];

    if (argc == 4) {
        normalize = GetStackInt(stack++);
    }

    angle = 0.0f;
    angle += DEG_TO_RAD * GetStackFloat(stack++);

    if (angle > PI) {
        angle -= TWO_PI;
    }

    if (angle < -PI) {
        angle += TWO_PI;
    }

    getCharacterVector(vector, angle);

    if (normalize != 0) {
        sceVu0Normalize(vector, vector);
    }

    SetStack(stack++, vector[0]);
    SetStack(stack++, vector[1]);
    SetStack(stack, vector[2]);
    return 1;
}

int _STATUS_GET_HEIGHT(RS_STACKDATA *stack, int argc) {
    SetStack(stack, NowMonstorUnit->monster[NowMonstorUnit->current_monster].ground_distance);
    return 1;
}

int _GET_RAND(RS_STACKDATA *stack, int argc) {
    int bound = GetStackInt(stack++);

    int roll = (float) bound * rand() / 2147483648.0f;

    SetStack(stack, roll);
    return 1;
}

int _GET_RANDF(RS_STACKDATA *stack, int argc) {
    float bound = GetStackFloat(stack++);

    SetStack(stack, (float) (int) (bound * rand() / 2147483648.0f));
    return 1;
}

int _SIN_DEG(RS_STACKDATA *stack, int argc) {
    float angle = GetStackFloat(stack++);

    angle = DEG_TO_RAD * angle;
    SetStack(stack, sinf(angle));
    return 1;
}

int _COS_DEG(RS_STACKDATA *stack, int argc) {
    float angle = GetStackFloat(stack++);

    angle = DEG_TO_RAD * angle;
    SetStack(stack, cosf(angle));
    return 1;
}

// clang-format off
int elmColor[6][3] = {
    {230, 90, 0}, {0, 200, 255}, {255, 255, 0}, {0, 255, 0}, {255, 200, 255}, {255, 0, 0},
};
// clang-format on

int _STATUS_SET_PALLET(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    if (argc == 5) {
        NowMonstorUnit->monster[monster_no].palette_target[0] = GetStackFloat(stack++);
        NowMonstorUnit->monster[monster_no].palette_target[1] = GetStackFloat(stack++);
        NowMonstorUnit->monster[monster_no].palette_target[2] = GetStackFloat(stack++);
    } else {
        int element = NowMonstorUnit->monster[monster_no].hit_element;

        NowMonstorUnit->monster[monster_no].palette_target[0] = elmColor[element][0];
        NowMonstorUnit->monster[monster_no].palette_target[1] = elmColor[element][1];
        NowMonstorUnit->monster[monster_no].palette_target[2] = elmColor[element][2];
    }

    NowMonstorUnit->monster[monster_no].palette_cycles = GetStackInt(stack++);
    NowMonstorUnit->monster[monster_no].palette_step = GetStackFloat(stack);
    NowMonstorUnit->monster[monster_no].palette_blend = 0.0f;
    return 1;
}

int _STATUS_SET_CLIPLEVEL(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].clip_distance = GetStackFloat(stack);
    return 1;
}

int _STATUS_GET_HITDMG_VOL(RS_STACKDATA *stack, int argc) {
    SetStack(stack, NowMonstorUnit->monster[NowMonstorUnit->current_monster].last_hit_damage);
    return 1;
}

int _STATUS_GET_MOTION_ID(RS_STACKDATA *stack, int argc) {
    SetStack(stack, NowMonstorUnit->chara[NowMonstorUnit->current_monster][0].motion_no);
    return 1;
}

int _STATUS_GET_DMG_ID(RS_STACKDATA *stack, int argc) {
    SetStack(stack, NowMonstorUnit->monster[NowMonstorUnit->current_monster].last_hit_id);
    return 1;
}

int _STATUS_SET_LOCKON_DIST(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].lock_range = GetStackFloat(stack);
    return 1;
}

int _STATUS_SET_SHADOW_LEN(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].shadow_length = GetStackFloat(stack);
    return 1;
}

int _STATUS_SET_LOCKON_TRG(RS_STACKDATA *stack, int argc) {
    char   *name;
    int     monster_no = NowMonstorUnit->current_monster;
    float   scale_x = 1.0f;
    float   scale_y = scale_x;
    CFrame *frame;

    name = GetStackString(stack++);

    if (argc == 3) {
        scale_x = GetStackFloat(stack++);
        scale_y = GetStackFloat(stack);
    }

    frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(name);

    if (frame == NULL) {
        printf("lock:NofFountNull %s\n", name);
    } else {
        NowMonstorUnit->monster[monster_no].lockon_frame = frame;
        NowMonstorUnit->monster[monster_no].lockon_scale_x = scale_x;
        NowMonstorUnit->monster[monster_no].lockon_scale_y = scale_y;
    }

    return 1;
}

int _SET_MOV_COL(RS_STACKDATA *stack, int argc) {
    char *name = GetStackString(stack++);
    float radius = GetStackFloat(stack);
    int   i;
    int   monster_no = NowMonstorUnit->current_monster;

    for (i = 0; i < 12; i++) {
        if (NowMonstorUnit->effect3[monster_no].timer[i] == 0) {
            CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(name);

            if (frame != NULL) {
                NowMonstorUnit->effect3[monster_no].timer[i] = 1;
                NowMonstorUnit->effect3[monster_no].frame[i] = frame;
                NowMonstorUnit->effect3[monster_no].radius[i] = radius;
                NowMonstorUnit->effect3[monster_no].count++;
                break;
            }

            printf("[%d]mov col -> %s\n", NowMonstorUnit->monster[monster_no].base_model, name);
        }
    }

    return 1;
}

int _SET_BODY_COL(RS_STACKDATA *stack, int argc) {
    char *name = GetStackString(stack++);
    float radius = GetStackFloat(stack++);
    float start = 0.0f;
    float end = 0.0f;
    int   i;
    int   monster_no;
    int   j;

    if (argc == 4) {
        start = GetStackFloat(stack++);
        end = GetStackFloat(stack);
    }

    monster_no = NowMonstorUnit->current_monster;

    for (i = 0; i < 16; i++) {
        if (NowMonstorUnit->effect[monster_no].timer[i] == 0) {
            CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(name);

            if (frame != NULL) {
                NowMonstorUnit->effect[monster_no].timer[i] = 1;
                NowMonstorUnit->effect[monster_no].frame[i] = frame;
                NowMonstorUnit->effect[monster_no].radius[i] = radius;
                NowMonstorUnit->effect[monster_no].motion_start[i] = start;
                NowMonstorUnit->effect[monster_no].motion_end[i] = end;

                for (j = 0; j < 5; j++) {
                    NowMonstorUnit->effect[monster_no].body_parameter[i][j] = 100;
                }

                for (j = 0; j < 6; j++) {
                    NowMonstorUnit->effect[monster_no].parameter[i][j] = 100;
                }

                bak_ColNo = i;
                break;
            }

            printf("[%d]body col -> %s\n", NowMonstorUnit->monster[monster_no].base_model, name);
            bak_ColNo = -1;
        }
    }

    return 1;
}

int _SET_BODY_COL_PARA(RS_STACKDATA *stack, int argc) {
    int parameter_no = GetStackInt(stack++);
    int value = GetStackInt(stack);
    int monster_no = NowMonstorUnit->GetCurrentMonsterIndex();

    if (bak_ColNo == -1) {
        return 1;
    }

    if (parameter_no <= 9) {
        NowMonstorUnit->effect[monster_no].body_parameter[bak_ColNo][parameter_no] = value;
    }

    if (parameter_no >= 10) {
        NowMonstorUnit->effect[monster_no].parameter[bak_ColNo][parameter_no - 10] = value;
    }

    return 1;
}

int _SET_DMG_COL(RS_STACKDATA *stack, int argc) {
    char *name = GetStackString(stack++);
    float radius = GetStackFloat(stack++);
    float start = GetStackFloat(stack++);
    float end = GetStackFloat(stack);
    int   i;
    int   monster_no = NowMonstorUnit->current_monster;

    for (i = 0; i < 16; i++) {
        if (NowMonstorUnit->effect2[monster_no].active[i] == 0) {
            CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(name);

            if (frame != NULL) {
                NowMonstorUnit->effect2[monster_no].active[i] = 1;
                NowMonstorUnit->effect2[monster_no].frame[i] = frame;
                NowMonstorUnit->effect2[monster_no].radius[i] = radius;
                NowMonstorUnit->effect2[monster_no].motion_start[i] = start;
                NowMonstorUnit->effect2[monster_no].motion_end[i] = end;
                NowMonstorUnit->effect2[monster_no].last_slot = i;
                break;
            }

            printf("[%d] dcol -> %s\n", NowMonstorUnit->monster[monster_no].base_model, name);
            NowMonstorUnit->effect2[monster_no].last_slot = -1;
            return 1;
        }
    }

    return 1;
}

int _SET_DMG_PARA(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;
    int slot = NowMonstorUnit->effect2[monster_no].last_slot;

    if (slot != -1) {
        NowMonstorUnit->effect2[monster_no].damage[slot] = GetStackInt(stack++);
        NowMonstorUnit->effect2[monster_no].flags[slot] = GetStackInt(stack++);
        NowMonstorUnit->effect2[monster_no].kind[slot] = GetStackInt(stack++);
        NowMonstorUnit->effect2[monster_no].angle[slot] = 0.0f;

        if (NowMonstorUnit->effect2[monster_no].kind[slot] == 3 && argc == 4) {
            NowMonstorUnit->effect2[monster_no].angle[slot] = GetStackFloat(stack);
        }
    }

    return 1;
}

int _SET_SHOT(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    char *name;

    if (NowMonstorUnit->monster[monster_no].shot_effect == -1) {
        return 1;
    }

    name = GetStackString(stack++);
    NowMonstorUnit->event[monster_no].local_position[0] = GetStackFloat(stack++);
    NowMonstorUnit->event[monster_no].local_position[1] = GetStackFloat(stack++);
    NowMonstorUnit->event[monster_no].local_position[2] = GetStackFloat(stack++);
    NowMonstorUnit->event[monster_no].local_position[3] = 1.0f;

    if (NowMonstorUnit->event[monster_no].timer == 0) {
        CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(name);

        if (frame == NULL) {
            printf("not shot null !!\n");
            return 1;
        }

        NowMonstorUnit->event[monster_no].frame = frame;
        NowMonstorUnit->event[monster_no].timer = 1;
    }

    NowMonstorUnit->event[monster_no].damage_override = -1;

    if (argc == 5) {
        NowMonstorUnit->event[monster_no].damage_override = GetStackInt(stack);
    }

    return 1;
}

int _SET_SHOT2(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    char *name;

    if (NowMonstorUnit->monster[monster_no].shot_effect2 == -1) {
        return 1;
    }

    printf("shot !!\n");
    name = GetStackString(stack++);
    NowMonstorUnit->event2[monster_no].local_position[0] = GetStackFloat(stack++);
    NowMonstorUnit->event2[monster_no].local_position[1] = GetStackFloat(stack++);
    NowMonstorUnit->event2[monster_no].local_position[2] = GetStackFloat(stack++);
    NowMonstorUnit->event2[monster_no].local_position[3] = 1.0f;

    if (NowMonstorUnit->event2[monster_no].timer == 0) {
        CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(name);

        if (frame == NULL) {
            printf("not shot null !!\n");
            return 1;
        }

        NowMonstorUnit->event2[monster_no].frame = frame;
        NowMonstorUnit->event2[monster_no].timer = 1;
    }

    NowMonstorUnit->event2[monster_no].damage_override = -1;

    if (argc == 5) {
        NowMonstorUnit->event2[monster_no].damage_override = GetStackInt(stack);
    }

    return 1;
}

int _SET_SND_FRM(RS_STACKDATA *stack, int argc) {
    int slot;
    int i;
    int monster_no = NowMonstorUnit->current_monster;

    slot = -1;

    for (i = 0; i < 16; i++) {
        if (NowMonstorUnit->sound[monster_no].id[i] == -1) {
            if (NowMonstorUnit->sound[monster_no].cooldown[i] == 0) {
                slot = i;
            }

            break;
        }
    }

    if (slot == -1) {
        return 1;
    }

    NowMonstorUnit->sound[monster_no].start[slot] = GetStackFloat(stack++);
    NowMonstorUnit->sound[monster_no].end[slot] = GetStackFloat(stack++);
    NowMonstorUnit->sound[monster_no].id[slot] = GetStackInt(stack);
    return 1;
}

int _SET_LOOP_SND(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->sound[monster_no].sequence_start = GetStackFloat(stack++);
    NowMonstorUnit->sound[monster_no].sequence_end = GetStackFloat(stack++);
    NowMonstorUnit->sound[monster_no].sequence_step = GetStackInt(stack++);
    NowMonstorUnit->sound[monster_no].sequence_id = GetStackInt(stack);
    return 1;
}

int _STOP_LOOP_SND(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;
    int sound_id = NowMonstorUnit->sound[monster_no].sequence_id;

    if (sound_id == -1) {
        return 1;
    }

    NowMonstorUnit->sound[monster_no].sequence_id = -1;
    SndSeStop(sound_id, monster_no * 2);
    return 1;
}

int _DEL_LOOP_SND(RS_STACKDATA *stack, int argc) {
    // The sound keeps playing; the monster just stops owning it.
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->sound[monster_no].sequence_id = -1;
    return 1;
}

int _SET_SND_NOW(RS_STACKDATA *stack, int argc) {
    SndSePlay(GetStackInt(stack), -1, 0);
    return 1;
}

int _STOP_SND_NOW(RS_STACKDATA *stack, int argc) {
    SndSeStop(GetStackInt(stack), 0);
    return 1;
}

int _GET_CHR_ID(RS_STACKDATA *stack, int argc) {
    SetStack(stack, (int) UserStatus->cur_chara);
    return 1;
}

int _GET_COL_HIT_ID(RS_STACKDATA *stack, int argc) {
    SetStack(stack, NowMonstorUnit->effect[NowMonstorUnit->current_monster].hit_slot);
    return 1;
}

int _GET_SCRIPT_ID(RS_STACKDATA *stack, int argc) {
    SetStack(stack, NowMonstorUnit->current_monster);
    return 1;
}

int _GET_MONSTOR_POS(RS_STACKDATA *stack, int argc) {
    float position[4];

    NowMonstorUnit->chara[GetStackInt(stack++)][0].GetPosition(position);
    SetStack(stack++, position[0]);
    SetStack(stack++, position[1]);
    SetStack(stack, position[2]);
    return 1;
}

int _GET_MONSTOR_FRM(RS_STACKDATA *stack, int argc) {
    int monster_no = GetStackInt(stack++);

    float frame = NowMonstorUnit->chara[monster_no][0].motion_type.state.time;

    SetStack(stack, frame);
    return 1;
}

int _SET_MONSTOR_POS(RS_STACKDATA *stack, int argc) {
    int   monster_no = GetStackInt(stack++);
    float position[4];

    position[0] = GetStackFloat(stack++);
    position[1] = GetStackFloat(stack++);
    position[2] = GetStackFloat(stack);
    NowMonstorUnit->chara[monster_no][0].SetPosition(position);
    return 1;
}

int _SET_MONSTOR_MOVE(RS_STACKDATA *stack, int argc) {
    int   monster_no = GetStackInt(stack++);
    float direction[4];
    float position[4];

    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    direction[0] = GetStackFloat(stack++);
    direction[1] = GetStackFloat(stack++);
    direction[2] = GetStackFloat(stack++);
    direction[0] -= position[0];
    direction[1] -= position[1];
    direction[2] -= position[2];
    direction[3] = 1.0f;
    sceVu0Normalize(NowMonstorUnit->monster[monster_no].movement, direction);
    NowMonstorUnit->monster[monster_no].movement_speed = GetStackFloat(stack);
    return 1;
}

int _SET_MONSTOR_LINK_MOVE(RS_STACKDATA *stack, int argc) {
    int monster_no = GetStackInt(stack++);
    int leader_no = GetStackInt(stack);

    sceVu0CopyVector(NowMonstorUnit->monster[monster_no].movement, NowMonstorUnit->monster[leader_no].movement);
    NowMonstorUnit->monster[monster_no].movement_speed = NowMonstorUnit->monster[leader_no].movement_speed;
    return 1;
}

int _SET_MONSTOR_MOVE_CANSEL(RS_STACKDATA *stack, int argc) {
    int monster_no = GetStackInt(stack);

    NowMonstorUnit->monster[monster_no].movement_speed = 0.0f;
    return 1;
}

int _SET_LOCKON_DIST(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].lock_range = GetStackFloat(stack);
    return 1;
}

int _SET_LOCKON_SW(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].lockon_enabled = GetStackInt(stack);
    return 1;
}

int _SET_MONSTOR_MOTION(RS_STACKDATA *stack, int argc) {
    int monster_no = GetStackInt(stack++);
    int motion_id = GetStackInt(stack++);

    NowMonstorUnit->monster[monster_no].last_hit_damage = -1;
    NowMonstorUnit->monster[monster_no].requested_motion_speed = -1.0f;

    if (argc == 2) {
        NowMonstorUnit->chara[monster_no][0].SetMotion(motion_id, 0);
        NowMonstorUnit->monster[monster_no].requested_motion = motion_id;
        NowMonstorUnit->monster[monster_no].requested_motion_flags = 0;

        for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
            NowMonstorUnit->chara[monster_no][i + 1].SetMotion(motion_id, 0);
        }
    }

    if (argc == 3) {
        float speed = GetStackFloat(stack++);

        NowMonstorUnit->chara[monster_no][0].SetMotion(motion_id, 0);
        NowMonstorUnit->chara[monster_no][0].SetMotionSpeed(speed);
        NowMonstorUnit->monster[monster_no].requested_motion = motion_id;
        NowMonstorUnit->monster[monster_no].requested_motion_flags = 0;
        NowMonstorUnit->monster[monster_no].requested_motion_speed = speed;

        for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
            NowMonstorUnit->chara[monster_no][i + 1].SetMotion(motion_id, 0);
            NowMonstorUnit->chara[monster_no][i + 1].SetMotionSpeed(speed);
        }
    }

    if (argc == 4) {
        float speed = GetStackFloat(stack++);
        int   mode = GetStackInt(stack);

        NowMonstorUnit->chara[monster_no][0].SetMotion(motion_id, mode);
        NowMonstorUnit->chara[monster_no][0].SetMotionSpeed(speed);
        NowMonstorUnit->monster[monster_no].requested_motion = motion_id;
        NowMonstorUnit->monster[monster_no].requested_motion_flags = mode;
        NowMonstorUnit->monster[monster_no].requested_motion_speed = speed;

        for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
            NowMonstorUnit->chara[monster_no][i + 1].SetMotion(motion_id, mode);
            NowMonstorUnit->chara[monster_no][i + 1].SetMotionSpeed(speed);
        }
    }

    return 1;
}

int _SET_GLOBAL_INT(RS_STACKDATA *stack, int argc) {
    int slot = GetStackInt(stack++);

    GL_INT[slot] = GetStackInt(stack);
    return 1;
}

int _GET_GLOBAL_INT(RS_STACKDATA *stack, int argc) {
    int slot = GetStackInt(stack++);

    SetStack(stack++, GL_INT[slot]);
    return 1;
}

/**
 * Reads the world position of a named frame of the monster's model.
 */
static int _GET_OBJ_POS(RS_STACKDATA *stack, int argc) {
    int     monster_no = NowMonstorUnit->current_monster;
    CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame->SearchFrame(GetStackString(stack++));
    float   local[4];
    float   world[4];

    sceVu0CopyVector(local, frame->position);
    frame->GetWorldPosition(world, local);
    SetStack(stack++, world[0]);
    SetStack(stack++, world[1]);
    SetStack(stack, world[2]);
    return 1;
}

int _SET_ROTATION_X(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float position[4];
    float rotation[4];
    float direction[4];

    NowMonstorUnit->monster[monster_no].turn_target[0] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_target[1] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_target[2] = GetStackFloat(stack);
    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    NowMonstorUnit->chara[monster_no][0].GetRotation(rotation);
    sceVu0SubVector(direction, NowMonstorUnit->monster[monster_no].turn_target, position);
    rotation[0] = -atan2f(direction[1], direction[2]);
    NowMonstorUnit->chara[monster_no][0].SetRotation(rotation);
    return 1;
}

int _LOOKAT(RS_STACKDATA *stack, int argc) {
    int           axis;
    int           monster_no = NowMonstorUnit->current_monster;
    float         position[4];
    float         rotation[4];
    sceVu0FMATRIX matrix;
    float         direction[4];
    float         x_axis[4];
    float         y_axis[4];
    float         z_axis[4];

    NowMonstorUnit->monster[monster_no].turn_target[0] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_target[1] = GetStackFloat(stack++);
    NowMonstorUnit->monster[monster_no].turn_target[2] = GetStackFloat(stack++);
    axis = GetStackInt(stack);
    sceVu0UnitMatrix(matrix);
    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    NowMonstorUnit->chara[monster_no][0].GetRotation(rotation);
    sceVu0CopyMatrix(matrix, NowMonstorUnit->chara[monster_no][0].frame->local);
    sceVu0SubVector(direction, NowMonstorUnit->monster[monster_no].turn_target, position);

    if (axis == 0) {
        sceVu0Normalize(x_axis, direction);
        sceVu0OuterProduct(z_axis, x_axis, matrix[1]);
        sceVu0OuterProduct(y_axis, x_axis, z_axis);
        sceVu0CopyVector(matrix[0], x_axis);
        sceVu0CopyVector(matrix[1], y_axis);
        sceVu0CopyVector(matrix[2], z_axis);
        NowMonstorUnit->chara[monster_no][0].frame->SetTransMatrix(matrix);
    }

    if (axis == 1) {
        sceVu0Normalize(y_axis, direction);
        sceVu0OuterProduct(x_axis, y_axis, matrix[2]);
        sceVu0OuterProduct(z_axis, x_axis, z_axis);
        sceVu0CopyVector(matrix[0], x_axis);
        sceVu0CopyVector(matrix[1], y_axis);
        sceVu0CopyVector(matrix[2], z_axis);
        NowMonstorUnit->chara[monster_no][0].frame->SetTransMatrix(matrix);
    }

    if (axis == 2) {
        sceVu0Normalize(z_axis, direction);
        sceVu0OuterProduct(x_axis, z_axis, matrix[1]);
        sceVu0OuterProduct(y_axis, x_axis, z_axis);
        sceVu0CopyVector(matrix[0], x_axis);
        sceVu0CopyVector(matrix[1], y_axis);
        sceVu0CopyVector(matrix[2], z_axis);
        NowMonstorUnit->chara[monster_no][0].frame->SetTransMatrix(matrix);
    }

    return 1;
}

int _SET_MOTION_CHANGE_STEP(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float step = GetStackFloat(stack);

    NowMonstorUnit->chara[monster_no][0].motion_type.state.blend_step = step;

    for (int i = 0; i < NowMonstorUnit->monster[monster_no].attachment_count; i++) {
        NowMonstorUnit->chara[monster_no][i + 1].motion_type.state.blend_step = step;
    }

    return 1;
}

int _GET_MONSTOR_VECTOR(RS_STACKDATA *stack, int argc) {
    int   monster_no = GetStackInt(stack++);
    float angle = 0.0f;
    float direction[4];
    float position[4];
    float turn_matrix[4][4];
    float identity[4][4];

    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    direction[0] = GetStackFloat(stack++);
    direction[1] = GetStackFloat(stack++);
    direction[2] = GetStackFloat(stack++);
    direction[3] = 1.0f;

    if (argc == 8) {
        angle = GetStackFloat(stack++);

        if (angle >= 180.0f) {
            angle -= 360.0f;
        }

        angle = DEG_TO_RAD * angle;

        if (angle > TWO_PI) {
            angle -= TWO_PI;
        }

        if (angle < -PI) {
            angle += TWO_PI;
        }
    }

    direction[0] -= position[0];
    direction[1] -= position[1];
    direction[2] -= position[2];
    sceVu0Normalize(direction, direction);

    if (argc == 8) {
        sceVu0UnitMatrix(identity);
        sceVu0RotMatrixY(turn_matrix, identity, angle);
        sceVu0ApplyMatrix(direction, turn_matrix, direction);
    }

    SetStack(stack++, direction[0]);
    SetStack(stack++, direction[1]);
    SetStack(stack, direction[2]);
    return 1;
}

int _STATUS_SET_LIFE(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float rate = GetStackFloat(stack);

    NowMonstorUnit->monster[monster_no].hp = (int) (NowMonstorUnit->monster[monster_no].max_hp * rate);
    return 1;
}

int _SET_BIN2(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].anger_timer = GetStackInt(stack);
    return 1;
}

int _SET_STATUS_CHANGE(RS_STACKDATA *stack, int argc) {
    int kind = GetStackInt(stack++);
    int weight = GetStackInt(stack++);
    int monster_no = GetStackInt(stack);

    if (monster_no < 0 || monster_no > 15) {
        return 1;
    }

    if (kind < 0 || kind > 4) {
        kind = 0;
    }

    NowMonstorUnit->monster[monster_no].attachment_weight[kind] = weight;
    return 1;
}

int _SET_TEX_ANIME_SW(RS_STACKDATA *stack, int argc) {
    int anime_no = GetStackInt(stack++);
    int enable = GetStackInt(stack++);
    int monster_no = GetStackInt(stack);

    if (monster_no < 0 || monster_no > 15) {
        return 2;
    }

    if (enable) {
        NowMonstorUnit->chara[monster_no][0].TexAnimeOn(anime_no);
        printf("animOn %d\n", anime_no);
    } else {
        NowMonstorUnit->chara[monster_no][0].TexAnimeOff(anime_no);
        printf("animOff %d\n", anime_no);
    }

    return 1;
}

int _GET_STATUS_BIN2(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;
    int flags = NowMonstorUnit->monster[monster_no].anger_timer;

    NowMonstorUnit->monster[monster_no].anger_timer = flags;
    SetStack(stack, flags);
    return 1;
}

int _SET_COLLISION_WIDTH(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].collision_radius = GetStackFloat(stack);
    return 1;
}

int _GET_NEAR_MONSTER(RS_STACKDATA *stack, int argc) {
    int   found;
    int   i;
    int   monster_no = NowMonstorUnit->current_monster;
    float nearest = 240.0f;
    float position[4];
    float other_position[4];
    float found_position[4];

    found = -1;
    NowMonstorUnit->chara[monster_no][0].GetPosition(position);

    for (i = 0; i < 16; i++) {
        if (i != monster_no && NowMonstorUnit->monster[i].state == 2) {
            NowMonstorUnit->chara[i][0].GetPosition(other_position);
            float distance = DistVector(position, other_position);

            if (distance < nearest) {
                sceVu0CopyVector(found_position, other_position);
                nearest = distance;
                found = i;
            }
        }
    }

    SetStack(stack++, found_position[0]);
    SetStack(stack++, found_position[1]);
    SetStack(stack++, found_position[2]);
    SetStack(stack, found);
    PORT_RETURN(1);
}

int _BOSS_FADE_OUT(RS_STACKDATA *stack, int argc) {
    EdFadeInit();
    EdFadeOut(120, 0.0f, 0.0f, 0.0f);
    BtActStatus.can_act = 0;
    BtActStatus.movement_locked = 1;
    return 1;
}

int _CHEKC_FADE_OUT(RS_STACKDATA *stack, int argc) {
    int done = EdFadeOutCheck();

    SetStack(stack, done);
    return 1;
}

int _SET_GRAVITY(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].free_fall = GetStackInt(stack);
    return 1;
}

int _SET_GUARD_FRAME(RS_STACKDATA *stack, int argc) {
    int start;
    int monster_no = NowMonstorUnit->current_monster;
    int end;
    int slot;

    start = (int) GetStackFloat(stack++);
    end = (int) GetStackFloat(stack);
    slot = -1;

    for (int i = 0; i < 3; i++) {
        if (NowMonstorUnit->guard[monster_no].active[i] == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return 1;
    }

    NowMonstorUnit->guard[monster_no].active[slot] = 1;
    NowMonstorUnit->guard[monster_no].motion_start[slot] = start;
    NowMonstorUnit->guard[monster_no].motion_end[slot] = end;
    return 1;
}

int _GUARD_SEARCH(RS_STACKDATA *stack, int argc) {
    int   monster_no = NowMonstorUnit->current_monster;
    float player[4];
    float position[4];
    float direction[4];
    float distance;

    sceVu0CopyVector(player, CharaMain.pos);
    NowMonstorUnit->chara[monster_no][0].GetPosition(position);
    distance = DistVector(player, position);
    direction[0] = position[0] - player[0];
    direction[1] = 0.0f;
    direction[2] = position[2] - player[2];
    direction[3] = 1.0f;
    sceVu0Normalize(direction, direction);

    if (sceVu0InnerProduct(BtActStatus.input_direction, direction) >= 0.35) {
        BtActStatus.frames_since_attack = 3600;
    }

    SetStack(stack++, BtActStatus.frames_since_attack);
    SetStack(stack++, distance);
    SetStack(stack, UserStatus->cur_chara);
    return 1;
}

int _GET_MOVE_VEC(RS_STACKDATA *stack, int argc) {
    SetStack(stack++, BtActStatus.script_move_vector[0]);
    SetStack(stack++, BtActStatus.script_move_vector[1]);
    SetStack(stack, BtActStatus.script_move_vector[2]);
    return 1;
}

int _PUSH_IGLOBAL(RS_STACKDATA *stack, int argc) {
    int slot;
    int monster_no = NowMonstorUnit->current_monster;

    slot = GetStackInt(stack++);

    if (slot < 0 || slot > 7) {
        return 0;
    }

    PUSH_INT_DATA[monster_no][slot] = GetStackInt(stack);
    return 1;
}

int _POP_IGLOBAL(RS_STACKDATA *stack, int argc) {
    int slot;
    int monster_no = NowMonstorUnit->current_monster;

    slot = GetStackInt(stack++);

    if (slot < 0 || slot > 7) {
        return 0;
    }

    SetStack(stack++, PUSH_INT_DATA[monster_no][slot]);
    return 1;
}

int _GET_USER_STATUS(RS_STACKDATA *stack, int argc) {
    int kind = GetStackInt(stack++);
    int chara = UserStatus->cur_chara;
    int ailment = 0;

    if (UserStatus->ailments[chara] != 0 && kind != 0) {
        ailment = UserStatus->ailment_frames[chara];
    }

    SetStack(stack, ailment);
    return 1;
}

int _SET_REFERENCE(RS_STACKDATA *stack, int argc) {
    int     monster_no = NowMonstorUnit->current_monster;
    int     target_no = GetStackInt(stack++);
    char   *name = GetStackString(stack);
    CFrame *frame;
    CFrame *anchor;

    if (name == NULL) {
        return 1;
    }

    frame = NowMonstorUnit->chara[monster_no][0].frame;
    anchor = NowMonstorUnit->chara[target_no][0].frame->SearchFrame(name);

    if (anchor == NULL) {
        return 1;
    }

    frame->SetReference(anchor);
    return 1;
}

int _DEL_REFERENCE(RS_STACKDATA *stack, int argc) {
    int     monster_no = GetStackInt(stack);
    CFrame *frame = NowMonstorUnit->chara[monster_no][0].frame;

    if (frame == NULL) {
        return 1;
    }

    frame->DeleteReference();
    return 1;
}

int _SET_SHADOW_FLAG(RS_STACKDATA *stack, int argc) {
    int monster_no = NowMonstorUnit->current_monster;

    NowMonstorUnit->monster[monster_no].shadow_enabled = GetStackInt(stack);
    return 1;
}

int BtSetEventScript(CRunScript *script, char *program, CDataAlloc2<1> *arena) {
    RS_STACKDATA *stack = (RS_STACKDATA *) arena->Alloc(64);

    script->load((RS_PROG_HEADER *) program, stack, 128, (RS_CALLDATA *) arena->Alloc(384), 512);
    script->ext_func(ext_func, 256);
    return 1;
}

/**
 * Opcodes of the monster scripts, ended by an entry with no function.
 */
static BT_EVENT_EXTERNAL_FUNCTION ext_func_info[] = {
    {_GET_DISTANCE,            10 },
    {_GET_POSITION,            11 },
    {_SET_ROTATION,            12 },
    {_CHK_ROTATION,            13 },
    {_CHK_MOVE,                14 },
    {_CHK_USER_INNER_PRODUCT,  15 },
    {_GET_VECTOR,              30 },
    {_GET_DIRECTION,           31 },
    {_SET_MOVE,                32 },
    {_CHK_MOVE_INFO,           33 },
    {_SET_MOVE_CANSEL,         34 },
    {_SET_ROT_CANSEL,          35 },
    {_SET_POSITION,            36 },
    {_STATUS_SET_FALL,         100},
    {_STATUS_SET_MUTEKI,       101},
    {_STATUS_SET_ALPHA,        102},
    {_STATUS_CHK_ALPHA,        103},
    {_STATUS_SET_DEAD,         104},
    {_STATUS_SET_PALLET,       105},
    {_GET_RAND,                180},
    {_GET_RANDF,               181},
    {_SIN_DEG,                 182},
    {_COS_DEG,                 183},
    {_STATUS_SET_CLIPLEVEL,    106},
    {_STATUS_SET_EVENT,        107},
    {_STATUS_SET_COL_OFF,      108},
    {_STATUS_GET_LIFE_RATE,    109},
    {_STATUS_GET_HEIGHT,       112},
    {_STATUS_GET_HITDMG_VOL,   113},
    {_STATUS_GET_MOTION_ID,    114},
    {_STATUS_GET_DMG_ID,       115},
    {_STATUS_SET_LOCKON_DIST,  116},
    {_STATUS_SET_SHADOW_LEN,   117},
    {_STATUS_SET_LOCKON_TRG,   118},
    {_STATUS_GET_USER_VECTOR,  110},
    {_SET_MOV_COL,             136},
    {_SET_BODY_COL,            130},
    {_SET_BODY_COL_PARA,       134},
    {_SET_DMG_COL,             131},
    {_SET_DMG_PARA,            132},
    {_SET_SHOT,                133},
    {_SET_SHOT2,               229},
    {_SET_SND_FRM,             140},
    {_SET_LOOP_SND,            230},
    {_STOP_LOOP_SND,           231},
    {_DEL_LOOP_SND,            232},
    {_SET_SND_NOW,             141},
    {_SET_MOTION,              200},
    {_CHK_MOTION_FRM,          201},
    {_GET_MOTION_FRM,          202},
    {_SET_MOTION_FRM,          203},
    {_GET_CHR_ID,              210},
    {_GET_COL_HIT_ID,          211},
    {_GET_SCRIPT_ID,           212},
    {_GET_MONSTOR_POS,         213},
    {_RUN_SCRIPT,              111},
    {_GET_MONSTOR_FRM,         214},
    {_SET_MONSTOR_POS,         215},
    {_SET_MONSTOR_MOVE,        216},
    {_SET_MONSTOR_LINK_MOVE,   217},
    {_SET_MONSTOR_MOVE_CANSEL, 218},
    {_SET_MONSTOR_MOTION,      219},
    {_SET_GLOBAL_INT,          220},
    {_GET_GLOBAL_INT,          221},
    {_GET_OBJ_POS,             222},
    {_SET_ROTATION_X,          223},
    {_SET_MOTION_CHANGE_STEP,  224},
    {_GET_MONSTOR_VECTOR,      225},
    {_SET_LOCKON_DIST,         226},
    {_SET_LOCKON_SW,           227},
    {_STOP_SND_NOW,            142},
    {_STATUS_SET_LIFE,         228},
    {_SET_BIN2,                209},
    {_SET_STATUS_CHANGE,       204},
    {_SET_TEX_ANIME_SW,        205},
    {_GET_STATUS_BIN2,         208},
    {_SET_COLLISION_WIDTH,     206},
    {_GET_NEAR_MONSTER,        207},
    {_BOSS_FADE_OUT,           240},
    {_CHEKC_FADE_OUT,          241},
    {_SET_GRAVITY,             242},
    {_SET_GUARD_FRAME,         244},
    {_GUARD_SEARCH,            245},
    {_GET_MOVE_VEC,            246},
    {_PUSH_IGLOBAL,            247},
    {_POP_IGLOBAL,             248},
    {_GET_USER_STATUS,         249},
    {_SET_REFERENCE,           250},
    {_DEL_REFERENCE,           251},
    {_LOOKAT,                  252},
    {_SET_SHADOW_FLAG,         253},
    {NULL,                     -1 },
};

void BtSetEventExtendTable() {
    int i;

    for (i = 0; i < 256; i++) {
        ext_func[i] = NULL;
    }

    for (i = 0;; i++) {
        if (ext_func_info[i].function == NULL) {
            break;
        }

        int j;

        for (j = 0; j < i; j++) {
            if (ext_func_info[i].operation == ext_func_info[j].operation) {
                printf("same ext_func_no!!!\n");

                while (1) {
                }
            }
        }

        int operation = ext_func_info[i].operation;

        if (operation < 0 || operation >= 256) {
            printf("ext func over!!");
        } else {
            ext_func[operation] = ext_func_info[i].function;
        }
    }
}
