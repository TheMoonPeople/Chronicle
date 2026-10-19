#pragma once

// The texture converter: reads the TIM2 pictures out of the extracted game files and writes them as PNGs the port
// loads in place of the packed ones (port/src/texture_assets.hpp, docs/TEXTURE_ASSETS.md). Header-only like the rest
// of dcdata and free of game headers. tools/areapack is the reference for the decoding and the naming.
//
// Layout written under <assets>:
//
//     textures/<name>.png            used in every area
//     <area>/textures/<name>.png     used only in that dungeon (d01..d07) or town / event map, when it differs
//     a_eng/textures/<name>.png      used only with that language (a_jpn a_usa a_eng a_fre a_ger a_ita a_spa)
//
// A name two sizes share is written `<name>_<w>x<h>.png`. A picture that is the same everywhere it appears is written
// once, in textures/.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "dcdata.hpp"

namespace dcdata {

// -------------------------------------------------------------------------------------------------- PNG

namespace png_detail {

inline std::uint32_t Crc32(const std::uint8_t *data, std::size_t size, std::uint32_t crc = 0) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; i++) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
            }
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = ~crc;
    for (std::size_t i = 0; i < size; i++) {
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return ~c;
}

struct Bits {
    std::vector<std::uint8_t> out;
    std::uint32_t             buffer = 0;
    int                       count = 0;

    void Put(std::uint32_t value, int bits) { // least significant bit first
        buffer |= value << count;
        count += bits;
        while (count >= 8) {
            out.push_back(static_cast<std::uint8_t>(buffer));
            buffer >>= 8;
            count -= 8;
        }
    }
    void PutCode(std::uint32_t code, int bits) { // Huffman codes go most significant bit first
        std::uint32_t reversed = 0;
        for (int i = 0; i < bits; i++) {
            reversed |= ((code >> i) & 1u) << (bits - 1 - i);
        }
        Put(reversed, bits);
    }
    void Flush() {
        if (count > 0) {
            out.push_back(static_cast<std::uint8_t>(buffer));
            buffer = 0;
            count = 0;
        }
    }
};

inline void PutLiteral(Bits &bits, unsigned value) {
    if (value < 144) {
        bits.PutCode(0x30 + value, 8);
    } else if (value < 256) {
        bits.PutCode(0x190 + (value - 144), 9);
    } else if (value < 280) {
        bits.PutCode(value - 256, 7);
    } else {
        bits.PutCode(0xC0 + (value - 280), 8);
    }
}

// A zlib stream: one block of fixed Huffman codes over a hash-chain LZ77. Smaller than stored blocks by a wide
// margin on flat pictures, and a few lines instead of a dependency.
inline std::vector<std::uint8_t> Zlib(const std::vector<std::uint8_t> &in) {
    static constexpr int kLengthBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                            31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static constexpr int kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                             2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static constexpr int kDistBase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                          193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static constexpr int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                           6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    Bits bits;
    bits.out = {0x78, 0x01};
    bits.Put(1, 1); // final block
    bits.Put(1, 2); // fixed Huffman codes

    const std::size_t        n = in.size();
    constexpr int            kHash = 1 << 15;
    constexpr std::size_t    kWindow = 32768;
    std::vector<std::int32_t> head(kHash, -1);
    std::vector<std::int32_t> prev(n, -1);
    auto hash = [&](std::size_t i) {
        return ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & (kHash - 1);
    };
    auto insert = [&](std::size_t i) {
        if (i + 2 < n) {
            int h = hash(i);
            prev[i] = head[h];
            head[h] = static_cast<std::int32_t>(i);
        }
    };

    std::size_t i = 0;
    while (i < n) {
        std::size_t best_length = 0;
        std::size_t best_distance = 0;
        if (i + 2 < n) {
            std::int32_t candidate = head[hash(i)];
            int          steps = 24;
            while (candidate >= 0 && steps-- > 0 && i - static_cast<std::size_t>(candidate) <= kWindow) {
                std::size_t length = 0;
                std::size_t limit = std::min<std::size_t>(258, n - i);
                while (length < limit && in[candidate + length] == in[i + length]) {
                    length++;
                }
                if (length > best_length) {
                    best_length = length;
                    best_distance = i - static_cast<std::size_t>(candidate);
                    if (length == limit) {
                        break;
                    }
                }
                candidate = prev[candidate];
            }
        }
        if (best_length >= 3) {
            int code = 28;
            while (kLengthBase[code] > static_cast<int>(best_length)) {
                code--;
            }
            PutLiteral(bits, 257 + code);
            bits.Put(static_cast<std::uint32_t>(best_length - kLengthBase[code]), kLengthExtra[code]);
            int dist = 29;
            while (kDistBase[dist] > static_cast<int>(best_distance)) {
                dist--;
            }
            bits.PutCode(static_cast<std::uint32_t>(dist), 5);
            bits.Put(static_cast<std::uint32_t>(best_distance - kDistBase[dist]), kDistExtra[dist]);
            for (std::size_t k = 0; k < best_length; k++) {
                insert(i + k);
            }
            i += best_length;
        } else {
            PutLiteral(bits, in[i]);
            insert(i);
            i++;
        }
    }
    PutLiteral(bits, 256);
    bits.Flush();

    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t byte : in) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    std::uint32_t adler = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) {
        bits.out.push_back(static_cast<std::uint8_t>(adler >> s));
    }
    return std::move(bits.out);
}

} // namespace png_detail

