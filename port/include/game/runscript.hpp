#pragma once

#include "common.h"

/**
 * Opcodes of the script virtual machine, as vmcode_t::op holds them.
 */
// clang-format off
enum RS_OPCODE {
    RS_OP_LOAD       = 1,  /**< Push a variable. */
    RS_OP_LOAD_ADDR  = 2,  /**< Push a variable's address. */
    RS_OP_PUSH_CONST = 3,  /**< Push a constant. */
    RS_OP_POP        = 4,  /**< Pop. */
    RS_OP_STORE      = 5,  /**< Store. */
    RS_OP_ADD        = 6,  /**< Add. */
    RS_OP_SUB        = 7,  /**< Subtract. */
    RS_OP_MUL        = 8,  /**< Multiply. */
    RS_OP_DIV        = 9,  /**< Divide. */
    RS_OP_MOD        = 10, /**< Remainder. */
    RS_OP_NEG        = 11, /**< Negate. */
    RS_OP_ITOF       = 12, /**< Integer to float. */
    RS_OP_FTOI       = 13, /**< Float to integer. */
    RS_OP_CMP        = 14, /**< Compare. */
    RS_OP_RET        = 15, /**< Return. */
    RS_OP_JMP        = 16, /**< Jump. */
    RS_OP_JMP_FALSE  = 17, /**< Jump if false. */
    RS_OP_JMP_TRUE   = 18, /**< Jump if true. */
    RS_OP_CALL       = 19, /**< Call. */
    RS_OP_PRINT      = 20, /**< Print. */
    RS_OP_EXT        = 21, /**< Call an external function. */
    RS_OP_WAIT       = 23, /**< Wait. */
    RS_OP_AND        = 24, /**< Logical and. */
    RS_OP_OR         = 25, /**< Logical or. */
    RS_OP_NOT        = 26, /**< Logical not. */
    RS_OP_END        = 27, /**< End. */
    RS_OP_SKIP_END   = 28, /**< Clear the skip wait. */
    RS_OP_SIN        = 29, /**< Sine. */
    RS_OP_COS        = 30, /**< Cosine. */
};

// clang-format on

/**
 * How a script load addresses its variable, as vmcode_t::arg2 holds it.
 */
// clang-format off
enum RS_ADDR_MODE {
    RS_ADDR_LOCAL               = 1,  /**< Local slot. */
    RS_ADDR_LOCAL_INDEX         = 2,  /**< Indexed local slot. */
    RS_ADDR_POINTER_INDEX       = 4,  /**< Indexed through a pointer. */
    RS_ADDR_LOCAL_FLOAT         = 8,  /**< Local slot, as a float. */
    RS_ADDR_LOCAL_INDEX_FLOAT   = 16, /**< Indexed local slot, as a float. */
    RS_ADDR_POINTER_INDEX_FLOAT = 32, /**< Indexed through a pointer, as a float. */
};

// clang-format on

/**
 * Comparisons of the script compare opcode, as vmcode_t::arg1 holds them.
 */
// clang-format off
enum RS_COMPARE {
    RS_CMP_EQ = 40, /**< Equal. */
    RS_CMP_NE = 41, /**< Not equal. */
    RS_CMP_LT = 42, /**< Less than. */
    RS_CMP_LE = 43, /**< Less than or equal. */
    RS_CMP_GT = 44, /**< Greater than. */
    RS_CMP_GE = 45, /**< Greater than or equal. */
};

// clang-format on

/**
 * A tagged value stored on the script interpreter's operand stack.
 */
struct RS_STACKDATA {
    int type; /**< Identifies the active value representation. */

    union {
        int           i; /**< Value of an integer. */
        float         f; /**< Value of a float. */
        char         *s; /**< Value of a string. */
        RS_STACKDATA *p; /**< Stack slot a reference points at. */
    };
};

enum {
    RS_INT = 0,
    RS_FLOAT = 1,
    RS_STR = 2,
    RS_PTR = 3
};

