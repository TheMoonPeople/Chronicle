#pragma once

#include "common.h"

#include "sysmes.hpp"

/** Types used to decode values following a script tag or command. */
enum SCRIPT_ARGUMENT_TYPE {
    SCRIPT_ARGUMENT_STRING = 0,  /**< A quoted text argument. */
    SCRIPT_ARGUMENT_INTEGER = 1, /**< A signed decimal integer argument. */
    SCRIPT_ARGUMENT_FLOAT = 2,   /**< A signed decimal floating-point argument. */
};

/** Script command callback. */
typedef int (*SPI_FUNCTION)(void **arguments);

/**
 *
 * Describes one callable script command and its argument types.
 *
 */
struct SPI_FUNC_PARAM {
    char        *name;               /**< Text name used to find the command. */
    int          argument_types[24]; /**< Argument kinds, terminated by a negative value. */
    SPI_FUNCTION function;           /**< Routine invoked after its arguments are decoded. */
};

STATIC_ASSERT(sizeof(SPI_FUNC_PARAM) == 0x68);

/**
 *
 * Describes one script tag and the types of arguments that follow it.
 *
 */
struct TAG_PARAM {
    char *name;               /**< Text name used to find the tag. */
    int   argument_types[24]; /**< Argument kinds, terminated by a negative value. */
};

STATIC_ASSERT(sizeof(TAG_PARAM) == 0x64);

/**
 *
 * Parses tagged text scripts and dispatches their embedded commands.
 *
 */
class CScriptInterpreter : public input_str {
public:
    int             current_tag;                   /**< Index of the tag most recently read, or -1. */
    int             argument_data_used;            /**< Offset of command-call values in the work buffer. */
    int             tag_count;                     /**< Number of entries in the tag table. */
    u8              function_argument_data[0x400]; /**< Storage used to marshal command-call arguments. */
    TAG_PARAM      *tag_table;                     /**< Tags recognised by the interpreter. */
    SPI_FUNC_PARAM *function_table;                /**< Commands callable from control codes. */
    int             function_count;                /**< Number of entries in the command table. */
    void           *arguments[24];                 /**< Arguments decoded for the current command. */
    char            argument_text[0x40C];          /**< Inline storage for decoded string arguments. */

    /**
     * Reads the next tag out of the script and gives its number.
     *
     * @mangled GetNextTAG__18CScriptInterpreterFv
     * @address 0x15F100
     * @size 0x144
     */
    int GetNextTAG();

    /**
     * Gives the interpreter the table of tags it recognises.
     *
     * @mangled SetTAG__18CScriptInterpreterFP9TAG_PARAMi
     * @address 0x15F250
     * @size 0x10
     */
    void SetTAG(TAG_PARAM *tags, int count);

    /**
     * Gives the interpreter the table of commands it may call.
     *
     * @mangled SetFunction__18CScriptInterpreterFP14SPI_FUNC_PARAMi
     * @address 0x15F260
     * @size 0x10
     */
    void SetFunction(SPI_FUNC_PARAM *functions, int count);

    /**
     * Points the interpreter at the script text it is to read.
     *
     * @mangled SetScript__18CScriptInterpreterFPci
     * @address 0x15F270
     * @size 0x38
     */
    void SetScript(char *script, int script_size);

    /**
     * Constructs an interpreter with no script, tags or commands.
     *
     * @mangled __ct__18CScriptInterpreterFv
     * @address 0x15F2B0
     * @size 0x38
     */
    CScriptInterpreter();

    /**
     * Reads one control code out of the script.
     *
     * @mangled ControlCode__18CScriptInterpreterFv
     * @address 0x15F2F0
     * @size 0x178
     */
    int ControlCode();

    /**
     * Calls the command the script names, having gathered its arguments.
     *
     * @mangled CallFunction__18CScriptInterpreterFPi
     * @address 0x15F470
     * @size 0x320
     */
    int CallFunction(int *result);

    /**
     * Reads one command argument out of the script.
     *
     * @mangled GetArg__18CScriptInterpreterFPi
     * @address 0x15F790
     * @size 0x3F0
     */
    int GetArg(int *argument_types);

    /**
     * Finds the command table entry the script's next word names.
     *
     * @mangled SearchCommand__18CScriptInterpreterFPi
     * @address 0x15FB80
     * @size 0x1A4
     */
    int SearchCommand(int *tag_index);
};

STATIC_ASSERT(sizeof(CScriptInterpreter) == 0x890);