/// An 8-bit RGBA PNG, rows top to bottom.
inline std::vector<std::uint8_t> EncodePng(const std::uint8_t *rgba, std::uint32_t width, std::uint32_t height) {
    using namespace png_detail;
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(height) * (width * 4 + 1));
    for (std::uint32_t y = 0; y < height; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + static_cast<std::size_t>(y) * width * 4,
                   rgba + static_cast<std::size_t>(y + 1) * width * 4);
    }
    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    auto put32 = [](std::vector<std::uint8_t> &v, std::uint32_t x) {
        for (int s = 24; s >= 0; s -= 8) {
            v.push_back(static_cast<std::uint8_t>(x >> s));
        }
    };
    auto chunk = [&](const char *type, const std::vector<std::uint8_t> &body) {
        put32(out, static_cast<std::uint32_t>(body.size()));
        std::vector<std::uint8_t> typed(type, type + 4);
        typed.insert(typed.end(), body.begin(), body.end());
        out.insert(out.end(), typed.begin(), typed.end());
        put32(out, Crc32(typed.data(), typed.size()));
    };
    std::vector<std::uint8_t> header;
    put32(header, width);
    put32(header, height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    chunk("IHDR", header);
    chunk("IDAT", Zlib(raw));
    chunk("IEND", {});
    return out;
}

// -------------------------------------------------------------------------------------------------- TIM2

struct Picture {
    std::uint32_t             width = 0;
    std::uint32_t             height = 0;
    std::vector<std::uint8_t> rgba; // width * height * 4
};

namespace tim2_detail {

inline std::uint32_t Le16(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0] | p[1] << 8);
}

// The order the PS2 stores 8-bit pictures in, as tools/areapack/tim2.py reads it. Per column parity.
inline const std::array<std::array<int, 64>, 2> &InverseLut() {
    static const int lut[128] = {
        0,  36, 8,  44, 1,  37, 9,  45, 2,  38, 10, 46, 3,  39, 11, 47, 4,  32, 12, 40, 5,  33, 13, 41, 6,  34, 14, 42, 7,  35, 15, 43,
        16, 52, 24, 60, 17, 53, 25, 61, 18, 54, 26, 62, 19, 55, 27, 63, 20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51, 31, 59,
        4,  32, 12, 40, 5,  33, 13, 41, 6,  34, 14, 42, 7,  35, 15, 43, 0,  36, 8,  44, 1,  37, 9,  45, 2,  38, 10, 46, 3,  39, 11, 47,
        20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51, 31, 59, 16, 52, 24, 60, 17, 53, 25, 61, 18, 54, 26, 62, 19, 55, 27, 63};
    static const std::array<std::array<int, 64>, 2> inverse = [] {
        std::array<std::array<int, 64>, 2> t{};
        for (int parity = 0; parity < 2; parity++) {
            for (int m = 0; m < 64; m++) {
                t[parity][lut[parity * 64 + m]] = m;
            }
        }
        return t;
    }();
    return inverse;
}

