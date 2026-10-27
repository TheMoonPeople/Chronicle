#include "texture.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "localize/localize_texture.hpp"
#include "mglib.hpp"
#include "render/texture_registry.hpp"
#include "text/caption_text.hpp"
#include "tim2.hpp"

namespace {

std::string Sha256Hex(std::string_view input) {
    constexpr std::array<std::uint32_t, 64> round_constants = {
        0x428a2f98,
        0x71374491,
        0xb5c0fbcf,
        0xe9b5dba5,
        0x3956c25b,
        0x59f111f1,
        0x923f82a4,
        0xab1c5ed5,
        0xd807aa98,
        0x12835b01,
        0x243185be,
        0x550c7dc3,
        0x72be5d74,
        0x80deb1fe,
        0x9bdc06a7,
        0xc19bf174,
        0xe49b69c1,
        0xefbe4786,
        0x0fc19dc6,
        0x240ca1cc,
        0x2de92c6f,
        0x4a7484aa,
        0x5cb0a9dc,
        0x76f988da,
        0x983e5152,
        0xa831c66d,
        0xb00327c8,
        0xbf597fc7,
        0xc6e00bf3,
        0xd5a79147,
        0x06ca6351,
        0x14292967,
        0x27b70a85,
        0x2e1b2138,
        0x4d2c6dfc,
        0x53380d13,
        0x650a7354,
        0x766a0abb,
        0x81c2c92e,
        0x92722c85,
        0xa2bfe8a1,
        0xa81a664b,
        0xc24b8b70,
        0xc76c51a3,
        0xd192e819,
        0xd6990624,
        0xf40e3585,
        0x106aa070,
        0x19a4c116,
        0x1e376c08,
        0x2748774c,
        0x34b0bcb5,
        0x391c0cb3,
        0x4ed8aa4a,
        0x5b9cca4f,
        0x682e6ff3,
        0x748f82ee,
        0x78a5636f,
        0x84c87814,
        0x8cc70208,
        0x90befffa,
        0xa4506ceb,
        0xbef9a3f7,
        0xc67178f2,
    };
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto                   rotate = [](std::uint32_t value, unsigned amount) {
        return (value >> amount) | (value << (32 - amount));
    };
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const std::uint64_t       bit_length = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while (bytes.size() % 64 != 56) {
        bytes.push_back(0);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>(bit_length >> shift));
    }
    for (std::size_t block = 0; block < bytes.size(); block += 64) {
        std::array<std::uint32_t, 64> words{};
        for (unsigned index = 0; index < 16; index++) {
            const std::size_t at = block + (static_cast<std::size_t>(index) * 4);
            words[index] = (static_cast<std::uint32_t>(bytes[at]) << 24) |
                           (static_cast<std::uint32_t>(bytes[at + 1]) << 16) |
                           (static_cast<std::uint32_t>(bytes[at + 2]) << 8) | bytes[at + 3];
        }
        for (unsigned index = 16; index < 64; index++) {
            const std::uint32_t s0 = rotate(words[index - 15], 7) ^ rotate(words[index - 15], 18) ^
                                     (words[index - 15] >> 3);
            const std::uint32_t s1 = rotate(words[index - 2], 17) ^ rotate(words[index - 2], 19) ^
                                     (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for (unsigned index = 0; index < 64; index++) {
            const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + s1 + choose + round_constants[index] + words[index];
            const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    constexpr char hex_digits[] = "0123456789abcdef";
    std::string    result;
    result.reserve(64);
    for (std::uint32_t word : state) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result.push_back(hex_digits[(word >> shift) & 0xF]);
        }
    }
    return result;
}

std::string Dc1SourceTag(std::string_view alias) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char byte : alias) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    constexpr char hex_digits[] = "0123456789abcdef";
    std::string    result(16, '0');
    for (int index = 15; index >= 0; index--) {
        result[static_cast<std::size_t>(index)] = hex_digits[hash & 0xF];
        hash >>= 4;
    }
    return result;
}

enum class EnterMode {
    Block,
    Extended,
    Fixed,
};

u_long Tex0(unsigned tbp, int tbw, int psm, int tw, int th, unsigned cbp, int cld) {
    return static_cast<u_long>(tbp) | (static_cast<u_long>(tbw) << 14) | (static_cast<u_long>(psm) << 20) |
           (static_cast<u_long>(tw) << 26) | (static_cast<u_long>(th) << 30) | (static_cast<u_long>(1) << 34) |
           (static_cast<u_long>(cbp) << 37) | (static_cast<u_long>(cld) << 61);
}

int CeilLog2(int value) {
    int log = 0;
    for (int power = value; power >= 2; power >>= 1) {
        log++;
    }
    return (1 << log) == value ? log : log + 1;
}

int Psm(int bpp) {
    switch (bpp) {
        case 0:
            return SCE_GS_PSMT4;
        case 1:
            return SCE_GS_PSMT8;
        case 2:
            return SCE_GS_PSMCT16;
        case 3:
            return SCE_GS_PSMCT24;
        case 4:
            return SCE_GS_PSMCT32;
        default:
            break;
    }
    return -1;
}

int ImageBytes(int width, int height, int bpp) {
    return bpp == 0 ? width * height / 2 : width * height * bpp;
}

bool TextureSourceExtension(std::string_view extension) {
    if (extension == "img" || extension == "tm2" || extension == "im2" || extension == "im3" ||
        extension == "tim2") {
        return true;
    }
    return std::ranges::any_of(std::array<std::string_view, 2>{"img", "tm2"}, [extension](std::string_view base) {
        return extension.starts_with(base) && extension.size() > base.size() &&
               extension.substr(base.size()).find_first_not_of('_') == std::string_view::npos;
    });
}

