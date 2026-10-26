#include "scriptinterpreter.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int CScriptInterpreter::GetNextTAG() {
    if (tag_table == NULL) {
        return -1;
    }

    do {
        if (!ControlCode()) {
            return -1;
        }

        if (!SearchCommand(&current_tag)) {
            return -1;
        }
    } while (current_tag >= tag_count || current_tag < 0);

    if (tag_table[current_tag].argument_types[0] >= 0) {
        int status = GetArg(tag_table[current_tag].argument_types);

        if (status == 0) {
            return -1;
        }

        if (status < 0) {
            printf("error at ");
            printf("%s\n", tag_table[current_tag].name);

            for (;;) {
            }
        }
    }

    return current_tag;
}

void CScriptInterpreter::SetTAG(TAG_PARAM *tags, int count) {
    tag_table = tags;
    tag_count = count;
}

void CScriptInterpreter::SetFunction(SPI_FUNC_PARAM *functions, int count) {
    function_table = functions;
    function_count = count;
}

void CScriptInterpreter::SetScript(char *script, int script_size) {
    data = script;
    size = script_size;
    pos = 0;
    argument_data_used = 0;
    current_tag = -1;
    PreProcess__FR9input_str__2(*this);
}

CScriptInterpreter::CScriptInterpreter() {
    data = NULL;
    data = NULL;
    size = 0;
    pos = 0;
    argument_data_used = 0;
    current_tag = -1;
    tag_table = NULL;
    function_table = NULL;
}

int CScriptInterpreter::ControlCode() {
    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    int control_type = 1;

    if (memcmp(data + pos, "if", 2) == 0) {
        control_type = 2;
        printf("if\n");
    }

    if (control_type == 1) {
        return 1;
    }

    int c;

    do {
        if (get(&c) == 0) {
            return 0;
        }
    } while (c != '(');

    int condition;

    if (!CallFunction(&condition)) {
        return 0;
    }

    if (condition == 0) {
        do {
            if (get(&c) == 0) {
                return 0;
            }
        } while (c != '}');
    }

    return control_type;
}

int CScriptInterpreter::CallFunction(int *result) {
    char  call_text[512];
    void *call_arguments[24];

    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    int length = 0;
    int argc = 0;
    int c;

    while (1) {
        if (get(&c) == 0) {
            return 0;
        }

        if (c == ')') {
            break;
        }

        if (c == '(' || c == ',') {
            call_text[length++] = 0;
            argc++;
        } else if (CheckChar__Fc__3(c)) {
            call_text[length++] = c;
        }
    }

    call_text[length] = 0;
    call_text[length + 1] = 0;

    if (function_table == NULL) {
        return -1;
    }

    SPI_FUNC_PARAM *command = NULL;

    for (length = 0; length < function_count; length++) {
        if (strcmp(function_table[length].name, call_text) == 0) {
            command = &function_table[length];
            break;
        }
    }

    if (command == NULL) {
        return -1;
    }

    for (length = 0; command->argument_types[length] >= 0; length++) {
    }

    if (length != argc || argc >= 24) {
        return -1;
    }

    int   used = 0;
    u8   *value_out = &function_argument_data[argument_data_used];
    char *word = call_text;

    for (length = 0; length < argc; length++) {
        int value_size = 0;

        while (*word++ != 0) {
        }

        call_arguments[length] = value_out;

        switch (command->argument_types[length]) {
            case SCRIPT_ARGUMENT_STRING:
                strcpy((char *) value_out, word);
                value_size = strlen(word);
                value_size = ((value_size >> 2) + 1) << 2;
                break;
            case SCRIPT_ARGUMENT_INTEGER:
                *(int *) value_out = atoi(word);
                value_size = 4;
                break;
            case SCRIPT_ARGUMENT_FLOAT:
                *(float *) value_out = atof(word);
                value_size = 4;
                break;
        }

        value_out += value_size;
        used += value_size;
    }

    *result = command->function(call_arguments);
    return 1;
}

int CScriptInterpreter::GetArg(int *argument_types) {
    char token[256];

    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    int argc;

    for (argc = 0; argument_types[argc] >= 0; argc++) {
    }

    int c;
    int i;
    int used = 0;

    for (i = 0; i < argc; i++) {
        arguments[i] = &argument_text[used];
        int length = 0;

        if (!SkipSpace__FR9input_str__3(*this)) {
            return 0;
        }

        if (get(&c) == 0) {
            return 0;
        }

        if (c != ',') {
            token[length++] = c;
        } else if (!SkipSpace__FR9input_str__3(*this)) {
            return 0;
        }

        while (1) {
            if (get(&c) == 0) {
                return 0;
            }

            if (c == ',' || !CheckChar__Fc__3(c)) {
                break;
            }

            token[length++] = c;
        }

        token[length] = 0;

        switch (argument_types[i]) {
            case SCRIPT_ARGUMENT_STRING:
                if (token[0] != '"') {
                    return -1;
                }

                for (length = 1;; length++) {
                    if (token[length] == '"') {
                        token[length] = 0;
                        break;
                    }

                    if (token[length] == 0) {
                        return -1;
                    }
                }

                strcpy((char *) arguments[i], token + 1);
                used += ((((int) strlen((char *) arguments[i]) + 1) >> 2) + 1) << 2;
                break;
            case SCRIPT_ARGUMENT_INTEGER:
                for (length = 0; token[length] != 0; length++) {
                    char digit = token[length];

                    if ((digit < '0' || digit > '9') && digit != '-') {
                        return -1;
                    }
                }

                *(int *) arguments[i] = atoi(token);
                used += 4;
                break;
            case SCRIPT_ARGUMENT_FLOAT:
                for (length = 0; token[length] != 0; length++) {
                    char digit = token[length];

                    if ((digit < '0' || digit > '9') && digit != '.' && digit != '-') {
                        return -1;
                    }
                }

                *(float *) arguments[i] = atof(token);
                used += 4;
                break;
            default:
                return -1;
        }
    }

    return 1;
}

int CScriptInterpreter::SearchCommand(int *tag_index) {
    char command[256];

    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    int length = 0;
    int c;

    while (1) {
        if (get(&c) == 0) {
            return 0;
        }

        if (!CheckChar__Fc__3(c)) {
            break;
        }

        command[length++] = c;
    }

    command[length] = 0;

    if (command[0] < 'A' || command[0] > 'Z') {
        *tag_index = -1;
        return 1;
    }

    for (int i = 0; i < tag_count; i++) {
        if (strcmp(tag_table[i].name, command) == 0) {
            *tag_index = i;
            return 1;
        }
    }

    if (strcmp(command, "end") == 0) {
        return 0;
    }

    *tag_index = -1;
    return 1;
}