inline std::vector<std::uint8_t> Unswizzle8(std::uint32_t w, std::uint32_t h, std::span<const std::uint8_t> data) {
    const auto &inverse = InverseLut();
    const long  px = ((static_cast<long>(w) - 1) >> 7) + 1;
    const long  py = ((static_cast<long>(h) - 1) >> 6) + 1;
    const long  pw = px == 1 ? w : 128;
    const long  rb = px == 1 ? static_cast<long>(w) * 2 : 256;
    const long  rc = py == 1 ? static_cast<long>(h) >> 1 : 32;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(w) * h, 0);
    for (long y = 0; y < static_cast<long>(h); y++) {
        for (long x = 0; x < static_cast<long>(w); x++) {
            long page_x = x / pw;
            long page_y = y >> 6;
            long lx = x - page_x * pw;
            long ly = y & 63;
            long bx = lx >> 4;
            long by = ly >> 4;
            long src = (ly & 15) * 16 + (lx & 15);
            long col = src >> 6;
            long position = col * 64 + inverse[col & 1][src & 63];
            long prow = by * 8 + (position >> 5);
            long pcol = bx * 32 + (position & 31);
            if (prow >= rc || pcol >= rb) {
                continue;
            }
            long offset = page_y * px * rb * rc + rb * page_x + prow * rb * px + pcol;
            offset = std::min<long>(offset, static_cast<long>(data.size()) - 1);
            if (offset >= 0) {
                out[static_cast<std::size_t>(y) * w + x] = data[offset];
            }
        }
    }
    return out;
}

// `count` colours of the CLUT kind (3: RGBA, 2: RGB, else ABGR1555) as RGBA; alpha 0..128 becomes 0..255.
inline std::vector<std::uint8_t> Colours(std::span<const std::uint8_t> bytes, std::size_t count, int kind) {
    std::vector<std::uint8_t> out(count * 4, 0);
    for (std::size_t i = 0; i < count; i++) {
        std::uint8_t *c = &out[i * 4];
        int            a;
        if (kind == 3) {
            std::memcpy(c, &bytes[i * 4], 3);
            a = bytes[i * 4 + 3];
        } else if (kind == 2) {
            std::memcpy(c, &bytes[i * 3], 3);
            a = 128;
        } else {
            unsigned v = bytes[i * 2] | bytes[i * 2 + 1] << 8;
            c[0] = static_cast<std::uint8_t>((v & 31) * 255 / 31);
            c[1] = static_cast<std::uint8_t>(((v >> 5) & 31) * 255 / 31);
            c[2] = static_cast<std::uint8_t>(((v >> 10) & 31) * 255 / 31);
            a = (v >> 15) ? 128 : 0;
        }
        c[3] = static_cast<std::uint8_t>(std::min(a * 2, 255));
    }
    return out;
}

} // namespace tim2_detail