std::string SafeTextureComponent(std::string_view component) {
    if (component == "." || component == "..") {
        std::string escaped;
        for (std::size_t index = 0; index < component.size(); index++) {
            escaped += "%2E";
        }
        return escaped;
    }
    constexpr char hex[] = "0123456789ABCDEF";
    std::string    result;
    for (unsigned char byte : component) {
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' || byte == '-') {
            result.push_back(static_cast<char>(byte));
        } else {
            result.push_back('%');
            result.push_back(hex[byte >> 4]);
            result.push_back(hex[byte & 0xF]);
        }
    }
    return result;
}

std::string LegacyTextureDocument(std::string_view key) {
    std::vector<std::string> raw;
    for (size_t start = 0; start < key.size();) {
        const size_t end = key.find('/', start);
        const size_t stop = end == std::string_view::npos ? key.size() : end;
        if (stop > start) {
            raw.emplace_back(key.substr(start, stop - start));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    for (size_t index = 0; index < raw.size();) {
        const size_t offset = raw[index].find("#offset:");
        if (offset != std::string::npos) {
            const std::string name = raw[index].substr(0, offset);
            const std::string value = raw[index].substr(offset + 8);
            raw[index] = name;
            raw.insert(raw.begin() + static_cast<std::ptrdiff_t>(index + 1), "embedded-offset-" + value + ".json");
            continue;
        }
        const size_t entry = raw[index].find("#entry:");
        if (entry != std::string::npos) {
            const std::string name = raw[index].substr(0, entry);
            const std::string ordinal = raw[index].substr(entry + 7);
            const size_t      dot = name.rfind('.');
            raw[index] = (dot == std::string::npos ? name : name.substr(0, dot)) + "-entry-" + ordinal + ".json";
        }
        index++;
    }
    if (raw.empty()) {
        return {};
    }
    std::vector<std::string> parts;
    parts.reserve(raw.size() + 1);
    for (size_t index = 0; index < raw.size(); index++) {
        std::string part = SafeTextureComponent(raw[index]);
        if (index + 1 < raw.size()) {
            if (part.size() >= 4 && part.substr(part.size() - 4) == ".img") {
                part.resize(part.size() - 4);
                part += "-img";
            }
        } else {
            std::string lower = part;
            std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (lower.ends_with(".tm2") || lower.ends_with(".img")) {
                part.resize(part.size() - 4);
                part += ".json";
            } else if (!lower.ends_with(".json")) {
                parts.push_back(std::move(part));
                parts.emplace_back("embedded.json");
                continue;
            }
        }
        parts.push_back(std::move(part));
    }
    std::string result;
    for (const std::string &part : parts) {
        if (!result.empty()) {
            result.push_back('/');
        }
        result += part;
    }
    return result;
}

std::string Dc1TextureDocument(std::string_view alias) {
    const size_t           slash = alias.rfind('/');
    const std::string_view leaf = alias.substr(slash == std::string_view::npos ? 0 : slash + 1);
    const size_t           marker = leaf.find('#');
    const std::string_view source_name = leaf.substr(0, marker);
    const size_t           dot = source_name.rfind('.');
    std::string            extension = dot == std::string_view::npos ? std::string{} : std::string(source_name.substr(dot + 1));
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::string path;
    if (!TextureSourceExtension(extension)) {
        path = LegacyTextureDocument(alias);
    } else {
        std::string projected(alias);
        if (extension != "img" && extension != "tm2") {
            const size_t alias_marker = projected.find('#');
            const size_t source_end = alias_marker == std::string::npos ? projected.size() : alias_marker;
            const size_t source_dot = projected.rfind('.', source_end);
            if (source_dot == std::string::npos) {
                return {};
            }
            projected.replace(source_dot, source_end - source_dot, ".img");
        }
        path = LegacyTextureDocument(projected);
        if (path.size() < 5 || path.substr(path.size() - 5) != ".json") {
            return {};
        }
        path.resize(path.size() - 5);
        path += "_" + extension + ".json";
    }

    std::vector<std::string> raw;
    for (size_t start = 0; start < alias.size();) {
        const size_t end = alias.find('/', start);
        const size_t stop = end == std::string_view::npos ? alias.size() : end;
        if (stop > start) {
            raw.emplace_back(alias.substr(start, stop - start));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    std::vector<std::string>   folders;
    std::optional<std::string> character;
    bool                       has_container = false;
    for (size_t index = 0; index < raw.size(); index++) {
        const std::string_view component(raw[index]);
        const size_t           mark = component.find('#');
        const std::string      base(component.substr(0, mark));
        const size_t           base_dot = base.rfind('.');
        if (index + 1 < raw.size() && base_dot != std::string::npos) {
            has_container = true;
        }
        if (base == "." || base == "..") {
            folders.push_back(SafeTextureComponent(base));
        } else if (base_dot != std::string::npos) {
            std::string suffix = base.substr(base_dot + 1);
            std::ranges::transform(suffix, suffix.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (suffix == "chr" || suffix == "chr_") {
                character = SafeTextureComponent(base);
            }
        } else if (base != "." && base != ".." && index + 1 < raw.size()) {
            folders.push_back(SafeTextureComponent(base));
        }
    }
    const size_t path_slash = path.rfind('/');
    std::string  leaf_path = path.substr(path_slash == std::string::npos ? 0 : path_slash + 1);
    if (character) {
        std::string owner = *character;
        const bool  underscore = owner.size() >= 5 && owner.substr(owner.size() - 5) == ".chr_";
        owner.resize(owner.size() - (underscore ? 5 : 4));
        leaf_path = owner + (underscore ? "_" : "") + "-" + leaf_path;
    }
    if (has_container && leaf_path.ends_with(".json")) {
        leaf_path.insert(leaf_path.size() - 5, "-" + Dc1SourceTag(alias));
    }
    std::string result = "textures";
    for (const std::string &folder : folders) {
        result += "/" + folder;
    }
    result += "/" + leaf_path;
    if (result.size() > 160) {
        return "textures/long-path-" + Sha256Hex(alias).substr(0, 24) + ".json";
    }
    return result;
}

uint16_t ReadLe16(const std::uint8_t *bytes) {
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint32_t ReadLe32(const std::uint8_t *bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

std::optional<std::size_t> EmbeddedTextureOffset(const std::vector<std::uint8_t> &bytes, std::size_t container_start,
                                                 std::size_t container_size, std::size_t image_offset) {
    if (container_start > bytes.size() || container_size > bytes.size() - container_start ||
        image_offset < container_start || image_offset - container_start >= container_size) {
        return std::nullopt;
    }
    const std::size_t          container_end = container_start + container_size;
    std::optional<std::size_t> result;
    auto                       record = [&](std::size_t offset) {
        if (result && *result != offset) {
            result = std::numeric_limits<std::size_t>::max();
        } else {
            result = offset;
        }
    };
    for (std::size_t at = container_start; at + 16 <= container_end; at++) {
        if (std::memcmp(bytes.data() + at, "IMG\0", 4) == 0) {
            const uint32_t count = ReadLe32(bytes.data() + at + 4);
            if (count == 0 || count > 256 || static_cast<std::size_t>(count) > (container_end - at - 16) / 48) {
                continue;
            }
            for (uint32_t entry = 0; entry < count; entry++) {
                const std::size_t table_entry = at + 16 + static_cast<std::size_t>(entry) * 48;
                const uint32_t    head_offset = ReadLe32(bytes.data() + table_entry + 32);
                if (head_offset > container_end - at || head_offset + 32 > container_end - at) {
                    continue;
                }
                const std::size_t picture = at + head_offset + 16;
                const uint16_t    header_size = ReadLe16(bytes.data() + picture + 12);
                if (header_size < 0x30 || header_size > container_end - picture ||
                    picture + header_size != image_offset) {
                    continue;
                }
                record(at - container_start);
            }
        }
        if (std::memcmp(bytes.data() + at, "TIM2", 4) != 0 || at + 16 > container_end) {
            continue;
        }
        const uint16_t count = ReadLe16(bytes.data() + at + 6);
        if (count == 0 || count > 256) {
            continue;
        }
        std::size_t picture = at + (bytes[at + 5] != 0 ? 0x80 : 16);
        for (uint16_t index = 0; index < count && picture + 0x30 <= container_end; index++) {
            const uint32_t total = ReadLe32(bytes.data() + picture);
            const uint32_t clut_size = ReadLe32(bytes.data() + picture + 4);
            const uint32_t image_size = ReadLe32(bytes.data() + picture + 8);
            const uint16_t header_size = ReadLe16(bytes.data() + picture + 12);
            if (header_size < 0x30 || header_size > container_end - picture ||
                image_size > container_end - picture - header_size || picture + header_size != image_offset) {
                if (header_size >= 0x30 && header_size <= container_end - picture &&
                    image_size <= container_end - picture - header_size) {
                    picture += header_size + image_size + clut_size;
                    continue;
                }
                break;
            }
            record(at - container_start);
            if (total < header_size + image_size + clut_size || total > container_end - picture) {
                break;
            }
            picture += total;
        }
    }
    if (result && *result == std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return result;
}

std::optional<unsigned> Dc1ImgEntryIndex(const DataReadSource &source) {
    std::size_t member_start = 0;
    auto        member_size = static_cast<std::size_t>(source.range_size);
    if (!source.member.empty()) {
        if (source.member_source_offsets.empty() || source.member_sizes.empty()) {
            return std::nullopt;
        }
        member_start = static_cast<std::size_t>(source.member_source_offsets.back());
        member_size = static_cast<std::size_t>(source.member_sizes.back());
    }
    if (source.offset < member_start || source.offset - member_start >= member_size) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> raw;
    if (!DataReadReadResolvedFile(source.path, raw) || member_start > raw.size() ||
        member_size > raw.size() - member_start || member_size < 16) {
        return std::nullopt;
    }
    const std::uint8_t *member = raw.data() + member_start;
    const bool          img3 = std::memcmp(member, "IM3\0", 4) == 0;
    if (!img3 && std::memcmp(member, "IMG\0", 4) != 0 && std::memcmp(member, "IM2\0", 4) != 0) {
        return std::nullopt;
    }
    const unsigned    count = ReadLe32(member + (img3 ? 8 : 4));
    const std::size_t stride = img3 ? 64 : 48;
    if (count == 0 || count > 16384 || count > (member_size - 16) / stride) {
        return std::nullopt;
    }
    const auto              image_offset = static_cast<std::size_t>(source.offset - member_start);
    std::optional<unsigned> result;
    for (unsigned index = 0; index < count; index++) {
        const std::size_t table_entry = 16 + static_cast<std::size_t>(index) * stride;
        const std::size_t head_offset = ReadLe32(member + table_entry + (img3 ? 36 : 32));
        bool              matches = false;
        if (img3) {
            const std::size_t image_size = ReadLe32(member + table_entry + 52);
            matches = head_offset <= member_size && image_size <= member_size - head_offset &&
                      image_offset >= head_offset && image_offset - head_offset < image_size;
        } else if (head_offset <= member_size && 32 <= member_size - head_offset) {
            const std::size_t picture = head_offset + 16;
            const std::size_t header_size = ReadLe16(member + picture + 12);
            matches = header_size >= 0x30 && header_size <= member_size - picture &&
                      picture + header_size == image_offset;
        }
        if (matches) {
            if (result) {
                return std::nullopt;
            }
            result = index;
        }
    }
    return result;
}

std::optional<std::string> BakedTextureDocument(const DataReadSource &source) {
    std::string alias = source.path;
    if (!source.member.empty()) {
        alias += '/';
        alias += source.member;
    }
    const size_t           last_slash = alias.rfind('/');
    const std::string_view leaf(alias.data() + (last_slash == std::string::npos ? 0 : last_slash + 1),
                                alias.size() - (last_slash == std::string::npos ? 0 : last_slash + 1));
    const size_t           hash = leaf.find('#');
    const std::string_view basename = leaf.substr(0, hash);
    const size_t           dot = basename.rfind('.');
    std::string            extension = dot == std::string_view::npos ? std::string{} : std::string(basename.substr(dot + 1));
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if ((extension == "chr" || extension == "chr_") && !alias.contains("#offset:")) {
        std::size_t container_start = 0;
        std::size_t container_size = 0;
        if (!source.member.empty()) {
            if (source.member_source_offsets.empty() || source.member_sizes.empty()) {
                return std::nullopt;
            }
            container_start = static_cast<std::size_t>(source.member_source_offsets.back());
            container_size = static_cast<std::size_t>(source.member_sizes.back());
        } else {
            container_size = static_cast<std::size_t>(source.range_size);
        }
        std::vector<std::uint8_t> raw;
        if (!DataReadReadResolvedFile(source.path, raw)) {
            return std::nullopt;
        }
        const auto offset = EmbeddedTextureOffset(raw, container_start, container_size,
                                                  static_cast<std::size_t>(source.offset));
        if (!offset) {
            return std::nullopt;
        }
        if (*offset != 0) {
            alias += "#offset:" + std::to_string(*offset);
        }
    }
    std::string path = Dc1TextureDocument(alias);
    if (path.empty()) {
        return std::nullopt;
    }
    return path;
}

[[noreturn]] void Stop(const char *what, const char *name, int value) {
    std::println(stderr, "texture: {} ({} {})", what, name, value);
    std::abort();
}

[[noreturn]] void StopBakedTexture(const char *name, std::string_view message) {
    std::println(stderr, "texture: baked asset resolution failed for {}: {}", name, message);
    std::abort();
}

void CopyTextureName(std::span<char, 32> destination, std::string_view name) {
    const std::size_t length = std::min(name.size(), destination.size() - 1);
    std::ranges::copy(name.substr(0, length), destination.begin());
    destination[length] = '\0';
}

void ReleaseKeys(const CTexture &texture) {
    if (texture.name[0] == 0) {
        return;
    }
    const auto tex0 = std::bit_cast<sceGsTex0>(texture.tex0);
    PortReleaseKey(tex0.TBP0);
    if (tex0.CBP != 0) {
        PortReleaseKey(tex0.CBP);
    }
}

uint8_t NearestPaletteColor(const std::array<uint32_t, 256> &palette, uint32_t color) {
    unsigned best_distance = ~0u;
    uint8_t  best_index = 0;
    for (unsigned index = 0; index < palette.size(); index++) {
        unsigned distance = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            int difference = static_cast<int>((palette[index] >> shift) & 0xff) -
                             static_cast<int>((color >> shift) & 0xff);
            distance += static_cast<unsigned>(difference * difference);
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_index = static_cast<uint8_t>(index);
        }
    }
    return best_index;
}

uint32_t BlendColors(uint32_t first, uint32_t second, unsigned first_weight, unsigned total_weight) {
    uint32_t blended = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        unsigned first_channel = (first >> shift) & 0xff;
        unsigned second_channel = (second >> shift) & 0xff;
        unsigned channel = (first_channel * first_weight + second_channel * (total_weight - first_weight) +
                            total_weight / 2) /
                           total_weight;
        blended |= channel << shift;
    }
    return blended;
}

// DC_TEXTURE_DUMP=<dir>: every entered texture is written there as <name>.png, indexed textures
// through their palette.
void DumpTexture(const char *name, const PortDecodedTexture &texture) {
    static const char *dir = std::getenv("DC_TEXTURE_DUMP");
    if (dir == nullptr || texture.levels.empty()) {
        return;
    }
    const unsigned       width = texture.width;
    const unsigned       height = texture.height;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; pixel++) {
        if (texture.format == gfx::TextureFormat::Index8) {
            const uint32_t color = texture.palette[texture.levels[0][pixel]];
            std::memcpy(&rgba[pixel * 4], &color, 4);
        } else {
            std::memcpy(&rgba[pixel * 4], &texture.levels[0][pixel * 4], 4);
        }
    }
    // A PNG with stored (uncompressed) deflate blocks.
    auto crc = [](std::span<const uint8_t> data) {
        uint32_t value = ~0u;
        for (const uint8_t byte : data) {
            value ^= byte;
            for (int bit = 0; bit < 8; bit++) {
                value = (value >> 1) ^ (0xEDB88320u & (0u - (value & 1)));
            }
        }
        return ~value;
    };
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    auto                 put32 = [](std::vector<uint8_t> &target, uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            target.push_back(static_cast<uint8_t>(value >> shift));
        }
    };
    auto chunk = [&](std::string_view type, const std::vector<uint8_t> &body) {
        put32(out, static_cast<uint32_t>(body.size()));
        std::vector<uint8_t> typed(type.begin(), type.end());
        typed.insert(typed.end(), body.begin(), body.end());
        out.insert(out.end(), typed.begin(), typed.end());
        put32(out, crc(typed));
    };
    std::vector<uint8_t> ihdr;
    put32(ihdr, width);
    put32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk("IHDR", ihdr);
    std::vector<uint8_t> raw;
    for (unsigned y = 0; y < height; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + static_cast<size_t>(y) * width * 4,
                   rgba.begin() + static_cast<size_t>(y + 1) * width * 4);
    }
    std::vector<uint8_t> zlib = {0x78, 0x01};
    uint32_t             adler_low = 1;
    uint32_t             adler_high = 0;
    for (const uint8_t byte : raw) {
        adler_low = (adler_low + byte) % 65521;
        adler_high = (adler_high + adler_low) % 65521;
    }
    for (size_t position = 0; position < raw.size() || position == 0; position += 65535) {
        const size_t length = std::min<size_t>(65535, raw.size() - position);
        zlib.push_back(position + length >= raw.size() ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(length));
        zlib.push_back(static_cast<uint8_t>(length >> 8));
        zlib.push_back(static_cast<uint8_t>(~length));
        zlib.push_back(static_cast<uint8_t>((~length) >> 8));
        const auto block_begin = raw.begin() + static_cast<std::ptrdiff_t>(position);
        zlib.insert(zlib.end(), block_begin, block_begin + static_cast<std::ptrdiff_t>(length));
    }
    put32(zlib, (adler_high << 16) | adler_low);
    chunk("IDAT", zlib);
    chunk("IEND", {});
    std::filesystem::create_directories(dir);
    std::string path = std::string(dir) + "/" + name + ".png";
    if (FILE *file = std::fopen(path.c_str(), "wb")) {
        std::fwrite(out.data(), 1, out.size(), file);
        std::fclose(file);
    }
}

// Ground models repeat these images across UV islands and cells. Their source edges differ, so
// blend a narrow indexed border into a periodic image before uploading it.
void MakeGroundPeriodic(std::string_view name, PortDecodedTexture &texture) {
    if ((name != "d02b05" && name != "d02b06" && name != "e04b01") ||
        (texture.format != gfx::TextureFormat::Index8 && texture.format != gfx::TextureFormat::Rgba8) ||
        texture.width < 64 || texture.height < 64 ||
        texture.levels.empty()) {
        return;
    }

    const unsigned        horizontal_band = name == "d02b05" ? 32 : 8;
    const unsigned        vertical_band = name == "d02b06" ? 32 : 8;
    std::vector<uint8_t> &pixels = texture.levels[0];
    const unsigned        width = texture.width;
    const unsigned        height = texture.height;
    const auto            color_at = [&](unsigned pixel) {
        if (texture.format == gfx::TextureFormat::Index8) {
            return texture.palette[pixels[pixel]];
        }
        uint32_t color = 0;
        std::memcpy(&color, pixels.data() + static_cast<std::size_t>(pixel) * 4, sizeof(color));
        return color;
    };
    const auto set_color = [&](unsigned pixel, uint32_t color) {
        if (texture.format == gfx::TextureFormat::Index8) {
            pixels[pixel] = NearestPaletteColor(texture.palette, color);
        } else {
            std::memcpy(pixels.data() + static_cast<std::size_t>(pixel) * 4, &color, sizeof(color));
        }
    };

    for (unsigned y = 0; y < height; y++) {
        for (unsigned offset = 0; offset < horizontal_band; offset++) {
            unsigned left = y * width + offset;
            unsigned right = y * width + width - 1 - offset;
            uint32_t left_color = color_at(left);
            uint32_t right_color = color_at(right);
            unsigned left_weight = horizontal_band + offset;
            unsigned right_weight = horizontal_band - offset;
            set_color(left, BlendColors(left_color, right_color, left_weight, 2 * horizontal_band));
            set_color(right, BlendColors(left_color, right_color, right_weight, 2 * horizontal_band));
        }
    }

    for (unsigned x = 0; x < width; x++) {
        for (unsigned offset = 0; offset < vertical_band; offset++) {
            unsigned top = offset * width + x;
            unsigned bottom = (height - 1 - offset) * width + x;
            uint32_t top_color = color_at(top);
            uint32_t bottom_color = color_at(bottom);
            unsigned top_weight = vertical_band + offset;
            unsigned bottom_weight = vertical_band - offset;
            set_color(top, BlendColors(top_color, bottom_color, top_weight, 2 * vertical_band));
            set_color(bottom, BlendColors(top_color, bottom_color, bottom_weight, 2 * vertical_band));
        }
    }
}

// EnterTexture, EnterTextureEX and EnterFixTexture share everything but where pixels are staged and
// which VRAM end moves. The staging and VRAM arithmetic must match retail: the game sizes later
// allocations from buffer_used (editloop's LoadTexture). The image goes to the renderer, and
// TBP0/CBP are registry keys, not VRAM addresses.
void Enter(CTextureManager &manager, EnterMode mode, int block, char *name, u_char *image, int width, int height,
           int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_long tex1,
           int swizzled) {
    if (name[0] == 0) {
        return;
    }
    CTexture *tex = manager.SearchTexture(name);
    if (image == nullptr || mip1 == nullptr) {
        mipmap = false;
    }
    int psm = Psm(bpp);
    if (psm < 0) {
        return;
    }
    bool indexed = bpp < 2;
    int  tw = CeilLog2(width);
    int  th = CeilLog2(height);
    int  tbw = width >> 6;
    if (tbw <= 0) {
        tbw = 1;
    }
    int image_size = ImageBytes(width, height, bpp);
    int image_blocks = image_size >> 8;
    int clut_bytes = bpp == 0 ? 64 : 1024;

    if (mode == EnterMode::Fixed) {
        int fix_blocks = image_blocks;
        if (mipmap) {
            int mip_blocks = image_blocks >> 2;
            fix_blocks += mip_blocks + (mip_blocks >> 2);
        }
        if (fix_blocks % 32) {
            fix_blocks += 32 - fix_blocks % 32;
        }
        manager.vram_fix -= fix_blocks;
        if (image == nullptr ? psm != SCE_GS_PSMCT32 : indexed) {
            manager.vram_fix -= 32;
        }
        if (image != nullptr && !indexed) {
            tex->clut = nullptr;
        }
    } else {
        CTextureBlock &entry = manager.blocks[block];
        int            vram_top = entry.vram_top;
        int            vram_end = entry.vram_end;
        bool           staged = mode == EnterMode::Block;
        if (!staged) {
            entry.extend = true;
        }
        if (image == nullptr) {
            vram_top += image_blocks;
            if (indexed) {
                vram_top += 4;
            }
            vram_end = vram_top;
        } else {
            auto stage = [&](u_char *source, int bytes, int blocks) -> u_int * {
                auto *destination = reinterpret_cast<u_int *>(manager.buffer + manager.buffer_used);
                if (staged) {
                    // A picture with fewer levels than the block asks for leaves the last one null,
                    // which retail copies from all the same.
                    if (source != nullptr) {
                        std::memcpy(destination, source, bytes);
                    }
                    manager.buffer_used += blocks * 16;
                }
                vram_end += blocks;
                return staged ? destination : reinterpret_cast<u_int *>(source);
            };
            tex->image[0] = stage(image, image_size, image_blocks);
            if (mipmap) {
                tex->image[1] = stage(mip1, image_size >> 2, image_blocks >> 2);
                tex->image[2] = stage(mip2, image_size >> 4, (image_blocks >> 2) >> 2);
            }
            if (indexed) {
                auto *staged_clut = reinterpret_cast<u_int *>(manager.buffer + manager.buffer_used);
                if (staged) {
                    std::memcpy(staged_clut, clut, clut_bytes);
                    manager.buffer_used += 64;
                }
                tex->clut = staged ? staged_clut : reinterpret_cast<u_int *>(clut);
                vram_end += 4;
            } else {
                tex->clut = nullptr;
            }
            tex->swizzled = staged ? indexed : swizzled;
        }
        if (vram_end % 32) {
            int pad = 32 - vram_end % 32;
            vram_end += pad;
            if (staged) {
                manager.buffer_used += pad * 16;
            }
        }
        entry.vram_top = vram_top;
        entry.vram_end = vram_end;
        if (staged && manager.buffer_used >= manager.buffer_size) {
            Stop("texture buffer over", name, manager.buffer_used);
        }
        if (entry.vram_end > manager.vram_max) {
            Stop("VRAM is not enough", name, entry.vram_end);
        }
    }

    unsigned           tbp = 0;
    unsigned           cbp = 0;
    bool               baked = false;
    bool               canvas_backing = false;
    bool               unsourced = false;
    PortDecodedTexture decoded;
    if (image != nullptr && !PortLegacyTexturePath()) {
        if (indexed) {
            PortRememberTextureClut(name, clut, static_cast<std::size_t>(clut_bytes));
        }
        DataReadSource             source;
        std::string                resolution_error;
        std::optional<std::string> document;
        if (image_size > 0 && DataReadSourceRange(image, static_cast<std::size_t>(image_size), source)) {
            document = BakedTextureDocument(source);
            if (!document) {
                StopBakedTexture(name, "unsupported source identity " + source.path +
                                           (source.member.empty() ? std::string{} : "/" + source.member));
            }
            const std::optional<unsigned> entry_index = Dc1ImgEntryIndex(source);
            baked = PortLoadBakedTexture(*document, name, 0, static_cast<unsigned>(width),
                                         static_cast<unsigned>(height), 0, clut,
                                         clut == nullptr ? 0 : static_cast<std::size_t>(clut_bytes), decoded,
                                         resolution_error, entry_index, bpp);
            canvas_backing = !baked && resolution_error == "canvas texture keeps its runtime backing";
            if (!baked && !canvas_backing) {
                StopBakedTexture(name, resolution_error);
            }
        } else {
            // Pixels outside any data source (a synthetic or runtime image) have no baked document
            // and decode from the bytes given.
            resolution_error = "texture pixels have no validated source range";
            unsourced = true;
        }
        if (!baked && indexed && !canvas_backing && !unsourced) {
            StopBakedTexture(name, resolution_error);
        }
        if (!baked && !canvas_backing && !resolution_error.empty()) {
            std::println(stderr, "texture: using source decoder for non-indexed {}: {}", name, resolution_error);
        }
    }
    if (image == nullptr) {
        tbp = PortCreatePlaceholder(name, width, height, bpp, PortTextureOwner::Manager, &cbp);
    } else {
        if (!baked) {
            const std::array<const u_char *, 3> levels = {image, mipmap ? mip1 : nullptr, mipmap ? mip2 : nullptr};
            if (!PortDecodeTexture(bpp, width, height, levels.data(), mipmap ? 3 : 1, clut, clut_colors,
                                   indexed && swizzled != 0, decoded)) {
                return;
            }
        }
        LocalizeTexture(name, decoded);
        caption::PreparePainted(name, decoded);
        MakeGroundPeriodic(name, decoded);
        DumpTexture(name, decoded);
        if (baked) {
            tex->clut = nullptr;
            cbp = 0;
            psm = SCE_GS_PSMCT32;
        }
        tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, &cbp);
    }
    tex->tex0 = Tex0(tbp, tbw, psm, tw, th, cbp, indexed ? 1 : 0);

    CopyTextureName(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = bpp;
    tex->block = mode == EnterMode::Fixed ? -1 : block;

    if (bpp > 0 && mipmap) {
        tex->tex1 = SCE_GS_SET_TEX1(0, 2, 1, 5, 1, 0, -120);
        if (tex1 != 0) {
            const auto lod = std::bit_cast<sceGsTex1>(tex1);
            tex->tex1 = SCE_GS_SET_TEX1(0, 2, 1, 5, 1, lod.L, lod.K);
        }
    }
    manager.texture_max = 195;
}

} // namespace

std::optional<std::string> PortTextureDocumentFromSource(const DataReadSource &source) {
    return BakedTextureDocument(source);
}

std::string PortTextureDocumentFromAlias(std::string_view source_alias) {
    return Dc1TextureDocument(source_alias);
}

std::optional<unsigned> PortTextureEntryIndexFromSource(const DataReadSource &source) {
    return Dc1ImgEntryIndex(source);
}

void CTextureManager::Initialize(int size) {
    PortReleaseOwner(PortTextureOwner::Manager);

    TextureData.used = 0;
    SetBuffer(reinterpret_cast<u_long128 *>(TextureData.base + (static_cast<std::ptrdiff_t>(TextureData.used) * 16)), TextureData.limit);

    for (CTextureBlock &block : blocks) {
        block.Initialize();
        block.vram_top = mgTopVRAM;
        block.vram_end = mgTopVRAM;
    }
    for (CTexture &texture : textures) {
        texture.Initialize();
    }

    texture_max = 1;
    vram_work = 8960;
    vram_size = size;
    vram_max = vram_size;
    vram_fix = size;

    // The frame-copy area water samples, a field of the frame on PS2: the water unit fills it.
    unsigned work = PortRegisterNamedTarget("work", 640, SCREEN_HALF_HEIGHT, true, PortTextureOwner::Manager);
    CopyTextureName(textures[0].name, "work");
    textures[0].tex0 = SCE_GS_SET_TEX0(work, 10, 0, 10, 8, 1, 0, 0, 0, 0, 0, 0);
    textures[0].block = -1;
    last_block = -1;
}

void CTextureManager::EnterTexture(int block, char *name, u_char *image, int width, int height, int bpp,
                                   u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2,
                                   u_char *mip3, u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Block, block, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1, mip2,
          tex1, swizzled);
}

void CTextureManager::EnterTextureEX(int block, char *name, u_char *image, int width, int height, int bpp,
                                     u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2,
                                     u_char *mip3, u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Extended, block, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1,
          mip2, tex1, swizzled);
}

