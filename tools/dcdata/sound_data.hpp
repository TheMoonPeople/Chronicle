#pragma once

// The parts of Dark Cloud's sound data that only this game has, read as the game reads them: a sound
// pack's load lists and their .cfg commands, and the global volume tables. Kazarov converts the banks
// and sequences themselves. Header-only and free of game headers, like dcdata.hpp, so the port's
// tests can use it.

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "dcdata.hpp"

namespace dcdata::sound {

enum class LoadKind {
    Bank,
    Sequence
};

struct LoadEntry {
    std::string member; // the name the game asks the pack for: "e01a_c.hd"
    char        slot;   // the name's sixth letter, which picks the driver slot
    LoadKind    kind;
};

// A pack's .txt as CSound::LoadSoundFileFromPack reads it: a line of nine characters or more names a
// member by its first nine, a bank (".hd") or a sequence (".sq"); other lines name nothing.
inline std::vector<LoadEntry> ReadLoadList(std::string_view text) {
    text = text.substr(0, text.find('\0'));
    std::vector<LoadEntry> entries;
    for (std::size_t start = 0; start < text.size();) {
        const std::size_t      end = std::min(text.find('\n', start), text.size());
        const std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (line.size() < 9) {
            continue;
        }
        const std::string_view member = line.substr(0, 9);
        if (member.ends_with(".hd")) {
            entries.push_back({std::string(member), member[5], LoadKind::Bank});
        } else if (member.ends_with(".sq")) {
            entries.push_back({std::string(member), member[5], LoadKind::Sequence});
        }
    }
    return entries;
}

// What a .cfg sets, as LoadSoundInfo leaves it: zero where a command is missing.
struct PackConfig {
    int reverb_mode = 0;
    int reverb_depth = 0;
    int se_table = 0;
    int se_table_type = 0;
};

// The REVERBE and TABLE commands of a .cfg, each followed by two integers.
inline PackConfig ReadPackConfig(std::string_view text) {
    text = text.substr(0, text.find('\0'));
    PackConfig  config;
    std::size_t at = 0;
    const auto  number = [&] {
        at = std::min(text.find_first_of("-0123456789", at), text.size());
        int        value = 0;
        const auto first = text.data() + at;
        const auto [last, error] = std::from_chars(first, text.data() + text.size(), value);
        at = last == first ? at + 1 : static_cast<std::size_t>(last - text.data());
        return error == std::errc{} ? value : 0;
    };
    while (at < text.size()) {
        if (!std::isalpha(static_cast<unsigned char>(text[at]))) {
            at++;
            continue;
        }
        const std::size_t begin = at;
        while (at < text.size() && std::isalnum(static_cast<unsigned char>(text[at]))) {
            at++;
        }
        const std::string_view word = text.substr(begin, at - begin);
        if (word == "REVERBE") {
            config.reverb_mode = number();
            config.reverb_depth = number();
        } else if (word == "TABLE") {
            config.se_table = number();
            config.se_table_type = number();
        }
    }
    return config;
}

// A global table's values as CSound's table readers take them: whitespace, commas and semicolons
// separate them, and the first line is a header.
inline std::vector<std::string_view> ReadTableTokens(std::string_view text) {
    text = text.substr(0, text.find('\0'));
    constexpr std::string_view    kDelimiters = " ;,\t\n\r";
    std::vector<std::string_view> tokens;
    std::size_t                   at = text.find_first_of("\n\r");
    while ((at = text.find_first_not_of(kDelimiters, at)) != std::string_view::npos) {
        const std::size_t end = std::min(text.find_first_of(kDelimiters, at), text.size());
        tokens.push_back(text.substr(at, end - at));
        at = end;
    }
    return tokens;
}

// The integer a table token starts with, or zero, as atoi reads it.
inline int TokenNumber(std::string_view token) {
    int value = 0;
    std::from_chars(token.data(), token.data() + token.size(), value);
    return value;
}

struct SequenceVolume {
    std::string name; // up to nine characters, as MIDI_SEQUENCE keeps it
    int         volume;
};

// sound/tbl/sqtbl.txt as CSound::LoadSqInf reads it: name and volume pairs, at most 400.
inline std::vector<SequenceVolume> ReadSequenceVolumes(std::string_view text) {
    const auto                  tokens = ReadTableTokens(text);
    std::vector<SequenceVolume> rows;
    for (std::size_t i = 0; i + 1 < tokens.size() && rows.size() < 400; i += 2) {
        rows.push_back({std::string(tokens[i].substr(0, 9)), TokenNumber(tokens[i + 1])});
    }
    return rows;
}

struct EffectVolume {
    int bank;
    int program;
    int volume;
};

// sound/tbl/setbl.txt as CSound::LoadSeInf reads it: bank, program and volume, each the number in its
// first four characters, at most 3,000 rows.
inline std::vector<EffectVolume> ReadEffectVolumes(std::string_view text) {
    const auto                tokens = ReadTableTokens(text);
    const auto                number = [](std::string_view token) { return TokenNumber(token.substr(0, 4)); };
    std::vector<EffectVolume> rows;
    for (std::size_t i = 0; i + 2 < tokens.size() && rows.size() < 3000; i += 3) {
        rows.push_back({number(tokens[i]), number(tokens[i + 1]), number(tokens[i + 2])});
    }
    return rows;
}

// Where a bank is published: once per distinct HD and BD pair, named for the first member that holds
// it and its contents' hash, since the same bank sits in many packs.
inline std::string SharedBankKey(std::string_view member, std::span<const unsigned char> hd,
                                 std::span<const unsigned char> bd) {
    std::vector<unsigned char> both(hd.begin(), hd.end());
    both.insert(both.end(), bd.begin(), bd.end());
    std::string stem;
    for (const char c : member.substr(0, member.rfind('.'))) {
        const auto byte = static_cast<unsigned char>(c);
        stem += std::isalnum(byte) || c == '_' ? static_cast<char>(std::tolower(byte)) : '_';
    }
    return std::format("sound/banks/{}-{:016x}.hd", stem.empty() ? "unnamed" : stem, Fnv1a64(both));
}

// The member of a pack the game finds by name: the first whose name matches, ignoring case.
inline const PackMember *FindMember(const std::vector<PackMember> &members, std::string_view name) {
    const auto found = std::ranges::find_if(members, [&](const PackMember &member) { return EqualsFolded(member.name, name); });
    return found == members.end() ? nullptr : &*found;
}

} // namespace dcdata::sound
