#include "runscript.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "battle_globals.hpp"
#include "dataalloc.hpp"

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
    DiscFuncData disc;
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

} // namespace

int chk_int(RS_STACKDATA data, funcdata *function) {
    if (data.type == RS_INT) {
        return data.i;
    }

    fprintf(stderr, "RUNTIME ERROR: %s: operand is not integer\n", FuncName(function));
    exit__2(-1);
    return 0;
}

void CRunScript::load(RS_PROG_HEADER *prog, RS_STACKDATA *stack, int stack_num, RS_CALLDATA *call, int call_num) {
    (void) stack;
    (void) call;
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

// editloop3.cpp's static event interpreter, under the global name
// port/include/stubs/editloop3.hpp gives it.
extern CRunScript EdEventScript asm("EditLoop3_EdEventScript");

void RunEvent(CRunScript *script, int program, CDataAlloc2<1> *arena);

// Retail's body, with the return it leaves to v0: 1 for an event still running, 0 for one that
// ran to its end and -1 for a program the script does not have. EdEventInit treats the last two
// as an event with nothing to show.
int EdRunEvent(int program, CDataAlloc2<1> *arena) {
    RunEvent(&EdEventScript, program, arena);
    return g_run_result;
}

int CRunScript::run(int no) {
    if (prog == 0) {
        return g_run_result = -1;
    }

    sp = stack;
    call_sp = call;
    func = 0;

    char *base = reinterpret_cast<char *>(prog);

    if (no < 0) {
        func = HostFunc(code, base + prog->main);
    } else {
        RS_PROGDATA *entry = reinterpret_cast<RS_PROGDATA *>(base + prog->prog);

        for (int i = 0; i < prog->prog_num; i++, entry++) {
            if (entry->no == no) {
                func = HostFunc(code, base + entry->func);
                break;
            }
        }
    }

    if (func == 0) {
        printf("not found program %d\n", no);
        return g_run_result = -1;
    }

    frame = sp - func->arg;
    sp = frame + func->local;
    check_stack();
    memset(frame + func->arg, 0, (func->local - func->arg) * sizeof(RS_STACKDATA));
    vmcode_t *start = CodeAt(code, func->addr);
    end = 0;
    skip_wait = 0;
    exe(start);
    return g_run_result = end != 0 ? 0 : 1;
}

// Retail's interpreter, opcode for opcode; only CALL's record and the diagnostics' names differ.
void CRunScript::exe(vmcode_t *entry) {
    RS_STACKDATA value;
    RS_STACKDATA rhs;
    RS_STACKDATA lhs;
    float        rhs_f;
    float        lhs_f;
    int          rhs_i;
    int          lhs_i;

    pc = entry;

    for (;;) {
        switch (pc->op) {
            case RS_OP_LOAD:
                switch (pc->arg2) {
                    case RS_ADDR_LOCAL:
                        push(*(frame + pc->arg1));
                        break;
                    case RS_ADDR_LOCAL_INDEX:
                        push(*(frame + pc->arg1 + chk_int(pop(), func)));
                        break;
                    case RS_ADDR_POINTER_INDEX:
                        push(*(frame[pc->arg1].p + chk_int(pop(), func)));
                        break;
                    case RS_ADDR_LOCAL_FLOAT:
                        (frame + pc->arg1)->type = RS_FLOAT;
                        push(*(frame + pc->arg1));
                        break;
                    case RS_ADDR_LOCAL_INDEX_FLOAT:
                        rhs_i = chk_int(pop(), func);
                        (frame + pc->arg1 + rhs_i)->type = RS_FLOAT;
                        push(*(frame + pc->arg1 + rhs_i));
                        break;
                    case RS_ADDR_POINTER_INDEX_FLOAT:
                        rhs_i = chk_int(pop(), func);
                        (frame[pc->arg1].p + rhs_i)->type = RS_FLOAT;
                        push(*(frame[pc->arg1].p + rhs_i));
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
                    float constant;
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
                if (!skip_wait) {
                    if (is_true(pop())) {
                        if (pc->arg2) {
                            push_int(1);
                        }

                        pc = CodeAt(code, pc->arg1);
                        continue;
                    }
                }

                break;
            case RS_OP_JMP_FALSE:
                if (!skip_wait) {
                    if (!is_true(pop())) {
                        if (pc->arg2) {
                            push_int(0);
                        }

                        pc = CodeAt(code, pc->arg1);
                        continue;
                    }
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
                        fprintf(stderr, "RUNTIME ERROR at _CMP: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _ADD: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _SUB: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR _MUL: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _DIV: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _INVT: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _SIN: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _COS: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _ITOF: %s: operand is not number\n", FuncName(func));
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
                    fprintf(stderr, "RUNTIME ERROR at _FTOI: %s: operand is not number\n", FuncName(func));
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
                pc = 0;
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
                    pc = 0;
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
        }

        pc++;
    }
}
