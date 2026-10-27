#include "scriptinterpreter.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <print>
#include <string>
#include <string_view>

namespace dc::game {

namespace {

// Script text may contain NUL bytes; a token compares and converts only up to the first one.
std::string_view CStringView(const std::string &text) {
    return std::string_view(text).substr(0, text.find('\0'));
}

int ArgumentCount(const int *argument_types) {
    int count = 0;

    while (argument_types[count] >= 0) {
        count++;
    }

    return count;
}

template <typename T>
void StoreValue(void *destination, T value) {
    std::memcpy(destination, &value, sizeof(value));
}

void StoreString(char *destination, std::string_view text) {
    std::ranges::copy(text, destination);
    destination[text.size()] = '\0';
}

} // namespace

int CScriptInterpreter::GetNextTAG() {
    if (tag_table == nullptr) {
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
        const int status = GetArg(tag_table[current_tag].argument_types);

        if (status == 0) {
            return -1;
        }

        if (status < 0) {
            std::print("error at ");
            std::println("{}", tag_table[current_tag].name);

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

CScriptInterpreter::CScriptInterpreter() : current_tag(-1), argument_data_used(0), tag_table(nullptr), function_table(nullptr) {
    data = nullptr;
    size = 0;
    pos = 0;
}

int CScriptInterpreter::ControlCode() {
    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    if (std::string_view(&data[pos], 2) != "if") {
        return 1;
    }

    std::println("if");

    int c = 0;

    do {
        if (get(&c) == 0) {
            return 0;
        }
    } while (c != '(');

    int condition = 0;

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

    return 2;
}

int CScriptInterpreter::CallFunction(int *result) {
    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    // The command name followed by its argument words, each ended by a NUL.
    std::string call_text;
    int         argc = 0;
    int         c = 0;

    for (;;) {
        if (get(&c) == 0) {
            return 0;
        }

        if (c == ')') {
            break;
        }

        if (c == '(' || c == ',') {
            call_text += '\0';
            argc++;
        } else if (CheckChar__Fc__3(static_cast<char>(c))) {
            call_text += static_cast<char>(c);
        }
    }

    call_text.append(2, '\0');

    if (function_table == nullptr) {
        return -1;
    }

    const std::string_view name = CStringView(call_text);
    SPI_FUNC_PARAM        *command = nullptr;

    for (int index = 0; index < function_count; index++) {
        if (std::string_view(function_table[index].name) == name) {
            command = &function_table[index];
            break;
        }
    }

    if (command == nullptr) {
        return -1;
    }

    if (ArgumentCount(command->argument_types) != argc || argc >= 24) {
        return -1;
    }

    std::array<void *, 24> call_arguments{};
    std::size_t            word_start = 0;
    int                    offset = argument_data_used;

    for (int index = 0; index < argc; index++) {
        word_start = call_text.find('\0', word_start) + 1;

        const char *word = &call_text[word_start];
        u8         *value_out = &function_argument_data[offset];
        int         value_size = 0;

        call_arguments[index] = value_out;

        switch (command->argument_types[index]) {
            case SCRIPT_ARGUMENT_STRING: {
                const std::string_view text = word;
                StoreString(reinterpret_cast<char *>(value_out), text);
                value_size = ((static_cast<int>(text.size()) >> 2) + 1) << 2;
                break;
            }
            case SCRIPT_ARGUMENT_INTEGER:
                StoreValue(value_out, std::atoi(word));
                value_size = 4;
                break;
            case SCRIPT_ARGUMENT_FLOAT:
                StoreValue(value_out, static_cast<float>(std::atof(word)));
                value_size = 4;
                break;
            default:
                break;
        }

        offset += value_size;
    }

    *result = command->function(call_arguments.data());
    return 1;
}

int CScriptInterpreter::GetArg(int *argument_types) {
    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    const int argc = ArgumentCount(argument_types);
    int       c = 0;
    int       used = 0;

    for (int index = 0; index < argc; index++) {
        char *destination = &argument_text[used];
        arguments[index] = destination;

        if (!SkipSpace__FR9input_str__3(*this)) {
            return 0;
        }

        if (get(&c) == 0) {
            return 0;
        }

        std::string token;

        if (c != ',') {
            token += static_cast<char>(c);
        } else if (!SkipSpace__FR9input_str__3(*this)) {
            return 0;
        }

        for (;;) {
            if (get(&c) == 0) {
                return 0;
            }

            if (c == ',' || !CheckChar__Fc__3(static_cast<char>(c))) {
                break;
            }

            token += static_cast<char>(c);
        }

        const std::string_view text = CStringView(token);

        switch (argument_types[index]) {
            case SCRIPT_ARGUMENT_STRING: {
                if (text.empty() || text[0] != '"') {
                    return -1;
                }

                const std::size_t closing_quote = text.find('"', 1);

                if (closing_quote == std::string_view::npos) {
                    return -1;
                }

                const std::string_view contents = text.substr(1, closing_quote - 1);
                StoreString(destination, contents);
                used += (((static_cast<int>(contents.size()) + 1) >> 2) + 1) << 2;
                break;
            }
            case SCRIPT_ARGUMENT_INTEGER:
                if (!std::ranges::all_of(text, [](char digit) { return (digit >= '0' && digit <= '9') || digit == '-'; })) {
                    return -1;
                }

                StoreValue(destination, std::atoi(token.c_str()));
                used += 4;
                break;
            case SCRIPT_ARGUMENT_FLOAT:
                if (!std::ranges::all_of(text, [](char digit) {
                        return (digit >= '0' && digit <= '9') || digit == '.' || digit == '-';
                    })) {
                    return -1;
                }

                StoreValue(destination, static_cast<float>(std::atof(token.c_str())));
                used += 4;
                break;
            default:
                return -1;
        }
    }

    return 1;
}

int CScriptInterpreter::SearchCommand(int *tag_index) {
    if (!SkipSpace__FR9input_str__3(*this)) {
        return 0;
    }

    std::string word;
    int         c = 0;

    for (;;) {
        if (get(&c) == 0) {
            return 0;
        }

        if (!CheckChar__Fc__3(static_cast<char>(c))) {
            break;
        }

        word += static_cast<char>(c);
    }

    const std::string_view command = CStringView(word);

    if (command.empty() || command[0] < 'A' || command[0] > 'Z') {
        *tag_index = -1;
        return 1;
    }

    for (int index = 0; index < tag_count; index++) {
        if (std::string_view(tag_table[index].name) == command) {
            *tag_index = index;
            return 1;
        }
    }

    if (command == "end") {
        return 0;
    }

    *tag_index = -1;
    return 1;
}

} // namespace dc::game
