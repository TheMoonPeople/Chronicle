#include "runscript.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <print>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "battle_globals.hpp"
#include "dataalloc.hpp"
#include "editloop3.hpp"

namespace dc::game {

// The script VM on the host. A compiled script (.stb) is read where it was loaded, as retail
// does: the header, the program table and the instructions are 32-bit integers on disc and on
// the host alike. The function records are not: on disc they are {addr, name, local, arg}, four
// 32-bit words with the name a 32-bit offset, while the host's funcdata holds an 8-byte pointer.
// Every place the VM takes a funcdata from the file (run's entry and main, the CALL opcode)
// decodes the 16-byte record into a host funcdata kept beside it.
//
// The stacks are the other half. EdSetEventScript and BtSetEventScript carve 128 operand slots
// and 512 call records out of an arena at the PS2's 8 and 12 bytes; the host records are 16 and
// 24. load keeps the counts and gives each interpreter host-sized storage of its own.

void runerror(const char *message) {
    std::println(stderr, "RUNTIME ERROR: {}", message);
    exit__2(-1);
}

void stkoverflow() {
    runerror("stack overflow");
}

namespace {

// Retail's EdRunEvent has no return statement: its caller reads what the tail of RunEvent, the
// call to CRunScript::run, left in v0. The host has no such register to lean on, so run records
// its result.
int g_run_result = -1;

struct DiscFuncData {
    int32_t  addr;
    uint32_t name;
    int32_t  local;
    int32_t  arg;
};

static_assert(sizeof(DiscFuncData) == 16);

struct HostStacks {
    std::unique_ptr<RS_STACKDATA[]> stack;
    int                             stack_num = 0;
    std::unique_ptr<RS_CALLDATA[]>  call;
    int                             call_num = 0;
};

// Keyed by the record's address in the loaded file. A record decoded again after its buffer took
// another file is rewritten in place; the frames that still name it died with the old program.
std::unordered_map<const char *, std::unique_ptr<funcdata>> &FuncRecords() {
    static std::unordered_map<const char *, std::unique_ptr<funcdata>> records;
    return records;
}

std::unordered_map<const CRunScript *, HostStacks> &Stacks() {
    static std::unordered_map<const CRunScript *, HostStacks> stacks;
    return stacks;
}

// Names are zero in every shipped script; a non-zero one is an offset into the code section, as
// the strings PUSH_CONST pushes are.
funcdata *HostFunc(const char *code, const char *record) {
    DiscFuncData disc{};
    std::memcpy(&disc, record, sizeof(disc));
    std::unique_ptr<funcdata> &slot = FuncRecords()[record];
    if (!slot) {
        slot = std::make_unique<funcdata>();
    }
    slot->addr = disc.addr;
    slot->name = disc.name != 0 ? const_cast<char *>(code + disc.name) : nullptr;
    slot->local = disc.local;
    slot->arg = disc.arg;
    return slot.get();
}

const char *FuncName(const funcdata *func) {
    return func != nullptr && func->name != nullptr ? func->name : "(null)";
}

vmcode_t *CodeAt(char *code, int offset) {
    return reinterpret_cast<vmcode_t *>(code + offset);
}

// The item the next kEventItemGet shows, or -1.
int g_item_get_item = -1;

// The town script's item-get routine, as the scripts call it: ItemGet(slot, item, fade, camera), slot 5
// being the character it puts the glow (/chara/iget.chr) in, fade the frames it fades out over when
// done (0: none) and camera its angle (-1: its own). The scripts carry no names, so it is found by
// what it does: of the functions the programs reach, the one of four arguments that pushes
// "/chara/iget.chr". Kept for the program it was found in.
const char *ItemGetRecord(CRunScript &script) {
    static const RS_PROG_HEADER *found_in = nullptr;
    static const char           *found = nullptr;
    if (script.prog == nullptr) {
        return nullptr;
    }
    if (found_in == script.prog) {
        return found;
    }
    found_in = script.prog;
    found = nullptr;

    char                            *base = reinterpret_cast<char *>(script.prog);
    char                            *code = script.code;
    std::vector<const char *>        pending;
    std::unordered_set<const char *> seen;
    const auto                      *entry = reinterpret_cast<const RS_PROGDATA *>(base + script.prog->prog);
    pending.reserve(static_cast<size_t>(script.prog->prog_num));
    for (int i = 0; i < script.prog->prog_num; i++) {
        pending.push_back(base + entry[i].func);
    }

    // Each function's instructions run from its address to the first word that is no opcode (its
    // end, a record or a string); a script function is far shorter than the limit.
    constexpr int most_instructions = 20000;
    constexpr int first_opcode = 1;
    constexpr int last_opcode = 30;
    while (!pending.empty()) {
        const char *record = pending.back();
        pending.pop_back();
        if (!seen.insert(record).second) {
            continue;
        }
        DiscFuncData disc{};
        std::memcpy(&disc, record, sizeof(disc));
        if (disc.addr < 0) {
            continue;
        }
        const auto *pc = reinterpret_cast<const vmcode_t *>(code + disc.addr);
        for (int n = 0; n < most_instructions; n++, pc++) {
            if (pc->op < first_opcode || pc->op > last_opcode) {
                break;
            }
            if (pc->op == RS_OP_CALL) {
                pending.push_back(code + pc->arg2);
            } else if (pc->op == RS_OP_PUSH_CONST && pc->arg1 == 3 && disc.arg == 4 &&
                       std::strcmp(code + pc->arg2, "/chara/iget.chr") == 0) {
                return found = record;
            }
        }
    }
    return nullptr;
}

} // namespace

bool EventScriptHasItemGet() {
    return ItemGetRecord(EdEventScript) != nullptr;
}

void EventScriptSetItemGet(int item) {
    g_item_get_item = item;
}

int chk_int(RS_STACKDATA data, funcdata *function) {
    if (data.type == RS_INT) {
        return data.i;
    }

    std::println(stderr, "RUNTIME ERROR: {}: operand is not integer", FuncName(function));
    exit__2(-1);
    return 0;
}

int is_true(RS_STACKDATA data) {
    return data.type != RS_INT || data.i != 0;
}

void divby0error() {
    runerror("Divide by 0");
}

void modby0error() {
    runerror("Modulo by 0");
}

void print(RS_STACKDATA *data, int count) {
    for (int index = 0; index < count; index++) {
        const RS_STACKDATA &value = data[index];
        // printf rather than std::print: script strings are Shift-JIS and must reach stdout as raw bytes.
        if (value.type == RS_INT) {
            std::printf("%d", value.i);
        } else if (value.type == RS_STR) {
            std::printf("%s", value.s);
        } else if (value.type == RS_FLOAT) {
            std::printf("%f", value.f);
        }

        std::fflush(stdout);
    }
}

CRunScript::CRunScript() {
    sp = stack;
    stack_end = stack;
    call_sp = call;
    call_end = call;
    pc = nullptr;
    ext_func_num = 0;
    stack_num = 0;
    call_num = 0;
    skip_wait = 0;
}

void CRunScript::check_stack() {
    if (sp >= stack_end) {
        stkoverflow();
    }
}

void CRunScript::push(RS_STACKDATA value) {
    check_stack();
    *sp++ = value;
}

void CRunScript::push_int(int value) {
    check_stack();
    sp->type = RS_INT;
    sp++->i = value;
}

void CRunScript::push_str(char *string) {
    check_stack();
    sp->type = RS_STR;
    sp++->s = string;
}

void CRunScript::push_ptr(RS_STACKDATA *pointer) {
    check_stack();
    sp->type = RS_PTR;
    sp++->p = pointer;
}

void CRunScript::push_float(float value) {
    check_stack();
    sp->type = RS_FLOAT;
    sp++->f = value;
}

RS_STACKDATA CRunScript::pop() {
    return *--sp;
}

// A call takes its arguments from the operand stack in place: the new frame starts where the
// caller left them, so nothing is copied and only the locals past them have to be cleared.
vmcode_t *CRunScript::call_func(funcdata *callee, vmcode_t *return_pc) {
    if (call_sp >= call_end) {
        std::fputs("\202\261\202\352\210\310\217\343\212\326\220\224\214\304\202\321\217\157\202\265"
                   "\202\252\202\305\202\253\202\334\202\271\202\361\201\102\n",
                   stdout);
        exit__2(-2);
    }

    call_sp->ret = return_pc;
    call_sp->func = func;
    call_sp->frame = frame;
    frame = sp - callee->arg;
    sp = frame + callee->local;
    func = callee;
    call_sp++;
    std::memset(frame + func->arg, 0, (func->local - func->arg) * sizeof(RS_STACKDATA));
    check_stack();
    return reinterpret_cast<vmcode_t *>(code + callee->addr);
}

vmcode_t *CRunScript::ret_func() {
    call_sp--;
    frame = call_sp->frame;
    func = call_sp->func;
    return call_sp->ret;
}

void CRunScript::ext(RS_STACKDATA *args, int argc) {
    const int index = args[0].i;
    if (index < 0 || index >= ext_func_num || ext_func_table[index] == nullptr) {
        std::println("not found ext {}", index);
    } else if (ext_func_table[index](&args[1], argc - 1) == 0) {
        std::println("illegal function call ext {}", index);
    }
}

void CRunScript::load(RS_PROG_HEADER *prog, [[maybe_unused]] RS_STACKDATA *stack, int stack_num, [[maybe_unused]] RS_CALLDATA *call,
                      int call_num) {
    HostStacks &host = Stacks()[this];
    if (host.stack_num != stack_num) {
        host.stack = std::make_unique<RS_STACKDATA[]>(static_cast<size_t>(stack_num));
        host.stack_num = stack_num;
    }
    if (host.call_num != call_num) {
        host.call = std::make_unique<RS_CALLDATA[]>(static_cast<size_t>(call_num));
        host.call_num = call_num;
    }
    this->stack = host.stack.get();
    this->stack_num = stack_num;
    this->call = host.call.get();
    this->call_num = call_num;
    this->stack_end = this->stack + stack_num;
    this->call_end = this->call + call_num;
    this->prog = prog;
    this->code = reinterpret_cast<char *>(prog) + prog->code;
}

void CRunScript::reload(RS_PROG_HEADER *prog) {
    this->prog = prog;
    code = reinterpret_cast<char *>(prog) + prog->code;
}

void CRunScript::ext_func(int (**table)(RS_STACKDATA *, int), int count) {
    ext_func_table = table;
    ext_func_num = count;
}

void CRunScript::resume() {
    if (pc != nullptr) {
        exe(pc);
    }
}

// Retail's body, with the return it leaves to v0: 1 for an event still running, 0 for one that
// ran to its end and -1 for a program the script does not have. EdEventInit treats the last two
// as an event with nothing to show.
int PortEdRunEvent(int program, CDataAlloc2<1> *arena) {
    RunEvent(&EdEventScript, program, arena);
    return g_run_result;
}

int CRunScript::run(int no) {
    if (prog == nullptr) {
        return g_run_result = -1;
    }

    sp = stack;
    call_sp = call;
    func = nullptr;

    char *base = reinterpret_cast<char *>(prog);

    // The port's item-get event: the town's own routine, given the item (runscript.hpp).
    if (no == kEventItemGet && this == &EdEventScript) {
        const char *record = ItemGetRecord(*this);
        const int   item = g_item_get_item;
        g_item_get_item = -1;
        if (record == nullptr || item < 0) {
            std::println("no item-get routine for item {}", item);
            return g_run_result = -1;
        }
        func = HostFunc(code, record);
        frame = stack;
        frame[0].type = RS_INT;
        frame[0].i = 5;
        frame[1].type = RS_INT;
        frame[1].i = item;
        frame[2].type = RS_INT;
        frame[2].i = 0;
        frame[3].type = RS_FLOAT;
        frame[3].f = -1.0f;
        sp = frame + func->local;
        check_stack();
        std::memset(frame + func->arg, 0, (func->local - func->arg) * sizeof(RS_STACKDATA));
        end = 0;
        skip_wait = 0;
        exe(CodeAt(code, func->addr));
        return g_run_result = end != 0 ? 0 : 1;
    }

    if (no < 0) {
        func = HostFunc(code, base + prog->main);
    } else {
        const auto *entries = reinterpret_cast<const RS_PROGDATA *>(base + prog->prog);

        for (int i = 0; i < prog->prog_num; i++) {
            if (entries[i].no == no) {
                func = HostFunc(code, base + entries[i].func);
                break;
            }
        }
    }

    if (func == nullptr) {
        std::println("not found program {}", no);
        return g_run_result = -1;
    }

    frame = sp - func->arg;
    sp = frame + func->local;
    check_stack();
    std::memset(frame + func->arg, 0, (func->local - func->arg) * sizeof(RS_STACKDATA));
    vmcode_t *start = CodeAt(code, func->addr);
    end = 0;
    skip_wait = 0;
    exe(start);
    return g_run_result = end != 0 ? 0 : 1;
}

int CRunScript::check_program(int no) {
    const auto *entries = reinterpret_cast<const RS_PROGDATA *>(reinterpret_cast<const char *>(prog) + prog->prog);

    for (int index = 0; index < prog->prog_num; index++) {
        if (entries[index].no == no) {
            return 1;
        }
    }

    return 0;
}

void CRunScript::skip() {
    skip_wait = 1;
    resume();
}

// Retail's interpreter, opcode for opcode, except for CALL's record decoding and the diagnostics' names.
void CRunScript::exe(vmcode_t *entry) {
    RS_STACKDATA value;
    RS_STACKDATA rhs;
    RS_STACKDATA lhs;
    float        rhs_f = 0.0f;
    float        lhs_f = 0.0f;
    int          rhs_i = 0;
    int          lhs_i = 0;

    pc = entry;

    for (;;) {
        switch (pc->op) {
            case RS_OP_LOAD:
                switch (pc->arg2) {
                    case RS_ADDR_LOCAL:
                        push(frame[pc->arg1]);
                        break;
                    case RS_ADDR_LOCAL_INDEX:
                        push(frame[pc->arg1 + chk_int(pop(), func)]);
                        break;
                    case RS_ADDR_POINTER_INDEX:
                        push(frame[pc->arg1].p[chk_int(pop(), func)]);
                        break;
                    case RS_ADDR_LOCAL_FLOAT:
                        frame[pc->arg1].type = RS_FLOAT;
                        push(frame[pc->arg1]);
                        break;
                    case RS_ADDR_LOCAL_INDEX_FLOAT:
                        rhs_i = chk_int(pop(), func);
                        frame[pc->arg1 + rhs_i].type = RS_FLOAT;
                        push(frame[pc->arg1 + rhs_i]);
                        break;
                    case RS_ADDR_POINTER_INDEX_FLOAT:
                        rhs_i = chk_int(pop(), func);
                        frame[pc->arg1].p[rhs_i].type = RS_FLOAT;
                        push(frame[pc->arg1].p[rhs_i]);
                        break;
                    default:
                        break;
                }

                break;
            case RS_OP_LOAD_ADDR:
                switch (pc->arg2) {
                    case RS_ADDR_LOCAL:
                    case RS_ADDR_LOCAL_FLOAT:
                        push_ptr(frame + pc->arg1);
                        break;
                    case RS_ADDR_LOCAL_INDEX:
                    case RS_ADDR_LOCAL_INDEX_FLOAT:
                        push_ptr(frame + pc->arg1 + chk_int(pop(), func));
                        break;
                    case RS_ADDR_POINTER_INDEX:
                    case RS_ADDR_POINTER_INDEX_FLOAT:
                        push_ptr(frame[pc->arg1].p + chk_int(pop(), func));
                        break;
                    default:
                        break;
                }

                break;
            case RS_OP_STORE:
                value = pop();
                *pop().p = value;
                push(value);
                break;
            case RS_OP_PUSH_CONST:
                if (pc->arg1 == 1) {
                    push_int(pc->arg2);
                } else if (pc->arg1 == 3) {
                    push_str(code + pc->arg2);
                } else if (pc->arg1 == 2) {
                    float constant = 0.0f;
                    std::memcpy(&constant, &pc->arg2, sizeof(constant));
                    push_float(constant);
                }

                break;
            case RS_OP_POP:
                sp--;
                break;
            case RS_OP_JMP:
                if (!skip_wait) {
                    pc = CodeAt(code, pc->arg1);
                    continue;
                }

                break;
            case RS_OP_JMP_TRUE:
                if (!skip_wait && is_true(pop())) {
                    if (pc->arg2) {
                        push_int(1);
                    }

                    pc = CodeAt(code, pc->arg1);
                    continue;
                }

                break;
            case RS_OP_JMP_FALSE:
                if (!skip_wait && !is_true(pop())) {
                    if (pc->arg2) {
                        push_int(0);
                    }

                    pc = CodeAt(code, pc->arg1);
                    continue;
                }

                break;
            case RS_OP_CMP:
                rhs = pop();
                lhs = pop();

                if (lhs.type == RS_INT && rhs.type == RS_INT) {
                    rhs_i = rhs.i;
                    lhs_i = lhs.i;

                    switch (pc->arg1) {
                        case RS_CMP_EQ:
                            push_int(rhs_i == lhs_i);
                            break;
                        case RS_CMP_NE:
                            push_int(rhs_i != lhs_i);
                            break;
                        case RS_CMP_LT:
                            push_int(lhs_i < rhs_i);
                            break;
                        case RS_CMP_LE:
                            push_int(lhs_i <= rhs_i);
                            break;
                        case RS_CMP_GT:
                            push_int(lhs_i > rhs_i);
                            break;
                        case RS_CMP_GE:
                            push_int(lhs_i >= rhs_i);
                            break;
                        default:
                            break;
                    }
                } else {
                    if (lhs.type == RS_FLOAT && rhs.type == RS_FLOAT) {
                        rhs_f = rhs.f;
                        lhs_f = lhs.f;
                    } else if (lhs.type == RS_INT && rhs.type == RS_FLOAT) {
                        rhs_f = rhs.f;
                        lhs_f = lhs.i;
                    } else if (lhs.type == RS_FLOAT && rhs.type == RS_INT) {
                        rhs_f = rhs.i;
                        lhs_f = lhs.f;
                    } else {
                        std::println(stderr, "RUNTIME ERROR at _CMP: {}: operand is not number", FuncName(func));
                        exit__2(-1);
                    }

                    switch (pc->arg1) {
                        case RS_CMP_EQ:
                            push_int(rhs_f == lhs_f);
                            break;
                        case RS_CMP_NE:
                            push_int(rhs_f != lhs_f);
                            break;
                        case RS_CMP_LT:
                            push_int(lhs_f < rhs_f);
                            break;
                        case RS_CMP_LE:
                            push_int(lhs_f <= rhs_f);
                            break;
                        case RS_CMP_GT:
                            push_int(lhs_f > rhs_f);
                            break;
                        case RS_CMP_GE:
                            push_int(lhs_f >= rhs_f);
                            break;
                        default:
                            break;
                    }
                }

                break;
            case RS_OP_ADD:
                rhs = pop();
                lhs = pop();

                if (lhs.type == RS_INT && rhs.type == RS_INT) {
                    push_int(lhs.i + rhs.i);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_FLOAT) {
                    push_float(lhs.f + rhs.f);
                } else if (lhs.type == RS_INT && rhs.type == RS_FLOAT) {
                    push_float(lhs.i + rhs.f);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_INT) {
                    push_float(lhs.f + rhs.i);
                } else {
                    std::println(stderr, "RUNTIME ERROR at _ADD: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_SUB:
                rhs = pop();
                lhs = pop();

                if (lhs.type == RS_INT && rhs.type == RS_INT) {
                    push_int(lhs.i - rhs.i);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_FLOAT) {
                    push_float(lhs.f - rhs.f);
                } else if (lhs.type == RS_INT && rhs.type == RS_FLOAT) {
                    push_float(lhs.i - rhs.f);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_INT) {
                    push_float(lhs.f - rhs.i);
                } else {
                    std::println(stderr, "RUNTIME ERROR at _SUB: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_MUL:
                rhs = pop();
                lhs = pop();

                if (lhs.type == RS_INT && rhs.type == RS_INT) {
                    push_int(lhs.i * rhs.i);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_FLOAT) {
                    push_float(lhs.f * rhs.f);
                } else if (lhs.type == RS_INT && rhs.type == RS_FLOAT) {
                    push_float(lhs.i * rhs.f);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_INT) {
                    push_float(lhs.f * rhs.i);
                } else {
                    std::println(stderr, "RUNTIME ERROR _MUL: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_DIV:
                rhs = pop();

                // Retail tests the integer view whatever the type: a float divisor of +0.0 stops it
                // too, -0.0 does not.
                if (rhs.i == 0) {
                    divby0error();
                }

                lhs = pop();

                if (lhs.type == RS_INT && rhs.type == RS_INT) {
                    push_int(lhs.i / rhs.i);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_FLOAT) {
                    push_float(lhs.f / rhs.f);
                } else if (lhs.type == RS_INT && rhs.type == RS_FLOAT) {
                    push_float(lhs.i / rhs.f);
                } else if (lhs.type == RS_FLOAT && rhs.type == RS_INT) {
                    push_float(lhs.f / rhs.i);
                } else {
                    std::println(stderr, "RUNTIME ERROR at _DIV: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_MOD:
                rhs_i = chk_int(pop(), func);

                if (rhs_i == 0) {
                    modby0error();
                }

                push_int(chk_int(pop(), func) % rhs_i);
                break;
            case RS_OP_AND:
                rhs_i = chk_int(pop(), func);
                push_int(rhs_i & chk_int(pop(), func));
                break;
            case RS_OP_OR:
                rhs_i = chk_int(pop(), func);
                push_int(rhs_i | chk_int(pop(), func));
                break;
            case RS_OP_NEG:
                rhs = pop();

                if (rhs.type == RS_INT) {
                    push_int(-rhs.i);
                } else if (rhs.type == RS_FLOAT) {
                    push_float(-rhs.f);
                } else {
                    std::println(stderr, "RUNTIME ERROR at _INVT: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_SIN:
                rhs = pop();

                if (rhs.type == RS_INT) {
                    push_float(sinf(rhs.i));
                } else if (rhs.type == RS_FLOAT) {
                    push_float(sinf(rhs.f));
                } else {
                    std::println(stderr, "RUNTIME ERROR at _SIN: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_COS:
                rhs = pop();

                if (rhs.type == RS_INT) {
                    push_float(cosf(rhs.i));
                } else if (rhs.type == RS_FLOAT) {
                    push_float(cosf(rhs.f));
                } else {
                    std::println(stderr, "RUNTIME ERROR at _COS: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_NOT:
                rhs = pop();

                if (rhs.type == RS_INT) {
                    push_int(!rhs.i);
                } else {
                    fprintf(stderr,
                            "RUNTIME ERROR: %s: \220\256\220\224\202\305\202\310\202\242\203\111"
                            "\203\171\203\211\203\223\203\150\n",
                            FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_ITOF:
                rhs = pop();

                if (rhs.type == RS_INT) {
                    push_float(rhs.i);
                } else if (rhs.type == RS_FLOAT) {
                    push_float(rhs.f);
                } else {
                    std::println(stderr, "RUNTIME ERROR at _ITOF: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_FTOI:
                rhs = pop();

                if (rhs.type == RS_INT) {
                    push_int(rhs.i);
                } else if (rhs.type == RS_FLOAT) {
                    push_int(static_cast<int>(rhs.f));
                } else {
                    std::println(stderr, "RUNTIME ERROR at _FTOI: {}: operand is not number", FuncName(func));
                    exit__2(-1);
                }

                break;
            case RS_OP_PRINT:
                sp -= pc->arg1;
                print(sp, pc->arg1);
                break;
            case RS_OP_EXT:
                sp -= pc->arg1;

                if (!skip_wait) {
                    ext(sp, pc->arg1);
                }

                break;
            case RS_OP_END:
                end = 1;
                pc = nullptr;
                return;
            case RS_OP_CALL:
                pc = call_func(HostFunc(code, code + pc->arg2), pc);
                pc--;
                break;
            case RS_OP_RET:
                value = pop();

                if (call_sp != call) {
                    sp = frame;
                    pc = ret_func();
                } else {
                    result = value.i;
                    push(value);
                    pc = nullptr;
                    end = 1;
                    return;
                }

                push(value);
                break;
            case RS_OP_WAIT:
                if (!skip_wait) {
                    pc++;
                    return;
                }

                break;
            case RS_OP_SKIP_END:
                if (skip_wait) {
                    skip_wait = 0;
                    pc++;
                    return;
                }

                break;
            default:
                break;
        }

        pc++;
    }
}

} // namespace dc::game
