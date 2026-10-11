#include "dun/keep_lock_on.hpp"

#include "common.h"

#include "dun/gameloop.hpp"
#include "monstorunit.hpp"
#include "platform/config.hpp"

extern s32 lockOnTargetNo;
extern s32 targetCursorShiftNo;
extern s32 targetCursorShiftRot;

namespace {

struct KeptLockOn {
    bool locked = false;
    s32  target = -1;
    s32  shift_no = -1;
    s32  shift_rot = 0;
};

KeptLockOn kept;

} // namespace

void KeepLockOnSave() {
    kept = KeptLockOn{};

    if (!ConfigGet().keep_lock_on || lockOnTargetFlag == 0 || lockOnTargetNo < 0 || lockOnTargetNo >= 16) {
        return;
    }

    kept.locked = true;
    kept.target = lockOnTargetNo;
    kept.shift_no = targetCursorShiftNo;
    kept.shift_rot = targetCursorShiftRot;
}

void KeepLockOnRestore() {
    KeptLockOn restore = kept;
    kept = KeptLockOn{};

    if (!restore.locked || NowMonstorUnit == nullptr) {
        return;
    }

    // What setTargetCursor lets go of at once, the lock is not put back on.
    const MONSTOR &monster = NowMonstorUnit->monster[restore.target];

    if (monster.state != 2 || monster.hp <= 0 || monster.lockon_enabled == 0) {
        return;
    }

    lockOnTargetNo = restore.target;
    lockOnTargetFlag = 1;
    targetCursorShiftNo = restore.shift_no;
    targetCursorShiftRot = restore.shift_rot;
}
