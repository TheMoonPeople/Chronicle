#include "motionmodel.hpp"

#include <array>
#include <cstdlib>
#include <print>
#include <string>

#include "btmisc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "hitvalue.hpp"
#include "mglib.hpp"

namespace dc::game {

int GetWeaponElementAttr(int element) {
    if (element < 0 || element > 5) {
        element = 5;
    }

    return element_tbl[element];
}

void CMotionModel::LoadPack(unsigned int *pack, const char *base_name, CDataAlloc2<1> *model_arena, CDataAlloc2<1> *motion_arena, MOTION_INFO *motion_info, int initialize_frames) {
    std::array<MOTION_FILE_INFO, 3> files{};
    const std::string               base(base_name);
    std::string                     model_name = base + ".mds";
    std::string                     bone_name = base + ".bbp";
    std::string                     motion_name = base + ".mot";
    std::string                     weight_name = base + ".wgt";
    int                             model_size = 0;
    int                             model_flags = 0;

    if (initialize_frames != 0) {
        model_flags = 6;
    }

    unsigned int *model_data = GetPackFile(pack, model_name.data(), &model_size);

    if (model_data == nullptr) {
        std::println("Model NotFound!! {}", model_name);
        exit__2(-1);
    }

    frame = LoadMDSFile(model_data, model_arena, model_flags, nullptr, nullptr);
    std::println("pack = {}", model_name);
    files[1].name = motion_name.data();
    files[0].name = bone_name.data();
    files[2].name = weight_name.data();
    files[0].size = 0;
    files[1].size = 0;
    files[2].size = 0;
    files[0].data = GetPackFile(pack, bone_name.data(), &files[0].size);
    files[1].data = GetPackFile(pack, motion_name.data(), &files[1].size);
    files[2].data = GetPackFile(pack, weight_name.data(), &files[2].size);

    for (MOTION_FILE_INFO &file : files) {
        if (file.size == 0) {
            file.name = nullptr;
        }
    }

    CreateAnimeDataEX(&motion, motion_arena, files.data());

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
    if (frame != nullptr) {
        MGDraw(frame);
    }
}

} // namespace dc::game
