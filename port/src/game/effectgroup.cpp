#include "effectgroup.hpp"

#include <span>

#include "effect.hpp"

namespace dc::game {

namespace {

std::span<CEffect> Pool(CEffect *table, int count) {
    return {table, static_cast<std::size_t>(count > 0 ? count : 0)};
}

} // namespace

int CEffectGroup::EnterEffect(CEffectParam *parameters) {
    for (CEffect &effect : Pool(effect_table, capacity)) {
        if (effect.active == 0) {
            effect.SetEffect(parameters);
            return 1;
        }
    }

    return 0;
}

void CEffectGroup::Step(int /*unused*/) {
    for (CEffect &effect : Pool(effect_table, capacity)) {
        effect.Step(1);
    }
}

void CEffectGroup::Draw() {
    for (CEffect &effect : Pool(effect_table, capacity)) {
        effect.Draw();
    }
}

void CEffectGroup::Clear() {
    for (CEffect &effect : Pool(effect_table, capacity)) {
        effect.Initialize();
    }
}

void CEffectGroup::Initialize(CEffect *table, int count) {
    effect_table = table;
    capacity = count;
}

} // namespace dc::game