void CTextureManager::EnterFixTexture(char *name, u_char *image, int width, int height, int bpp, u_char *clut,
                                      int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3,
                                      u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Fixed, -1, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1, mip2,
          tex1, swizzled);
}

namespace {

// GS pixel storage mode PSMT8H: 8-bit indices in the upper bytes of a 32-bit word.
constexpr unsigned kPsmT8H = 27;

// Decodes the fixed-Z sheet from the TIM2 in the buffer, for data without baked documents.
void EnterRawFixTextureZ(CTextureManager &manager, u_char *buffer) {
    char     *name = reinterpret_cast<char *>(buffer + 16);
    auto     *head = reinterpret_cast<TM2_head *>(buffer + *reinterpret_cast<int *>(buffer + 48));
    auto     *picture = reinterpret_cast<TM2_picture *>(reinterpret_cast<u_char *>(head) + 16);
    const int width = head->image_width;
    const int height = head->image_height;
    if (width != 640 || height > SCREEN_HALF_HEIGHT || picture->image_type != TIM2_IDTEX8) {
        return;
    }

    u_char   *image = reinterpret_cast<u_char *>(picture) + picture->header_size;
    u_char   *clut = image + picture->image_size;
    CTexture *tex = manager.SearchTexture(name);
    CopyTextureName(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = 1;
    tex->block = 73;

    const std::array<const u_char *, 1> levels = {image};
    PortDecodedTexture                  decoded;
    unsigned                            tbp = 0;
    unsigned                            cbp = 0;
    if (PortDecodeTexture(1, width, height, levels.data(), 1, clut, 256, false, decoded)) {
        LocalizeTexture(name, decoded);
        caption::PreparePainted(name, decoded);
        DumpTexture(name, decoded);
        tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, &cbp);
    }
    tex->tex0 = SCE_GS_SET_TEX0(tbp, 10, kPsmT8H, 10, 8, 1, 0, cbp, 0, 0, 0, 1);
}

} // namespace