/**
 * A single virtual-machine instruction and its two opcode-specific operands.
 */
struct vmcode_t {
    int op;   /**< Selects the operation to execute. @see RS_OPCODE. */
    int arg1; /**< Supplies the operation's first operand. */
    int arg2; /**< Supplies the operation's second operand. */
};

/**
 * Describes a script function's code position and stack-frame requirements.
 */
struct funcdata {
    int   addr;  /**< Stores the function's offset in the code block. */
    char *name;  /**< Names the function for runtime diagnostics. */
    int   local; /**< Gives the total number of frame slots. */
    int   arg;   /**< Gives the number of argument slots. */
};

/**
 * Preserves interpreter state for a suspended script call.
 */
struct RS_CALLDATA {
    vmcode_t     *ret;   /**< Points to the caller's return instruction. */
    RS_STACKDATA *frame; /**< Points to the caller's stack frame. */
    funcdata     *func;  /**< Describes the caller's function. */
};

/**
 * Maps an external program number to its function description.
 */
struct RS_PROGDATA {
    int no;   /**< Identifies the externally selectable program. */
    int func; /**< Stores the function-description offset. */
};

/**
 * Describes the offset-based sections of a compiled script program.
 */
struct RS_PROG_HEADER {
    int unk_0;
    int main;     /**< Stores the main function-description offset. */
    int code;     /**< Stores the bytecode-section offset. */
    int prog;     /**< Stores the program-table offset. */
    int prog_num; /**< Gives the number of program-table entries. */
};

/**
 * Runs one compiled script on a stack machine, suspending at its waits.
 */
class CRunScript {
public:
    int ext_func_num;                            /**< Gives the number of registered external functions. */
    int (**ext_func_table)(RS_STACKDATA *, int); /**< Points to the external function table. */
    int             stack_num;                   /**< Gives the operand stack capacity. */
    RS_STACKDATA   *stack;                       /**< Points to the operand stack storage. */
    RS_STACKDATA   *sp;                          /**< Points to the next operand stack slot. */
    RS_STACKDATA   *stack_end;                   /**< Points one past the operand stack storage. */
    int             call_num;                    /**< Gives the call stack capacity. */
    RS_CALLDATA    *call;                        /**< Points to the call stack storage. */
    RS_CALLDATA    *call_sp;                     /**< Points to the next call stack entry. */
    RS_CALLDATA    *call_end;                    /**< Points one past the call stack storage. */
    RS_STACKDATA   *frame;                       /**< Points to the active operand frame. */
    funcdata       *func;                        /**< Describes the active script function. */
    vmcode_t       *pc;                          /**< Points to the next instruction. */
    int             end;                         /**< Records whether execution has completed. */
    int             skip_wait;                   /**< Records whether wait operations should be skipped. */
    RS_PROG_HEADER *prog;                        /**< Points to the loaded program header. */
    char           *code;                        /**< Points to the loaded bytecode section. */
    int             result;                      /**< Stores the script's return value. */

    /**
     * Nonzero once the script has run to its end.
     */
    int IsEnd() { return end; }

    /**
     * Makes an interpreter with no program, stack or external functions.
     *
     * @mangled __ct__10CRunScriptFv
     * @address 0x23D940
     * @size 0x40
     */
    CRunScript();

    /**
     * Stops the game when the operand stack is full.
     *
     * @mangled check_stack__10CRunScriptFv
     * @address 0x23D980
     * @size 0x34
     * @unknownret
     */
    void check_stack();

    /**
     * Pushes one value onto the operand stack.
     *
     * @mangled push__10CRunScriptF12RS_STACKDATA
     * @address 0x23D9C0
     * @size 0x50
     * @unknownret
     */
    void push(RS_STACKDATA value);

    /**
     * Pushes an integer onto the operand stack.
     *
     * @mangled push_int__10CRunScriptFi
     * @address 0x23DA10
     * @size 0x50
     * @unknownret
     */
    void push_int(int value);