/// The pictures of the TIM2 file at d[off..]. `swizzled` is true for an IM2 container's 8-bit pictures. Empty on
/// anything that is not a readable TIM2.
inline std::vector<Picture> DecodeTim2(std::span<const std::uint8_t> d, std::size_t off, bool swizzled) {
    using namespace tim2_detail;
    std::vector<Picture> result;
    if (off + 16 > d.size() || std::memcmp(d.data() + off, "TIM2", 4) != 0) {
        return result;
    }
    std::size_t count = Le16(d.data() + off + 6);
    std::size_t p = off + (d[off + 5] == 1 ? 0x80 : 0x10);
    for (std::size_t n = 0; n < count; n++) {
        if (p + 0x1C > d.size()) {
            break;
        }
        const std::uint8_t *h = d.data() + p;
        std::size_t clut_size = Le32(h + 4);
        std::size_t image_size = Le32(h + 8);
        std::size_t header_size = Le16(h + 12);
        std::size_t clut_colours = Le16(h + 14);
        int         clut_type = h[0x12];
        int         image_type = h[0x13];
        std::uint32_t w = Le16(h + 0x14);
        std::uint32_t ht = Le16(h + 0x16);
        if (w == 0 || ht == 0 || w > 4096 || ht > 4096 || p + header_size + image_size > d.size()) {
            break;
        }
        std::span<const std::uint8_t> image = d.subspan(p + header_size, image_size);
        std::size_t                   clut_at = p + header_size + image_size;
        std::span<const std::uint8_t> clut = d.subspan(clut_at, std::min(clut_size, d.size() - clut_at));
        Picture                       picture;
        picture.width = w;
        picture.height = ht;
        picture.rgba.assign(static_cast<std::size_t>(w) * ht * 4, 0);
        (void) clut_colours;
        if (image_type == 4 || image_type == 5) {
            std::size_t colours = image_type == 4 ? 16 : 256;
            int         kind = clut_type & 63;
            std::size_t per = kind == 3 ? 4 : kind == 2 ? 3 : 2;
            std::size_t have = std::min(colours, clut.size() / per);
            std::vector<std::uint8_t> palette = Colours(clut, have, kind);
            palette.resize(colours * 4, 0);
            if (image_type == 5 && have == 256) {
                std::vector<std::uint8_t> fixed = palette;
                for (std::size_t base = 0; base < 256; base += 32) {
                    std::memcpy(&fixed[(base + 8) * 4], &palette[(base + 16) * 4], 8 * 4);
                    std::memcpy(&fixed[(base + 16) * 4], &palette[(base + 8) * 4], 8 * 4);
                }
                palette = std::move(fixed);
            }
            std::size_t pixels = static_cast<std::size_t>(w) * ht;
            std::vector<std::uint8_t> index(pixels, 0);
            if (image_type == 4) {
                for (std::size_t i = 0; i < pixels && i / 2 < image.size(); i++) {
                    index[i] = (i & 1) ? image[i / 2] >> 4 : image[i / 2] & 15;
                }
            } else if (swizzled) {
                index = Unswizzle8(w, ht, image);
            } else {
                std::memcpy(index.data(), image.data(), std::min(pixels, image.size()));
            }
            for (std::size_t i = 0; i < pixels; i++) {
                std::memcpy(&picture.rgba[i * 4], &palette[index[i] * 4], 4);
            }
        } else if (image_type >= 1 && image_type <= 3) {
            std::size_t per = image_type == 3 ? 4 : image_type == 2 ? 3 : 2;
            std::size_t pixels = std::min(static_cast<std::size_t>(w) * ht, image.size() / per);
            std::vector<std::uint8_t> colours = Colours(image, pixels, image_type);
            std::memcpy(picture.rgba.data(), colours.data(), colours.size());
        } else {
            break;
        }
        result.push_back(std::move(picture));
        p += header_size + image_size + clut_size;
    }
    return result;
}

// -------------------------------------------------------------------------------------------------- areas

/// The area a file (a case-folded path under the data folder) belongs to: "d01".."d07", a town or event map's folder
/// name, or "" for a file every area shares.
inline std::string AreaOf(std::string_view path) {
    if (path.starts_with("dun/")) {
        std::vector<std::string_view> parts;
        for (std::size_t start = 0; start <= path.size();) {
            std::size_t end = path.find('/', start);
            if (end == std::string_view::npos) {
                end = path.size();
            }
            parts.push_back(path.substr(start, end - start));
            start = end + 1;
        }
        for (int i = 1; i <= 7; i++) {
            std::string tag = "d0" + std::to_string(i);
            for (std::size_t k = 0; k + 1 < parts.size(); k++) {
                if (parts[k] == tag) {
                    return tag;
                }
            }
            std::string_view file = parts.back();
            if (file.starts_with(tag) && !(file.size() > tag.size() && file[tag.size()] >= '0' && file[tag.size()] <= '9')) {
                return tag;
            }
        }
        return {};
    }
    if (path.starts_with("gedit/")) {
        std::string_view rest = path.substr(6);
        std::size_t      slash = rest.find('/');
        if (slash == std::string_view::npos) {
            return {};
        }
        std::string_view folder = rest.substr(0, slash);
        if (folder == "system" || folder == "interior") {
            return {};
        }
        return std::string(folder);
    }
    return {};
}