// Retail parks "stayframe", the menus' 640-wide 8-bit sheet of frames and digits, in the upper
// bytes of the Z buffer as PSMT8H. Here it is an index texture like any other; the menus reach it
// by name and draw pieces of it with set2DSprite.
void CTextureManager::EnterFixTextureZ(u_char *buffer) {
    if (PortLegacyTexturePath()) {
        EnterRawFixTextureZ(*this, buffer);
        return;
    }
    DataReadSource source;
    if (!DataReadSourceRange(buffer, 64, source)) {
        StopBakedTexture("<unknown>", "fixed-Z texture header has no validated source range");
    }
    char             *name = reinterpret_cast<char *>(buffer + 16);
    const std::size_t name_size = std::string_view(name, 32).find('\0');
    if (name_size == 0 || name_size == std::string_view::npos) {
        StopBakedTexture("<unknown>", "fixed-Z texture name is not terminated");
    }
    const std::optional<std::string> document = BakedTextureDocument(source);
    if (!document) {
        StopBakedTexture(name, "fixed-Z source has no deterministic baked document path");
    }
    PortDecodedTexture decoded;
    std::string        error;
    if (!PortLoadBakedTexture(*document, name, 0, 640, 0, 0, nullptr, 0, decoded, error)) {
        StopBakedTexture(name, error);
    }
    if (decoded.height > SCREEN_HALF_HEIGHT) {
        StopBakedTexture(name, "fixed-Z baked texture exceeds the logical height limit");
    }
    CTexture *tex = SearchTexture(name);
    if (tex == nullptr) {
        StopBakedTexture(name, "fixed-Z texture has no manager entry");
    }
    CopyTextureName(tex->name, name);
    tex->width = static_cast<int>(decoded.width);
    tex->height = static_cast<int>(decoded.height);
    tex->bpp = 1;
    tex->block = 73;
    LocalizeTexture(name, decoded);
    caption::PreparePainted(name, decoded);
    DumpTexture(name, decoded);
    const unsigned tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, nullptr);
    tex->tex0 = SCE_GS_SET_TEX0(tbp, 10, SCE_GS_PSMCT32, 10, 8, 1, 0, 0, 0, 0, 0, 0);
}

