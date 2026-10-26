#include "effectgroup.hpp"

#include "effect.hpp"

int CEffectGroup::EnterEffect(CEffectParam *parameters) {
    for (int i = 0; i < capacity; i++) {
        if (effect_table[i].active == 0) {
            effect_table[i].SetEffect(parameters);
            return 1;
        }
    }

    return 0;
}

void CEffectGroup::Step(int unused) {
    for (int i = 0; i < capacity; i++) {
        effect_table[i].Step(1);
    }
}

void CEffectGroup::Draw() {
    for (int i = 0; i < capacity; i++) {
        effect_table[i].Draw();
    }
}

void CEffectGroup::Clear() {
    for (int i = 0; i < capacity; i++) {
        effect_table[i].Initialize();
    }
}

void CEffectGroup::Initialize(CEffect *table, int count) {
    effect_table = table;
    capacity = count;
}