/// The language folder a file is in (a_jpn, a_usa, a_eng, a_fre, a_ger, a_ita, a_spa), or "". The game reads the
/// folder of the language it is set to, so these pictures are the language's own.
inline std::string LanguageOf(std::string_view path) {
    for (std::string_view tag : {"a_jpn", "a_usa", "a_eng", "a_fre", "a_ger", "a_ita", "a_spa"}) {
        std::string key = "/" + std::string(tag) + "/";
        if (path.find(key) != std::string_view::npos || path.starts_with(key.substr(1))) {
            return std::string(tag);
        }
    }
    return {};
}

// -------------------------------------------------------------------------------------------------- scan

struct TextureFound {
    std::string   file;   // path under the data folder
    std::size_t   offset; // of the picture's TIM2
    bool          swizzled = false;
    std::string   name;
    std::string   area;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t hash = 0;
};

inline std::uint64_t HashPicture(const Picture &picture) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&h](std::uint8_t b) {
        h ^= b;
        h *= 1099511628211ull;
    };
    for (std::uint32_t v : {picture.width, picture.height}) {
        for (int s = 0; s < 32; s += 8) {
            mix(static_cast<std::uint8_t>(v >> s));
        }
    }
    for (std::uint8_t b : picture.rgba) {
        mix(b);
    }
    return h;
}

inline bool SafeTextureName(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    for (char c : name) {
        if (c < 0x21 || c > 0x7E || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            return false;
        }
    }
    return name != "." && name != "..";
}

/// One picture of an IMG / IM2 bank: the game's own picture list (CTextureManager::EnterIMGFile). A bank is a 16-byte
/// header whose word at +4 counts the entries, then 48-byte entries: a 32-byte name and the offset, from the bank's
/// start, of the entry's TIM2 file. An IM2 bank's 8-bit pictures are swizzled.
struct BankEntry {
    std::size_t bank;  // offset of the bank in the file
    std::size_t index; // entry number
    std::string name;
    bool        swizzled;
    std::size_t tim2;  // offset of the picture's TIM2 file
};

/// Whether a bank starts at d[pos]: its signature, a sane count and every entry's offset pointing at a TIM2.
inline bool IsBank(std::span<const std::uint8_t> d, std::size_t pos) {
    if (pos + 16 > d.size() || (std::memcmp(d.data() + pos, "IMG", 3) != 0 && std::memcmp(d.data() + pos, "IM2", 3) != 0) ||
        d[pos + 3] != 0) {
        return false;
    }
    std::uint32_t count = Le32(d.data() + pos + 4);
    if (count == 0 || count > 4096 || pos + 16 + std::uint64_t(count) * 48 > d.size()) {
        return false;
    }
    for (std::uint32_t i = 0; i < count; i++) {
        const std::uint8_t *entry = d.data() + pos + 16 + i * 48;
        std::uint64_t       at = pos + std::uint64_t(Le32(entry + 32));
        if (at + 32 > d.size() || std::memcmp(d.data() + at, "TIM2", 4) != 0) {
            return false;
        }
    }
    return true;
}

/// Calls visit(entry) for each entry of each bank in the file's bytes.
template <class Visit>
inline void ScanBanks(std::span<const std::uint8_t> d, Visit &&visit) {
    for (std::size_t pos = 0; pos + 16 <= d.size(); pos += 16) {
        if (d[pos] != 'I' || !IsBank(d, pos)) {
            continue;
        }
        bool          swizzled = std::memcmp(d.data() + pos, "IM2", 3) == 0;
        std::uint32_t count = Le32(d.data() + pos + 4);
        for (std::uint32_t i = 0; i < count; i++) {
            const std::uint8_t *entry = d.data() + pos + 16 + i * 48;
            const char         *text = reinterpret_cast<const char *>(entry);
            visit(BankEntry{pos, i, std::string(text, strnlen(text, 32)), swizzled, pos + Le32(entry + 32)});
        }
        pos += 16 + std::size_t(count) * 48 - 16; // the entries are not banks; the pictures after them are scanned on
    }
}

