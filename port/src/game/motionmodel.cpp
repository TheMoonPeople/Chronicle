#include "motionmodel.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "btmisc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "hitvalue.hpp"
#include "mglib.hpp"

/**
 * Returns the attribute bit for a weapon element, or zero for an invalid element.
 */
int GetWeaponElementAttr(int element) {
    if (element < 0 || element > 5) {
        element = 5;
    }

    return element_tbl[element];
}

void CMotionModel::LoadPack(unsigned int *pack, char *base_name, CDataAlloc2<1> *model_arena, CDataAlloc2<1> *motion_arena, MOTION_INFO *motion_info, int initialize_frames) {
    MOTION_FILE_INFO files[3];
    char             model_name[64];
    char             bone_name[64];
    char             motion_name[64];
    char             weight_name[76];
    int              model_size;
    unsigned int    *model_data;
    int              model_flags;

    strcpy(model_name, base_name);
    strcpy(bone_name, base_name);
    strcpy(motion_name, base_name);
    strcpy(weight_name, base_name);
    strcat(model_name, ".mds");
    strcat(bone_name, ".bbp");
    strcat(motion_name, ".mot");
    strcat(weight_name, ".wgt");
    model_flags = 0;

    if (initialize_frames != 0) {
        model_flags = 6;
    }

    model_data = GetPackFile(pack, model_name, &model_size);

    if (model_data == NULL) {
        printf("Model NotFound!! %s\n", model_name);
        exit__2(-1);
    }

    frame = (CFrame *) LoadMDSFile(model_data, model_arena, model_flags, NULL, NULL);
    printf("pack = %s\n", model_name);
    files[1].name = motion_name;
    files[0].name = bone_name;
    files[2].name = weight_name;
    files[0].size = 0;
    files[1].size = 0;
    files[2].size = 0;
    files[0].data = GetPackFile(pack, bone_name, &files[0].size);
    files[1].data = GetPackFile(pack, motion_name, &files[1].size);
    files[2].data = GetPackFile(pack, weight_name, &files[2].size);

    if (files[0].size == 0) {
        files[0].name = NULL;
    }

    if (files[1].size == 0) {
        files[1].name = NULL;
    }

    if (files[2].size == 0) {
        files[2].name = NULL;
    }

    CreateAnimeDataEX(&motion, motion_arena, files);

    if (initialize_frames != 0) {
        AnimeDataInit(frame, &motion, model_arena, &motion.frame_info);
    }

    motion.motion_info = motion_info;
    motion.state.time = motion_info->start;
    motion.state.blend_step = 1.0f;
    motion.state.motion_no = 0;
    motion.state.playing_no = 0;
    motion.state.blending = false;
    current_motion = 0;
}

void CMotionModel::Step() {
    motion.state.motion_no = current_motion;

    if (motion.state.motion_no != motion.state.playing_no) {
        motion.state.next_frame = motion.motion_info[current_motion].start;
    }

    SetMotionEX(frame, &motion, motion.motion_info, &motion.state, motion.frame_info);
}

void CMotionModel::Draw() {
    if (frame != NULL) {
        MGDraw(frame);
    }
}
