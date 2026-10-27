// Retail format decoding derived from TheMoonPeople/Kazarov 611182d.
// Copyright (c) 2026 TheMoonPeople, under the MIT license in tools/kazarov/LICENSE.
// No renderer or platform implementation is duplicated.
#include "app/icon_retail.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <span>
#include <string_view>

#include "data/data_read.hpp"
#include "platform/data_archive.hpp"
#include "platform/paths.hpp"

namespace dc::app {

using namespace dc::data;
using namespace dc::platform;

namespace fs = std::filesystem;

namespace {
constexpr int         kTextureSize = 128;
constexpr std::size_t kTexels = static_cast<std::size_t>(kTextureSize) * kTextureSize;

// Icon positions, normals and UVs are 4.12 fixed point.
constexpr float kFixedOne = 4096.0f;

struct Vertex {
    std::array<float, 3> pos;
    std::array<float, 3> normal;
    std::array<float, 2> uv;
};

struct Model {
    std::vector<Vertex>        vertices;
    std::vector<std::uint16_t> texture; // 128x128 RGB555, red in the low bits
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : data(data) {}

    bool Skip(std::size_t bytes) {
        if (bytes > data.size() - at) {
            return false;
        }
        at += bytes;
        return true;
    }

    template <typename T>
    bool Read(T &out) {
        if (sizeof(T) > data.size() - at) {
            return false;
        }
        std::memcpy(&out, data.subspan(at).data(), sizeof(T));
        at += sizeof(T);
        return true;
    }