// Native textures occupy independent storage, so simulated VRAM overlap never overwrites either
// image; only the staging bounds check and block finalization remain.
void CTextureManager::EndEnterTextureBlock(int block) {
    if (block < 0 || block >= 72) {
        return;
    }
    if (buffer_used > buffer_size) {
        Stop("texture buffer over", "quadwords", buffer_used);
    }
    CTextureBlock &entry = blocks[block];
    entry.buffer_end = buffer + buffer_used;
    if (entry.buffer_end == entry.buffer) {
        entry.buffer = nullptr;
        entry.buffer_end = nullptr;
    }
}

// Every texture is resident on the renderer from the moment it is entered, so a block never needs
// uploading; only its bookkeeping remains.
void CTextureManager::ReloadTexture(sceVif1Packet *packet, int block) {
    if (block < 0 || block >= 72) {
        last_block = -1;
        return;
    }
    last_block = block;
    blocks[block].loaded = true;
}

int CTextureManager::DeleteTextureBlock(int block) {
    if (block >= 72) {
        return 0;
    }
    if (block >= 0) {
        blocks[block].Initialize();
    }
    if (block < 0) {
        block = -1;
    }
    for (CTexture &texture : textures) {
        if (block < 0 || texture.block == block) {
            ReleaseKeys(texture);
            texture.Initialize();
        }
    }
    return 1;
}