    /**
     * Pushes a string onto the operand stack.
     *
     * @mangled push_str__10CRunScriptFPc
     * @address 0x23DA60
     * @size 0x54
     * @unknownret
     */
    void push_str(char *string);

    /**
     * Pushes a reference to a stack slot onto the operand stack.
     *
     * @mangled push_ptr__10CRunScriptFP12RS_STACKDATA
     * @address 0x23DAC0
     * @size 0x54
     * @unknownret
     */
    void push_ptr(RS_STACKDATA *pointer);

    /**
     * Pushes a float onto the operand stack.
     *
     * @mangled push_float__10CRunScriptFf
     * @address 0x23DB20
     * @size 0x54
     * @unknownret
     */
    void push_float(float value);

    /**
     * Pops the top value off the operand stack.
     *
     * @mangled pop__10CRunScriptFv
     * @address 0x23DB80
     * @size 0x24
     * @unknownret
     */
    RS_STACKDATA pop();

    /**
     * Enters a script function, and gives back its first instruction.
     *
     * @mangled call_func__10CRunScriptFP8funcdataP8vmcode_t
     * @address 0x23DBB0
     * @size 0x108
     * @unknownret
     */
    vmcode_t *call_func(funcdata *callee, vmcode_t *return_pc);

    /**
     * Leaves the current script function, and gives back the caller's instruction.
     *
     * @mangled ret_func__10CRunScriptFv
     * @address 0x23DCC0
     * @size 0x34
     * @unknownret
     */
    vmcode_t *ret_func();

    /**
     * Calls the external function the first argument names.
     *
     * @mangled ext__10CRunScriptFP12RS_STACKDATAi
     * @address 0x23DD00
     * @size 0xB4
     * @unknownret
     */
    void ext(RS_STACKDATA *args, int argc);

    /**
     * Gives the interpreter a program and the stacks to run it on.
     *
     * @mangled load__10CRunScriptFP14RS_PROG_HEADERP12RS_STACKDATAiP11RS_CALLDATAi
     * @address 0x23DDC0
     * @size 0x50
     * @unknownret
     */
    void load(RS_PROG_HEADER *prog, RS_STACKDATA *stack, int stack_num, RS_CALLDATA *call, int call_num);

    /**
     * Swaps in another program, keeping the stacks.
     *
     * @mangled reload__10CRunScriptFP14RS_PROG_HEADER
     * @address 0x23DE10
     * @size 0x18
     * @unknownret
     */
    void reload(RS_PROG_HEADER *prog);

    /**
     * Registers the table of external functions scripts can call.
     *
     * @mangled ext_func__10CRunScriptFPPFP12RS_STACKDATAi_ii
     * @address 0x23DE30
     * @size 0x10
     * @unknownret
     */
    void ext_func(int (**table)(RS_STACKDATA *, int), int count);

    /**
     * Continues a suspended script where it stopped.
     *
     * @mangled resume__10CRunScriptFv
     * @address 0x23DE40
     * @size 0x2C
     * @unknownret
     */
    void resume();

    /**
     * Starts a numbered program, or the main function for a negative number.
     *
     * @mangled run__10CRunScriptFi
     * @address 0x23DE70
     * @size 0x180
     * @unknownret
     */
    int run(int no);

    /**
     * Reports whether the program has a numbered entry.
     *
     * @mangled check_program__10CRunScriptFi
     * @address 0x23DFF0
     * @size 0x54
     * @unknownret
     */
    int check_program(int no);

    /**
     * Resumes the script, running through its waits.
     *
     * @mangled skip__10CRunScriptFv
     * @address 0x23E050
     * @size 0x28
     * @unknownret
     */
    void skip();

    /**
     * Runs instructions from one until the script ends or waits.
     *
     * @mangled exe__10CRunScriptFP8vmcode_t
     * @address 0x23E080
     * @size 0x1508
     * @unknownret
     */
    void exe(vmcode_t *entry);
};