struct TexturePlan {
    std::vector<TextureFound> found;
    std::size_t               unnamed = 0;  // pictures with no name to look them up by
    std::size_t               multiple = 0; // TIM2 files holding several pictures, not converted
};

inline TexturePlan ScanTextureFolder(const fs::path &data, std::FILE *log = nullptr) {
    TexturePlan plan;
    std::vector<std::string> files;
    for (const auto &entry : fs::recursive_directory_iterator(data)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string rel = FoldPath(entry.path().lexically_relative(data).generic_string());
        // Audio and text hold no pictures, and scanning them is slow.
        if (rel.starts_with("sound/") || rel.ends_with(".cfg") || rel.ends_with(".txt") || rel.ends_with(".json")) {
            continue;
        }
        files.push_back(rel);
    }
    std::sort(files.begin(), files.end());
    for (const std::string &rel : files) {
        std::vector<unsigned char> bytes;
        try {
            bytes = ReadFile(data / rel);
        } catch (const std::exception &) {
            continue;
        }
        std::string area = AreaOf(rel);
        if (area.empty()) {
            area = LanguageOf(rel); // a language's own pictures are kept apart like an area's
        }
        ScanBanks(bytes, [&](const BankEntry &entry) {
            std::vector<Picture> pictures = DecodeTim2(bytes, entry.tim2, entry.swizzled);
            if (pictures.empty()) {
                return; // a type the game skips, too
            }
            if (!SafeTextureName(entry.name)) {
                plan.unnamed++;
                return;
            }
            if (pictures.size() > 1) {
                plan.multiple++;
            }
            TextureFound f;
            f.file = rel;
            f.offset = entry.tim2;
            f.swizzled = entry.swizzled;
            f.name = entry.name;
            f.area = area;
            f.width = pictures[0].width;
            f.height = pictures[0].height;
            f.hash = HashPicture(pictures[0]);
            plan.found.push_back(std::move(f));
        });
    }
    if (log != nullptr) {
        std::fprintf(log, "textures: %zu named pictures in %zu files (%zu unnamed, %zu with extra pictures: first used)\n",
                     plan.found.size(), files.size(), plan.unnamed, plan.multiple);
    }
    return plan;
}

// -------------------------------------------------------------------------------------------------- write

struct TextureOutput {
    fs::path    path; // relative to the assets folder
    std::size_t found_index;
};

/// Decides which picture goes where: in textures/ the one most areas share (or the shared files' own), and in an
/// area's folder only a picture that differs from it.
inline std::vector<TextureOutput> PlanTextureOutputs(const TexturePlan &plan, std::vector<std::string> *conflicts = nullptr) {
    std::map<std::string, std::vector<std::size_t>> by_name;
    for (std::size_t i = 0; i < plan.found.size(); i++) {
        by_name[plan.found[i].name].push_back(i);
    }
    std::vector<TextureOutput> outputs;
    for (const auto &[name, indices] : by_name) {
        std::set<std::pair<std::uint32_t, std::uint32_t>> sizes;
        for (std::size_t i : indices) {
            sizes.insert({plan.found[i].width, plan.found[i].height});
        }
        for (const auto &[w, h] : sizes) {
            std::vector<std::size_t> group;
            for (std::size_t i : indices) {
                if (plan.found[i].width == w && plan.found[i].height == h) {
                    group.push_back(i);
                }
            }
            std::string stem = sizes.size() > 1 ? name + "_" + std::to_string(w) + "x" + std::to_string(h) : name;
            // The baseline: a shared file's picture, else the picture most areas have.
            std::map<std::uint64_t, int> votes;
            std::uint64_t                shared_hash = 0;
            bool                         has_shared = false;
            for (std::size_t i : group) {
                if (plan.found[i].area.empty()) {
                    if (!has_shared) {
                        shared_hash = plan.found[i].hash;
                        has_shared = true;
                    }
                } else {
                    votes[plan.found[i].hash]++;
                }
            }
            std::uint64_t baseline = shared_hash;
            if (!has_shared) {
                int best = -1;
                for (std::size_t i : group) { // first of the most common, in path order
                    int v = votes[plan.found[i].hash];
                    if (v > best) {
                        best = v;
                        baseline = plan.found[i].hash;
                    }
                }
            }
            std::set<std::string>              done_areas;
            std::map<std::string, std::size_t> kept;
            std::size_t                        first_shared = group[0];
            for (std::size_t i : group) {
                if (plan.found[i].area.empty()) {
                    first_shared = i;
                    break;
                }
            }
            bool wrote_baseline = false;
            for (std::size_t i : group) {
                const TextureFound &f = plan.found[i];
                if (f.hash == baseline) {
                    if (!wrote_baseline) {
                        outputs.push_back({fs::path("textures") / (stem + ".png"), i});
                        wrote_baseline = true;
                    }
                } else if (!f.area.empty()) {
                    if (done_areas.insert(f.area).second) {
                        kept[f.area] = i;
                        outputs.push_back({fs::path(f.area) / "textures" / (stem + ".png"), i});
                    } else if (conflicts != nullptr) {
                        conflicts->push_back(stem + " | " + f.area + " | kept: " + plan.found[kept[f.area]].file +
                                             " | not used: " + f.file);
                    }
                } else if (conflicts != nullptr) {
                    conflicts->push_back(stem + " | shared | kept: " + plan.found[first_shared].file +
                                         " | not used: " + f.file);
                }
            }
        }
    }
    return outputs;
}

