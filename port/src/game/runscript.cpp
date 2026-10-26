#include "runscript.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battle_globals.hpp"

void runerror(const char *message) {
    fprintf(stderr, "RUNTIME ERROR: %s\n", message);
    exit__2(-1);
}

void stkoverflow() {
    runerror("stack overflow");
}

int chk_int(RS_STACKDATA data, funcdata *function);

int is_true(RS_STACKDATA data) {
    return !(data.type == RS_INT && data.i == 0);
}

void divby0error() {
    runerror("Divide by 0");
}

void modby0error() {
    runerror("Modulo by 0");
}

void print(RS_STACKDATA *data, int count) {
    for (int index = 0; index < count; index++, data++) {
        if (data->type == RS_INT) {
            printf("%d", data->i);
        } else if (data->type == RS_STR) {
            printf("%s", data->s);
        } else if (data->type == RS_FLOAT) {
            printf("%f", data->f);
        }

        fflush(stdout);
    }
}

CRunScript::CRunScript() {
    sp = stack;
    stack_end = stack;
    call_sp = call;
    call_end = call;
    pc = 0;
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

/* A call takes its arguments from the operand stack in place: the new frame starts where the
   caller left them, so nothing is copied and only the locals past them have to be cleared. */
vmcode_t *CRunScript::call_func(funcdata *callee, vmcode_t *return_pc) {
    if (call_sp >= call_end) {
        printf("\202\261\202\352\210\310\217\343\212\326\220\224\214\304\202\321\217\157\202\265"
               "\202\252\202\305\202\253\202\334\202\271\202\361\201\102\n");
        exit__2(-2);
    }

    call_sp->ret = return_pc;
    call_sp->func = func;
    call_sp->frame = frame;
    frame = sp - callee->arg;
    sp = frame + callee->local;
    func = callee;
    call_sp++;
    memset(frame + func->arg, 0, (func->local - func->arg) * sizeof(RS_STACKDATA));
    check_stack();
    return (vmcode_t *) (code + (int) callee->addr);
}

vmcode_t *CRunScript::ret_func() {
    call_sp--;
    frame = call_sp->frame;
    func = call_sp->func;
    return call_sp->ret;
}

void CRunScript::ext(RS_STACKDATA *args, int argc) {
    if (args->i < 0 || args->i >= ext_func_num) {
        printf("not found ext %d\n", args->i);
    } else if (ext_func_table[args->i] == 0) {
        printf("not found ext %d\n", args->i);
    } else if ((*ext_func_table[args->i])(args + 1, argc - 1) == 0) {
        printf("illegal function call ext %d\n", args->i);
    }
}

void CRunScript::reload(RS_PROG_HEADER *prog) {
    this->prog = prog;
    code = (char *) prog + prog->code;
}

void CRunScript::ext_func(int (**table)(RS_STACKDATA *, int), int count) {
    ext_func_table = table;
    ext_func_num = count;
}

void CRunScript::resume() {
    if (pc) {
        exe(pc);
    }
}

int CRunScript::check_program(int no) {
    RS_PROGDATA    *entry;
    int             i;
    RS_PROG_HEADER *header;

    header = prog;
    entry = (RS_PROGDATA *) ((char *) header + header->prog);

    for (i = 0; i < header->prog_num; i++, entry++) {
        if (entry->no == no) {
            return 1;
        }
    }

    return 0;
}

void CRunScript::skip() {
    skip_wait = 1;
    resume();
}

/* The loop runs on pc rather than on the argument, because three of the opcodes leave the
   machine suspended and the only thing that makes a later resume possible is the program counter
   being in the object when they return. Every case that does not redirect control falls out of
   the switch to the step at the bottom, so an opcode that jumps writes pc and continues. */

