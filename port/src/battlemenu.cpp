#include "battlemenu/internal.hpp"

PORT_OVERRIDE void DrawBattleMain() {
    PortDrawBattleMenuMain();
}

PORT_OVERRIDE void BattleMenuDraw() {
    PortDrawBattleMenu();
}

PORT_OVERRIDE int BattleMenuSelect() {
    return PortBattleMenuSelect();
}

PORT_OVERRIDE void DrawMenuMove() {
    PortDrawMenuMove();
}

PORT_OVERRIDE void DrawWeaponSelectDialog(int x, int y, int alpha) {
    PortDrawWeaponSelectDialog(x, y, alpha);
}

PORT_OVERRIDE void DrawBuildUpWeaponSelect(int x, int y, int cursor) {
    PortDrawBuildUpWeaponSelect(x, y, cursor);
}

PORT_OVERRIDE void DrawAallWeapon(int x, int y, float depth, CCharacter *model,
                                  WEAPON_HAVE *weapon, int equipped, int selected, int alpha) {
    PortDrawAallWeapon(x, y, depth, model, weapon, equipped, selected, alpha);
}

PORT_OVERRIDE void BtlWeaponDraw(int x, float depth, int chara, int alpha) {
    PortBtlWeaponDraw(x, depth, chara, alpha);
}

PORT_OVERRIDE void WeaponMenuDraw() {
    PortWeaponMenuDraw();
}