    [[nodiscard]] std::size_t At() const { return at; }

private:
    std::span<const std::uint8_t> data;
    std::size_t                   at = 0;
};

// The PS2 icon layout: a header, every vertex with one position per animation shape, its normal, UV and
// colour, the animation frames and then a 128x128 texture, run-length compressed when bit 3 of the
// texture type is set.
bool Decode(std::span<const std::uint8_t> data, Model &model) {
    Reader        in(data);
    std::uint32_t magic = 0;
    std::uint32_t shapes = 0;
    std::uint32_t texture_type = 0;
    std::uint32_t vertex_count = 0;
    float         reserved = 0.0f;
    if (!in.Read(magic) || !in.Read(shapes) || !in.Read(texture_type) || !in.Read(reserved) ||
        !in.Read(vertex_count) || magic != 0x10000 || shapes == 0 || shapes > 16 || vertex_count == 0 ||
        vertex_count % 3 != 0 || vertex_count > data.size() / 16) {
        return false;
    }
    model.vertices.resize(vertex_count);
    for (Vertex &vertex : model.vertices) {
        std::array<std::int16_t, 4> position{};
        std::array<std::int16_t, 4> normal{};
        std::array<std::int16_t, 2> uv{};
        for (std::int16_t &component : position) {
            if (!in.Read(component)) {
                return false;
            }
        }
        if (!in.Skip(std::size_t{8} * (shapes - 1))) {
            return false;
        }
        for (std::int16_t &component : normal) {
            if (!in.Read(component)) {
                return false;
            }
        }
        if (!in.Read(uv[0]) || !in.Read(uv[1]) || !in.Skip(4)) {
            return false;
        }
        // The icon's y and z point down and away; flip them to a y-up, z-towards-the-viewer frame.
        vertex.pos[0] = position[0] / kFixedOne;
        vertex.pos[1] = -position[1] / kFixedOne;
        vertex.pos[2] = -position[2] / kFixedOne;
        vertex.normal[0] = normal[0] / kFixedOne;
        vertex.normal[1] = -normal[1] / kFixedOne;
        vertex.normal[2] = -normal[2] / kFixedOne;
        vertex.uv[0] = uv[0] / kFixedOne;
        vertex.uv[1] = uv[1] / kFixedOne;
    }
    std::uint32_t tag = 0;
    std::uint32_t frame_length = 0;
    std::uint32_t play_offset = 0;
    std::uint32_t frames = 0;
    float         speed = 0.0f;
    if (!in.Read(tag) || !in.Read(frame_length) || !in.Read(speed) || !in.Read(play_offset) || !in.Read(frames) ||
        frames > 1024) {
        return false;
    }
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        std::uint32_t shape = 0;
        std::uint32_t keys = 0;
        if (!in.Read(shape) || !in.Read(keys) || keys > 1024 || !in.Skip(8 * static_cast<std::size_t>(keys))) {
            return false;
        }
    }
    model.texture.clear();
    model.texture.reserve(kTexels);
    if (texture_type & 8) {
        std::uint32_t packed = 0;
        if (!in.Read(packed) || packed > data.size() - in.At()) {
            return false;
        }
        const std::size_t end = in.At() + packed;
        while (in.At() < end && model.texture.size() < kTexels) {
            std::uint16_t count = 0;
            std::uint16_t value = 0;
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
        std::uint16_t value = 0;
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

bool SameName(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char left, char right) {
        return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
    });
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
            if (SameName(it->path().filename().generic_string(), wanted)) {
                at = it->path();
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    out = std::move(at);
    return true;
}

// A PACK is a chain of 0x4C-byte entries (name[64], data offset, size, next entry), each offset relative
// to its own entry; an empty name ends it.
bool PackMember(const std::vector<std::uint8_t> &pack, std::string_view wanted, std::vector<std::uint8_t> &out) {
    std::size_t at = 0;
    while (at + 0x4C <= pack.size() && pack[at] != 0) {
        std::string_view name(reinterpret_cast<const char *>(&pack[at]), 64);
        name = name.substr(0, name.find('\0'));
        std::int32_t offset = 0;
        std::int32_t size = 0;
        std::int32_t next = 0;
        std::memcpy(&offset, &pack[at + 64], 4);
        std::memcpy(&size, &pack[at + 68], 4);
        std::memcpy(&next, &pack[at + 72], 4);
        if (SameName(name, wanted)) {
            if (offset < 0 || size <= 0 || at + offset > pack.size() ||
                static_cast<std::size_t>(size) > pack.size() - at - offset) {
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
constexpr std::array<std::uint32_t, 256> kCrcTable = [] {
    std::array<std::uint32_t, 256> entries{};
    for (std::uint32_t i = 0; i < 256; i++) {
        std::uint32_t value = i;
        for (int bit = 0; bit < 8; bit++) {
            value = (value & 1) ? 0xEDB88320u ^ (value >> 1) : value >> 1;
        }
        entries[i] = value;
    }
    return entries;
}();

std::uint32_t Crc32(std::span<const std::uint8_t> data, std::uint32_t crc = 0) {
    crc = ~crc;
    for (std::uint8_t byte : data) {
        crc = kCrcTable[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

void PutBigEndian(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void PutChunk(std::vector<std::uint8_t> &out, std::string_view type, const std::vector<std::uint8_t> &data) {
    PutBigEndian(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type.begin(), type.end());
    out.insert(out.end(), data.begin(), data.end());
    PutBigEndian(out, Crc32(std::span(out).subspan(start)));
}

std::vector<std::uint8_t> EncodePng(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> raw;
    raw.reserve((static_cast<std::size_t>(width) * 3 + 1) * height);
    for (std::uint32_t y = 0; y < height; y++) {
        raw.push_back(0);
        const auto row = rgba.subspan(static_cast<std::size_t>(y) * width * 4, static_cast<std::size_t>(width) * 4);
        for (std::uint32_t x = 0; x < width; x++) {
            const auto rgb = row.subspan(static_cast<std::size_t>(x) * 4, 3);
            raw.insert(raw.end(), rgb.begin(), rgb.end());
        }
    }

    // zlib stream of stored deflate blocks: no compressor, and screenshots are for tests.
    std::vector<std::uint8_t> zlib = {0x78, 0x01};
    std::size_t               pos = 0;
    do {
        const std::size_t length = std::min<std::size_t>(raw.size() - pos, 0xFFFF);
        const bool        last = pos + length == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<std::uint8_t>(length));
        zlib.push_back(static_cast<std::uint8_t>(length >> 8));
        zlib.push_back(static_cast<std::uint8_t>(~length));
        zlib.push_back(static_cast<std::uint8_t>(~length >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                    raw.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
    } while (pos < raw.size());
    std::uint32_t adler_a = 1;
    std::uint32_t adler_b = 0;
    for (std::uint8_t byte : raw) {
        adler_a = (adler_a + byte) % 65521;
        adler_b = (adler_b + adler_a) % 65521;
    }
    PutBigEndian(zlib, (adler_b << 16) | adler_a);

    std::vector<std::uint8_t> header;
    PutBigEndian(header, width);
    PutBigEndian(header, height);
    header.insert(header.end(), {8, 2, 0, 0, 0});

    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    PutChunk(png, "IHDR", header);
    PutChunk(png, "IDAT", zlib);
    PutChunk(png, "IEND", {});

    return png;
}

class IconFile final : public vfs::Mount {
public:
    std::vector<std::uint8_t> bytes;

    [[nodiscard]] std::size_t Count() const override { return 1; }

    [[nodiscard]] std::string Name(std::size_t /*index*/) const override { return "dkicon.glb"; }

    [[nodiscard]] std::string Display(std::size_t /*index*/) const override { return "retail-icon/dkicon.glb"; }

    [[nodiscard]] std::optional<std::size_t> Find(std::string_view name) const override {
        return name == "dkicon.glb" ? std::optional<std::size_t>(0) : std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint64_t> Size(std::size_t index) const override {
        return index == 0 ? std::optional<std::uint64_t>(bytes.size()) : std::nullopt;
    }

    bool Read(std::size_t index, std::vector<std::uint8_t> &out, std::uint64_t max) const override {
        if (index != 0 || bytes.size() > max) {
            return false;
        }
        out = bytes;
        return true;
    }

    bool ReadInto(std::size_t index, void *out, std::size_t size) const override {
        if (index != 0 || bytes.size() != size) {
            return false;
        }
        std::memcpy(out, bytes.data(), size);
        return true;
    }

    [[nodiscard]] std::string Stamp(std::size_t /*index*/) const override { return "retail-icon"; }

    [[nodiscard]] std::vector<vfs::Member> List(std::string_view directory) const override {
        if (!directory.empty()) {
            return {};
        }
        return {
            {.name = "dkicon.glb", .directory = false}
        };
    }
};
} // namespace

std::vector<std::uint8_t> RetailIconToGlb(std::span<const std::uint8_t> bytes) {
    Model model;
    if (!Decode(bytes, model)) {
        return {};
    }
    std::vector<std::uint8_t> rgba;
    for (const std::uint16_t texel : model.texture) {
        for (const int shift : {0, 5, 10}) {
            rgba.push_back(static_cast<std::uint8_t>(((texel >> shift) & 31) << 3));
        }
        rgba.push_back(255);
    }
    const auto                png = EncodePng(rgba, kTextureSize, kTextureSize);
    const auto                vertex_bytes = model.vertices.size() * sizeof(Vertex);
    std::vector<std::uint8_t> binary(vertex_bytes);
    std::memcpy(binary.data(), model.vertices.data(), vertex_bytes);
    binary.insert(binary.end(), png.begin(), png.end());
    using Json = nlohmann::json;
    Json accessors = Json::array();
    for (int attribute = 0; attribute < 3; ++attribute) {
        // Positions, normals and UVs are 12 bytes apart within a Vertex.
        Json accessor = {
            {"bufferView",    0                               },
            {"byteOffset",    attribute * 12                  },
            {"componentType", 5126                            },
            {"count",         model.vertices.size()           },
            {"type",          attribute == 2 ? "VEC2" : "VEC3"}
        };
        if (attribute == 0) {
            std::array<float, 3> lo = model.vertices[0].pos;
            std::array<float, 3> hi = lo;
            for (const Vertex &vertex : model.vertices) {
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    lo[axis] = std::min(lo[axis], vertex.pos[axis]);
                    hi[axis] = std::max(hi[axis], vertex.pos[axis]);
                }
            }
            accessor["min"] = lo;
            accessor["max"] = hi;
        }
        accessors.push_back(accessor);
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
    const auto                word = [&glb](std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            glb.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    };
    word(0x46546C67);
    word(2);
    word(static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + binary.size()));
    word(static_cast<std::uint32_t>(json.size()));
    word(0x4E4F534A);
    glb.insert(glb.end(), json.begin(), json.end());
    word(static_cast<std::uint32_t>(binary.size()));
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
    source.candidates = [root](const fs::path &relative) {
        fs::path                  path;
        std::vector<std::uint8_t> pack;
        std::vector<std::uint8_t> icon;
        std::vector<vfs::Ref>     result;
        const fs::path            pack_name = relative.parent_path();
        const bool                read = root == PathsDataRoot() && DataArchivePresent(root)
                                             ? DataReadReadFile(pack_name.generic_string(), pack)
                                             : FindFolded(root, pack_name, path) && ReadFile(path, pack);
        if (read && PackMember(pack, "dkicon.ico", icon)) {
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

} // namespace dc::app
