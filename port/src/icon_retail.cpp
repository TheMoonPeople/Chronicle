// Retail format decoding derived from TheMoonPeople/Kazarov 611182d.
// Copyright (c) 2026 TheMoonPeople; see ../third_party/kazarov-retail-LICENSE.
// No renderer or platform implementation is duplicated.
#include "icon_retail.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
namespace fs = std::filesystem;

namespace {
constexpr int kTextureSize = 128;

struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
};

struct Model {
    std::vector<Vertex>        vertices;
    std::vector<std::uint16_t> texture; // 128x128 RGB555, red in the low bits
};

class Reader {
public:
    Reader(const std::uint8_t *data, std::size_t size) : data_(data), size_(size) {}

    bool Skip(std::size_t bytes) {
        if (bytes > size_ - at_) {
            return false;
        }
        at_ += bytes;
        return true;
    }

    template <typename T>
    bool Read(T &out) {
        if (sizeof(T) > size_ - at_) {
            return false;
        }
        std::memcpy(&out, data_ + at_, sizeof(T));
        at_ += sizeof(T);
        return true;
    }

    std::size_t At() const { return at_; }

private:
    const std::uint8_t *data_;
    std::size_t         size_;
    std::size_t         at_ = 0;
};

// The PS2 icon layout: a header, every vertex with one position per animation shape, its normal, UV and
// colour, the animation frames and then a 128x128 texture, run-length compressed when bit 3 of the
// texture type is set.
bool Decode(const std::uint8_t *data, std::size_t size, Model &model) {
    Reader        in(data, size);
    std::uint32_t magic, shapes, texture_type, vertex_count;
    float         reserved;
    if (!in.Read(magic) || !in.Read(shapes) || !in.Read(texture_type) || !in.Read(reserved) ||
        !in.Read(vertex_count) || magic != 0x10000 || shapes == 0 || shapes > 16 || vertex_count == 0 ||
        vertex_count % 3 != 0 || vertex_count > size / 16) {
        return false;
    }
    model.vertices.resize(vertex_count);
    for (Vertex &vertex : model.vertices) {
        std::int16_t position[4], normal[4], uv[2];
        for (int i = 0; i < 4; ++i) {
            if (!in.Read(position[i])) {
                return false;
            }
        }
        if (!in.Skip(8 * (shapes - 1))) {
            return false;
        }
        for (int i = 0; i < 4; ++i) {
            if (!in.Read(normal[i])) {
                return false;
            }
        }
        if (!in.Read(uv[0]) || !in.Read(uv[1]) || !in.Skip(4)) {
            return false;
        }
        // The icon's y and z point down and away; flip them to a y-up, z-towards-the-viewer frame.
        constexpr float kOne = 4096.0f;
        vertex.pos[0] = position[0] / kOne;
        vertex.pos[1] = -position[1] / kOne;
        vertex.pos[2] = -position[2] / kOne;
        vertex.normal[0] = normal[0] / kOne;
        vertex.normal[1] = -normal[1] / kOne;
        vertex.normal[2] = -normal[2] / kOne;
        vertex.uv[0] = uv[0] / kOne;
        vertex.uv[1] = uv[1] / kOne;
    }
    std::uint32_t tag, frame_length, play_offset, frames;
    float         speed;
    if (!in.Read(tag) || !in.Read(frame_length) || !in.Read(speed) || !in.Read(play_offset) || !in.Read(frames) ||
        frames > 1024) {
        return false;
    }
    for (std::uint32_t i = 0; i < frames; ++i) {
        std::uint32_t shape, keys;
        if (!in.Read(shape) || !in.Read(keys) || keys > 1024 || !in.Skip(8 * std::size_t(keys))) {
            return false;
        }
    }
    constexpr std::size_t kTexels = kTextureSize * kTextureSize;
    model.texture.clear();
    model.texture.reserve(kTexels);
    if (texture_type & 8) {
        std::uint32_t packed;
        if (!in.Read(packed) || packed > size - in.At()) {
            return false;
        }
        const std::size_t end = in.At() + packed;
        while (in.At() < end && model.texture.size() < kTexels) {
            std::uint16_t count, value;
            if (!in.Read(count)) {
                return false;
            }
            if (count & 0x8000) {
                for (int i = 0; i < 0x10000 - count; ++i) {
                    if (!in.Read(value)) {
                        return false;
                    }
                    model.texture.push_back(value);
                }
            } else {
                if (!in.Read(value)) {
                    return false;
                }
                model.texture.insert(model.texture.end(), count, value);
            }
        }
    } else {
        std::uint16_t value;
        while (model.texture.size() < kTexels && in.Read(value)) {
            model.texture.push_back(value);
        }
    }
    if (model.texture.size() < kTexels) {
        return false;
    }
    model.texture.resize(kTexels);
    return true;
}

bool ReadFile(const fs::path &path, std::vector<std::uint8_t> &out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool SameName(const char *a, const char *b) {
    for (; *a != 0 && *b != 0; ++a, ++b) {
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) {
            return false;
        }
    }
    return *a == *b;
}