int LoadImage(u_int *packet, int dbp, int dpsm, int dbw, u_long128 *source, int qwc, int dsax, int dsay, int rrw,
              int rrh) {
    return 0;
}

int CTextureManager::CleanUpTextureList() {
    // Entries are never moved: a native visual keeps a table index (Draw3DStrip::texture) that a
    // move would retarget. SearchTexture fills empty entries, so a reloaded block takes the first
    // available holes.
    return 1;
}

void CTextureManager::SetBuffer(u_long128 *buffer, int size) {
    if (size < 0 || (buffer == nullptr && size != 0)) {
        buffer = nullptr;
        size = 0;
    }
    this->buffer = buffer;
    buffer_size = size;
    const int misalignment = buffer == nullptr ? 0 : static_cast<int>(reinterpret_cast<std::uintptr_t>(buffer) & 0x7f);
    if (misalignment != 0) {
        const int skipped_quads = (128 - misalignment) >> 4;
        if (skipped_quads > buffer_size) {
            this->buffer = nullptr;
            buffer_size = 0;
        } else {
            this->buffer += skipped_quads;
            buffer_size -= skipped_quads;
        }
    }

    buffer_used = 0;
}

int CTextureManager::CleanUpBuffer() {
    constexpr int block_count = static_cast<int>(std::extent_v<decltype(blocks)>);
    auto          block_start = std::make_unique<u_long128 *[]>(block_count);
    auto          block_order = std::make_unique<int[]>(block_count);
    auto          block_quads = std::make_unique<int[]>(block_count);

    for (int i = 0; i < block_count; i++) {
        block_start[i] = blocks[i].buffer;
        block_order[i] = i;
    }

    for (int i = 0; i < block_count - 1; i++) {
        for (int j = i + 1; j < block_count; j++) {
            if (reinterpret_cast<std::uintptr_t>(block_start[i]) > reinterpret_cast<std::uintptr_t>(block_start[j])) {
                std::swap(block_start[i], block_start[j]);
                std::swap(block_order[i], block_order[j]);
            }
        }
    }

    buffer_used = 0;
    if (buffer == nullptr || buffer_size < 0) {
        return 0;
    }

    const auto           buffer_address = reinterpret_cast<std::uintptr_t>(buffer);
    const std::uintptr_t buffer_bytes = static_cast<std::uintptr_t>(buffer_size) * sizeof(u_long128);
    if (buffer_bytes > std::numeric_limits<std::uintptr_t>::max() - buffer_address) {
        return 0;
    }
    const std::uintptr_t buffer_end = buffer_address + buffer_bytes;
    int                  required = 0;
    for (int i = 0; i < block_count; i++) {
        const int block_no = block_order[i];
        if (block_no <= 0 || block_start[i] == nullptr) {
            continue;
        }
        const auto start = reinterpret_cast<std::uintptr_t>(blocks[block_no].buffer);
        const auto end = reinterpret_cast<std::uintptr_t>(blocks[block_no].buffer_end);
        if (start < buffer_address || end < start || end > buffer_end || (end - start) % sizeof(u_long128) != 0) {
            return 0;
        }
        const std::uintptr_t quads = (end - start) / sizeof(u_long128);
        if (quads > static_cast<std::uintptr_t>(buffer_size - required)) {
            return 0;
        }
        block_quads[block_no] = static_cast<int>(quads);
        required += static_cast<int>(quads);
        const int padding = (8 - required % 8) % 8;
        if (padding > buffer_size - required) {
            return 0;
        }
        required += padding;
    }
    for (int i = 0; i < block_count; i++) {
        const int block_no = block_order[i];
        if (block_no <= 0) {
            continue;
        }

        u_long128 *source = block_start[i];
        if (source == nullptr) {
            continue;
        }

        const int            block_size = block_quads[block_no];
        u_long128           *destination = buffer + buffer_used;
        const std::ptrdiff_t shift = (source - destination) * static_cast<std::ptrdiff_t>(sizeof(u_long128));
        blocks[block_no].buffer = destination;
        blocks[block_no].buffer_end = destination + block_size;

        // Blocks are compacted towards the buffer start in address order, so a forward copy is safe.
        std::copy_n(source, block_size, destination);

        for (CTexture &tex : textures) {
            if (shift == 0) {
                break;
            }
            if (tex.block != block_no) {
                continue;
            }
            for (u_int *&level : tex.image) {
                if (level != nullptr) {
                    level = reinterpret_cast<u_int *>(reinterpret_cast<char *>(level) - shift);
                }
            }
            if (tex.clut != nullptr) {
                tex.clut = reinterpret_cast<u_int *>(reinterpret_cast<char *>(tex.clut) - shift);
            }
        }

        buffer_used += block_size;
        const int misalignment = block_size % 8;

        if (misalignment != 0) {
            const int padding = 8 - misalignment;
            if (padding > buffer_size - buffer_used) {
                return 0;
            }
            buffer_used += padding;
        }
    }

    return 1;
}
