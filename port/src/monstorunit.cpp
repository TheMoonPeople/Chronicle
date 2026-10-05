#include "monstorunit.hpp"

#include <libvu0.h>

#include "character.hpp"
#include "framevu1.hpp"
#include "mglib.hpp"

#include "platform/config.hpp"

// Retail's DrawShadowMonstor, which casts the shadows of the monsters taking part. A dormant
// monster within video.detail_distance of the player draws its model (DrawMonstorDetail,
// dun/gameloop.cpp), so it casts its shadow here too.
PC_OVERRIDE void CMonstorUnit::DrawShadowMonstor() {
    sceVu0FVECTOR position;
    sceVu0FVECTOR rotation;
    sceVu0FVECTOR light = {0.0f, 1.0f, 0.0f, 0.0f};
    float         detail = ConfigDetailDistance();

    for (int i = 0; i < 16; i++) {
        bool drawn = monster[i].state == 2 || (monster[i].state == 1 && monster[i].player_distance < detail);

        if (drawn && monster[i].shadow_visible != 0 && monster[i].revealed != 0) {
            if (chara[i][0].shadow_frame != NULL) {
                CCharacter *character = &chara[i][0];
                character->ShadowStep();
                character->GetPosition(position);
                character->GetRotation(rotation);
                chara[i][0].shadow_frame->SetPosition(position);
                chara[i][0].shadow_frame->SetRotation(0.0f, rotation[1], 0.0f);
                position[1] -= monster[i].shadow_length;
                MGDrawShadowFast(chara[i][0].shadow_frame, position, light);
            }
        }
    }
}
