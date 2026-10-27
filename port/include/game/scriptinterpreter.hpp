#pragma once

#include "common.h"

#include "sysmes.hpp"

namespace dc::game {

/** Types used to decode values following a script tag or command. */
enum SCRIPT_ARGUMENT_TYPE {
    SCRIPT_ARGUMENT_STRING = 0,  /**< A quoted text argument. */
    SCRIPT_ARGUMENT_INTEGER = 1, /**< A signed decimal integer argument. */
    SCRIPT_ARGUMENT_FLOAT = 2,   /**< A signed decimal floating-point argument. */
};

/** Script command callback. */
using SPI_FUNCTION = int (*)(void **arguments);

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
     */
    int GetNextTAG();

    /**
     * Gives the interpreter the table of tags it recognises.
     */
    void SetTAG(TAG_PARAM *tags, int count);

    /**
     * Gives the interpreter the table of commands it may call.
     */
    void SetFunction(SPI_FUNC_PARAM *functions, int count);

    /**
     * Points the interpreter at the script text it is to read.
     */
    void SetScript(char *script, int script_size);

    /**
     * Constructs an interpreter with no script, tags or commands.
     */
    CScriptInterpreter();

    /**
     * Reads one control code out of the script.
     */
    int ControlCode();

    /**
     * Calls the command the script names, having gathered its arguments.
     */
    int CallFunction(int *result);

    /**
     * Reads one command argument out of the script.
     */
    int GetArg(int *argument_types);

    /**
     * Finds the command table entry the script's next word names.
     */
    int SearchCommand(int *tag_index);
};

STATIC_ASSERT(sizeof(CScriptInterpreter) == 0x890);

} // namespace dc::game
