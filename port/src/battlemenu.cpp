#include "battlemenu/internal.hpp"

void DrawBattleMain() {
    PortDrawBattleMenuMain();
}

void BattleMenuDraw() {
    PortDrawBattleMenu();
}

int BattleMenuSelect() {
    return PortBattleMenuSelect();
}

void DrawMenuMove() {
    PortDrawMenuMove();
}

void DrawWeaponSelectDialog(int x, int y, int alpha) {
    PortDrawWeaponSelectDialog(x, y, alpha);
}

void DrawBuildUpWeaponSelect(int x, int y, int cursor) {
    PortDrawBuildUpWeaponSelect(x, y, cursor);
}

void DrawAallWeapon(int x, int y, float depth, CCharacter *model,
                    WEAPON_HAVE *weapon, int equipped, int selected, int alpha) {
    PortDrawAallWeapon(x, y, depth, model, weapon, equipped, selected, alpha);
}

void BtlWeaponDraw(int x, float depth, int chara, int alpha) {
    PortBtlWeaponDraw(x, depth, chara, alpha);
}

void WeaponMenuDraw() {
    PortWeaponMenuDraw();
}
