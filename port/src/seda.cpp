#include "seda.hpp"

#include <cstring>
#include <iterator>

#include "character.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "sedamod.hpp"

namespace {

bool g_forced = false;
bool g_equipped_hidden = false;

// Toan's KEYs (dun/mainchara/c01d.chr), the frames his attack code times things by.
struct ToanKey {
    float start;
    float end;
};

constexpr ToanKey kToanKeys[] = {
    {10,  20 },
    {70,  90 },
    {35,  55 },
    {191, 196},
    {285, 291},
    {300, 300},
    {295, 335},
    {315, 335},
    {210, 215},
    {216, 224},
    {224, 230},
    {116, 130},
    {136, 150},
    {155, 160},
    {160, 170},
    {180, 191},
    {191, 192},
    {196, 206},
    {100, 110},
    {340, 360},
    {370, 390},
    {400, 420},
    {430, 450},
    {460, 480},
    {715, 750},
    {196, 196},
    {546, 546},
    {546, 566},
    {620, 655},
    {690, 710},
    {756, 766},
    {660, 680},
    {570, 590},
    {778, 788},
    {792, 802},
    {805, 815},
    {820, 830},
    {830, 838},
    {838, 847},
    {847, 857},
    {856, 884},
};

} // namespace

void SedaSetForced(bool forced) {
    g_forced = forced;
}

bool SedaActive() {
    return g_forced;
}

void SedaSetEquippedHidden(bool hidden) {
    g_equipped_hidden = hidden;
}

namespace {

// Whether a frame or one below it is one of Toan's (his models' objects are named c01...). Not the
// frame's brothers: the character's frame may hang among the dungeon's other frames.
bool HasToanFrame(const CFrame *frame, int depth = 0) {
    if (std::strncmp(frame->name, "c01", 3) == 0) {
        return true;
    }
    for (const CFrame *child = frame->child; child != nullptr && depth < 256; child = child->brother) {
        if (HasToanFrame(child, depth + 1)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool SedaRefusesMotionSet(CCharacter *chara, int set) {
    return SedaActive() && chara == &CharaMain && set > 0 && chara->frame != nullptr && !HasToanFrame(chara->frame);
}

bool SedaEquippedDrawn() {
    return !(SedaActive() && g_equipped_hidden);
}

float SedaToanTime(float time) {
    if (!SedaActive() || CharaMain.motion_type.motion_info == nullptr) {
        return time;
    }
    int key = CharaMain.motion_no;
    if (key < 0 || key >= static_cast<int>(std::size(kToanKeys))) {
        return time;
    }
    const ToanKey &toan = kToanKeys[key];
    float          start = static_cast<float>(CharaMain.motion_type.motion_info[key].start);
    float          end = static_cast<float>(CharaMain.motion_type.motion_info[key].end);
    if (end <= start) {
        return toan.start;
    }
    float share = (time - start) / (end - start);
    if (share < 0.0f || share > 1.0f) {
        return time < start ? toan.start - 1.0f : toan.end + 1.0f; // outside the key: outside Toan's too
    }
    return toan.start + share * (toan.end - toan.start);
}
