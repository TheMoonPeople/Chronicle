#pragma once

// VU1_PROGRAM is passed by value in the DrawVu1 declarations, which were
// recovered from CodeWarrior mangling -- `11VU1_PROGRAM` in
// DrawVu1__10CVisualVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii.
// That tells us the name and that it is a user-defined type taken by value, so
// the declarations need a complete type rather than a forward declaration.
//
// The real definition is not known yet. Nothing implements any of those
// functions, so this placeholder only has to let the declarations compile; an
// enum mangles the same way a struct of this name would. Work out the real
// layout before implementing one of them.
// clang-format off
enum VU1_PROGRAM {
    VU1_PROGRAM_UNKNOWN = 0,
    VU1_PROGRAM_UNKNOWN6 = 6
};

// clang-format on
