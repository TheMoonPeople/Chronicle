#include "btsysscript.hpp"

#include <cstddef>
#include <format>
#include <span>
#include <string>

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

namespace {

// Held as a full pointer rather than in the s32 BtEventData, which cannot store a host address.
char *g_event_data = nullptr;

int GetStackInt(const RS_STACKDATA &argument) {
    if (argument.type == RS_FLOAT) {
        return static_cast<int>(argument.f);
    }

    return argument.i;
}

char *ScriptBufferTail() {
    return reinterpret_cast<char *>(BtSystemScriptFileBuffer.base + (static_cast<std::ptrdiff_t>(BtSystemScriptFileBuffer.used) * 0x10));
}

} // namespace

void BtSystemScriptLoad(int floor) {
    std::string path = std::format("dun/script/d0{}/event.stb", floor + 1);
    std::string mes_path = std::format("dun/script/d0{}/d0{}_{}.mes", floor + 1, floor + 1, LanguageCode);
    int         read_size = 0;
    int         mes_size = 0;

    BtSystemScriptFileBuffer.used = 0;
    g_event_data = ScriptBufferTail();
    LoadFile(path.data(), g_event_data, &read_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((read_size >> 4) + 1);
    char *mes = ScriptBufferTail();
    LoadFile(mes_path.data(), mes, &mes_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((mes_size >> 4) + 1);
    EdSetEventScript(g_event_data, mes, &BtSystemScriptFileBuffer);
    AddSystemEventScript();
}

int BtSystemScriptRun(int event, CDataAlloc2<1> *arena) {
    return EdEventInit(event, arena, g_event_data);
}

// The selection slot is held in PortItemSelectResult; BtEventInfo.item_select_result only flags
// that a slot is pending, since its s32 cannot store a host address.
int _ITEM_USE_WINDOW(RS_STACKDATA *stack, int argument_count) {
    if (stack == nullptr || argument_count < 1 || argument_count > 17 || stack->type != RS_PTR) {
        return 0;
    }

    std::span<RS_STACKDATA> arguments(stack, argument_count);
    PortItemSelectResult = arguments[0].p;
    BtEventInfo.item_select_result = arguments[0].p != nullptr;

    int item_count = argument_count - 1;

    for (int i = 0; i < item_count; i++) {
        BtEventInfo.item_select_list[i] = GetStackInt(arguments[i + 1]);
    }

    BtEventInfo.item_select_filtered = argument_count > 1;
    BtEventInfo.item_select_list[item_count] = -1;
    BtEventInfo.request = BT_REQUEST_ITEM_WINDOW;
    return 1;
}
