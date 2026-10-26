#include "runscript.hpp"

#include <cstdio>
#include <cstring>
#include <print>

#include "battle_globals.hpp"

void runerror(const char *message) {
    std::println(stderr, "RUNTIME ERROR: {}", message);
    exit__2(-1);
}

void stkoverflow() {
    runerror("stack overflow");
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
