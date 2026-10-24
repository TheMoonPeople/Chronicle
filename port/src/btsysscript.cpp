#include "btsysscript.hpp"

#include <cstdio>
#include <memory>

#include "btitem_port.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "mainselect.hpp"
#include "nowload.hpp"

// Retail keeps the floor's event script in BtEventData, an s32, and casts it back to a pointer
// to run an event. Both ends are replaced to hold the address whole; nothing else reads it.

namespace {

char *g_event_data = nullptr;

int GetStackInt(RS_STACKDATA *argument) {
    if (argument->type == RS_FLOAT) {
        return static_cast<int>(argument->f);
    }

    return argument->i;
}

} // namespace

PORT_OVERRIDE void BtSystemScriptLoad(int floor) {
    auto  path = std::make_unique<char[]>(128);
    auto  mes_path = std::make_unique<char[]>(128);
    int   read_size;
    int   mes_size;
    char *mes;

    snprintf(path.get(), 128, "dun/script/d0%d/event.stb", floor + 1);
    snprintf(mes_path.get(), 128, "dun/script/d0%d/d0%d_%d.mes", floor + 1, floor + 1, LanguageCode);
    BtSystemScriptFileBuffer.used = 0;
    g_event_data = reinterpret_cast<char *>(BtSystemScriptFileBuffer.base + BtSystemScriptFileBuffer.used * 0x10);
    LoadFile(path.get(), g_event_data, &read_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((read_size >> 4) + 1);
    mes = reinterpret_cast<char *>(BtSystemScriptFileBuffer.base + BtSystemScriptFileBuffer.used * 0x10);
    LoadFile(mes_path.get(), mes, &mes_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((mes_size >> 4) + 1);
    EdSetEventScript(g_event_data, mes, &BtSystemScriptFileBuffer);
    AddSystemEventScript();
}

PORT_OVERRIDE int BtSystemScriptRun(int event, CDataAlloc2<1> *arena) {
    return EdEventInit(event, arena, g_event_data);
}

// Retail stores the slot's address in BtEventInfo.item_select_result, an s32, for
// BtMiniItemSelect_Loop to write the choice through; the s32 now only says that a slot is pending.
PORT_OVERRIDE int _ITEM_USE_WINDOW(RS_STACKDATA *stack, int argument_count) {
    if (stack == nullptr || argument_count < 1 || argument_count > 17 || stack->type != RS_PTR) {
        return 0;
    }

    PortItemSelectResult = stack->p;
    BtEventInfo.item_select_result = stack->p != nullptr;
    stack++;
    int i;

    for (i = 0; i < argument_count - 1; i++) {
        BtEventInfo.item_select_list[i] = GetStackInt(stack++);
    }

    BtEventInfo.item_select_filtered = false;

    if (argument_count > 1) {
        BtEventInfo.item_select_filtered = true;
    }

    BtEventInfo.item_select_list[i] = -1;
    BtEventInfo.request = BT_REQUEST_ITEM_WINDOW;
    return 1;
}