// relative under root, each component matched without regard to case: extraction may fold the disc's names.
bool FindFolded(const fs::path &root, const fs::path &relative, fs::path &out) {
    fs::path at = root;
    for (const fs::path &part : relative) {
        std::error_code error;
        if (fs::exists(at / part, error)) {
            at /= part;
            continue;
        }
        const std::string wanted = part.generic_string();
        bool              found = false;
        for (fs::directory_iterator it(at, error), end; !error && it != end; it.increment(error)) {
            if (SameName(it->path().filename().generic_string().c_str(), wanted.c_str())) {
                at = it->path();
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    out = at;
    return true;
}

// A PACK is a chain of 0x4C-byte entries (name[64], data offset, size, next entry), each offset relative
// to its own entry; an empty name ends it.
bool PackMember(const std::vector<std::uint8_t> &pack, const char *wanted, std::vector<std::uint8_t> &out) {
    std::size_t at = 0;
    while (at + 0x4C <= pack.size() && pack[at] != 0) {
        char name[65] = {};
        std::memcpy(name, &pack[at], 64);
        std::int32_t offset, size, next;
        std::memcpy(&offset, &pack[at + 64], 4);
        std::memcpy(&size, &pack[at + 68], 4);
        std::memcpy(&next, &pack[at + 72], 4);
        if (SameName(name, wanted)) {
            if (offset < 0 || size <= 0 || at + offset > pack.size() || std::size_t(size) > pack.size() - at - offset) {
                return false;
            }
            out.assign(pack.begin() + at + offset, pack.begin() + at + offset + size);
            return true;
        }
        if (next <= 0) {
            break;
        }
        at += next;
    }
    return false;
}

// In-memory PNG encoding derived from upstream gfx/readback.cpp.
uint32_t Crc32(const uint8_t *data, size_t size, uint32_t crc = 0) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> entries{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t value = i;
            for (int bit = 0; bit < 8; bit++) {
                value = (value & 1) ? 0xEDB88320u ^ (value >> 1) : value >> 1;
            }
            entries[i] = value;
        }
        return entries;
    }();
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

void PutBigEndian(std::vector<uint8_t> &out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void PutChunk(std::vector<uint8_t> &out, const char type[4], const std::vector<uint8_t> &data) {
    PutBigEndian(out, static_cast<uint32_t>(data.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    PutBigEndian(out, Crc32(out.data() + start, out.size() - start));
}

std::vector<uint8_t> EncodePng(const uint8_t *rgba, uint32_t width, uint32_t height) {
    std::vector<uint8_t> raw;
    raw.reserve((static_cast<size_t>(width) * 3 + 1) * height);
    for (uint32_t y = 0; y < height; y++) {
        raw.push_back(0);
        const uint8_t *row = rgba + static_cast<size_t>(y) * width * 4;
        for (uint32_t x = 0; x < width; x++) {
            raw.insert(raw.end(), row + x * 4, row + x * 4 + 3);
        }
    }

    // zlib stream of stored deflate blocks: no compressor, and screenshots are for tests.
    std::vector<uint8_t> zlib = {0x78, 0x01};
    size_t               pos = 0;
    do {
        size_t length = std::min<size_t>(raw.size() - pos, 0xFFFF);
        bool   last = pos + length == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(length));
        zlib.push_back(static_cast<uint8_t>(length >> 8));
        zlib.push_back(static_cast<uint8_t>(~length));
        zlib.push_back(static_cast<uint8_t>(~length >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                    raw.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
    } while (pos < raw.size());
    uint32_t a = 1;
    uint32_t b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    PutBigEndian(zlib, (b << 16) | a);

    std::vector<uint8_t> header;
    PutBigEndian(header, width);
    PutBigEndian(header, height);
    header.insert(header.end(), {8, 2, 0, 0, 0});

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    PutChunk(png, "IHDR", header);
    PutChunk(png, "IDAT", zlib);
    PutChunk(png, "IEND", {});

    return png;
}

class IconFile final : public vfs::Mount {
public:
    std::vector<std::uint8_t> bytes;

    std::size_t Count() const override { return 1; }

    std::string Name(std::size_t) const override { return "dkicon.glb"; }

    std::string Display(std::size_t) const override { return "retail-icon/dkicon.glb"; }

    std::optional<std::size_t> Find(std::string_view name) const override {
        return name == "dkicon.glb" ? std::optional<std::size_t>(0) : std::nullopt;
    }

    std::optional<std::uint64_t> Size(std::size_t i) const override {
        return i == 0 ? std::optional<std::uint64_t>(bytes.size()) : std::nullopt;
    }

    bool Read(std::size_t i, std::vector<std::uint8_t> &out, std::uint64_t max) const override {
        if (i || bytes.size() > max) {
            return false;
        }
        out = bytes;
        return true;
    }

    bool ReadInto(std::size_t i, void *out, std::size_t size) const override {
        if (i || bytes.size() != size) {
            return false;
        }
        std::memcpy(out, bytes.data(), size);
        return true;
    }

    std::string Stamp(std::size_t) const override { return "retail-icon"; }

    std::vector<vfs::Member> List(std::string_view directory) const override {
        return directory.empty() ? std::vector<vfs::Member>{
                                       {"dkicon.glb", false}
        }
                                 : std::vector<vfs::Member>{};
    }
};
} // namespace

std::vector<std::uint8_t> RetailIconToGlb(std::span<const std::uint8_t> bytes) {
    Model model;
    if (!Decode(bytes.data(), bytes.size(), model)) {
        return {};
    }
    std::vector<std::uint8_t> rgba;
    for (auto texel : model.texture) {
        for (int shift : {0, 5, 10}) {
            rgba.push_back(((texel >> shift) & 31) << 3);
        }
        rgba.push_back(255);
    }
    const auto                png = EncodePng(rgba.data(), 128, 128);
    const auto                vertex_bytes = model.vertices.size() * sizeof(Vertex);
    std::vector<std::uint8_t> binary(vertex_bytes);
    std::memcpy(binary.data(), model.vertices.data(), vertex_bytes);
    binary.insert(binary.end(), png.begin(), png.end());
    using Json = nlohmann::json;
    Json accessors = Json::array();
    for (int attribute = 0; attribute < 3; ++attribute) {
        Json a = {
            {"bufferView",    0                                                                                 },
            {"byteOffset",    attribute == 0 ? 0 : attribute == 1 ? 12
                                                                                  : 24},
            {"componentType", 5126                                                                              },
            {"count",         model.vertices.size()                                                             },
            {"type",          attribute == 2 ? "VEC2" : "VEC3"                                                  }
        };
        if (attribute == 0) {
            std::array<float, 3> lo, hi;
            std::copy_n(model.vertices[0].pos, 3, lo.begin());
            hi = lo;
            for (const auto &v : model.vertices) {
                for (int j = 0; j < 3; ++j) {
                    lo[j] = std::min(lo[j], v.pos[j]);
                    hi[j] = std::max(hi[j], v.pos[j]);
                }
            }
            a["min"] = lo;
            a["max"] = hi;
        }
        accessors.push_back(a);
    }
    Json doc = {
        {"asset",       {{"version", "2.0"}}                                                                                                                                                                      },
        {"buffers",     Json::array({{{"byteLength", binary.size()}}})                                                                                                                                            },
        {"bufferViews", Json::array({{{"buffer", 0}, {"byteOffset", 0}, {"byteLength", vertex_bytes}, {"byteStride", sizeof(Vertex)}}, {{"buffer", 0}, {"byteOffset", vertex_bytes}, {"byteLength", png.size()}}})},
        {"accessors",   accessors                                                                                                                                                                                 },
        {"images",      Json::array({{{"bufferView", 1}, {"mimeType", "image/png"}}})                                                                                                                             },
        {"textures",    Json::array({{{"source", 0}}})                                                                                                                                                            },
        {"materials",   Json::array({{{"pbrMetallicRoughness", {{"baseColorTexture", {{"index", 0}}}}}}})                                                                                                         },
        {"meshes",      Json::array({{{"primitives", Json::array({{{"attributes", {{"POSITION", 0}, {"NORMAL", 1}, {"TEXCOORD_0", 2}}}, {"material", 0}}})}}})                                                    },
        {"nodes",       Json::array({{{"mesh", 0}}})                                                                                                                                                              },
        {"scenes",      Json::array({{{"nodes", {0}}}})                                                                                                                                                           },
        {"scene",       0                                                                                                                                                                                         }
    };
    auto json = doc.dump();
    while (json.size() % 4) {
        json.push_back(' ');
    }
    while (binary.size() % 4) {
        binary.push_back(0);
    }
    std::vector<std::uint8_t> glb;
    auto                      word = [&](std::uint32_t v) { for(int i=0;i<4;++i){ glb.push_back(v>>(8*i));
} };
    word(0x46546C67);
    word(2);
    word(12 + 8 + json.size() + 8 + binary.size());
    word(json.size());
    word(0x4E4F534A);
    glb.insert(glb.end(), json.begin(), json.end());
    word(binary.size());
    word(0x004E4942);
    glb.insert(glb.end(), binary.begin(), binary.end());
    return glb;
}

GameIconSource RetailIconSource(const fs::path &root) {
    GameIconSource source;
    for (const char *language : {"a_usa", "a_eng", "a_jpn", "a_fre", "a_ger", "a_ita", "a_spa"}) {
        source.packs.push_back(fs::path("commenu") / language / "savetex.pak");
    }
    source.member = "dkicon.glb";
    source.ico_file = L"darkcloud.ico";
    source.shortcut = L"Dark Cloud";
    source.candidates = [root](const fs::path &relative) {
        fs::path                  path;
        std::vector<std::uint8_t> pack, icon;
        std::vector<vfs::Ref>     result;
        if (FindFolded(root, relative.parent_path(), path) && ReadFile(path, pack) && PackMember(pack, "dkicon.ico", icon)) {
            auto file = std::make_shared<IconFile>();
            file->bytes = RetailIconToGlb(icon);
            if (!file->bytes.empty()) {
                result.emplace_back(file, 0);
            }
        }
        return result;
    };
    return source;
}