struct TextureSummary {
    std::size_t scanned = 0;
    std::size_t written = 0;
    std::size_t kept = 0;
    std::size_t duplicates_saved = 0;
    std::uint64_t bytes = 0;
};

/// Converts the textures under `data` (an extracted disc) to PNGs under `assets`. A picture already there is left as
/// it is, so a rerun does not undo an artist's work; `overwrite` replaces them.
inline TextureSummary ConvertTextures(const fs::path &data, const fs::path &assets, std::FILE *log = nullptr,
                                      bool overwrite = false) {
    TexturePlan                plan = ScanTextureFolder(data, log);
    std::vector<std::string>   conflicts;
    std::vector<TextureOutput> outputs = PlanTextureOutputs(plan, &conflicts);
    TextureSummary             summary;
    summary.scanned = plan.found.size();
    summary.duplicates_saved = plan.found.size() - outputs.size();

    std::map<std::string, std::vector<const TextureOutput *>> by_file;
    for (const TextureOutput &o : outputs) {
        by_file[plan.found[o.found_index].file].push_back(&o);
    }
    for (const auto &[rel, list] : by_file) {
        std::vector<unsigned char> bytes = ReadFile(data / rel);
        for (const TextureOutput *o : list) {
            const TextureFound &f = plan.found[o->found_index];
            fs::path            target = assets / o->path;
            std::error_code     error;
            if (!overwrite && fs::exists(target, error)) {
                summary.kept++;
                continue;
            }
            std::vector<Picture> pictures = DecodeTim2(bytes, f.offset, f.swizzled);
            if (pictures.empty()) {
                continue;
            }
            std::vector<std::uint8_t> png = EncodePng(pictures[0].rgba.data(), pictures[0].width, pictures[0].height);
            WriteFile(target, png);
            summary.written++;
            summary.bytes += png.size();
        }
    }
    // A name two files of one area (or two shared files) give different pictures to cannot be told apart by name: the
    // first file's is used and the rest are listed here.
    {
        std::string text = "# name | area | kept | not used\n";
        for (const std::string &line : conflicts) {
            text += line + "\n";
        }
        WriteFile(assets / "_conflicts.txt", std::span<const unsigned char>(reinterpret_cast<const unsigned char *>(text.data()), text.size()));
    }
    if (log != nullptr) {
        std::fprintf(log, "textures: %zu names with different pictures in one place, listed in _conflicts.txt\n", conflicts.size());
        std::fprintf(log, "textures: wrote %zu pictures (%llu bytes), kept %zu already present, %zu duplicates not written\n",
                     summary.written, static_cast<unsigned long long>(summary.bytes), summary.kept,
                     summary.duplicates_saved);
    }
    return summary;
}

} // namespace dcdata
