#include "audio_retail.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>

#include "audio_retail_format.hpp"

namespace chronicle_audio {
namespace {
using Json = nlohmann::json;

// Only lives during CreateFromJsonCandidates. Samples are decoded into the official bank
// before this immutable in-memory mount is released; no generated files enter the data root.
class Samples final : public vfs::Mount {
public:
    std::vector<std::vector<std::uint8_t>> files;

    std::size_t Count() const override { return files.size(); }

    std::string Name(std::size_t i) const override { return std::to_string(i) + ".wav"; }

    std::string Display(std::size_t i) const override { return "retail-audio/" + Name(i); }

    std::optional<std::size_t> Find(std::string_view name) const override {
        for (std::size_t i = 0; i < files.size(); ++i) {
            if (Name(i) == name) {
                return i;
            }
        }
        return std::nullopt;
    }

    std::optional<std::uint64_t> Size(std::size_t i) const override {
        return i < files.size() ? std::optional<std::uint64_t>(files[i].size()) : std::nullopt;
    }

    bool Read(std::size_t i, std::vector<std::uint8_t> &out, std::uint64_t max) const override {
        if (i >= files.size() || files[i].size() > max) {
            return false;
        }
        out = files[i];
        return true;
    }

    bool ReadInto(std::size_t i, void *out, std::size_t size) const override {
        if (i >= files.size() || files[i].size() != size) {
            return false;
        }
        std::memcpy(out, files[i].data(), size);
        return true;
    }

    std::string Stamp(std::size_t i) const override { return Display(i); }

    std::vector<vfs::Member> List(std::string_view directory) const override {
        std::vector<vfs::Member> entries;
        if (directory.empty()) {
            for (std::size_t i = 0; i < files.size(); ++i) {
                entries.push_back({Name(i), false});
            }
        }
        return entries;
    }
};

std::vector<std::uint8_t> Wav(const retail::VagSample &sample, int rate) {
    std::vector<std::uint8_t> data;
    const auto                text = [&](std::string_view s) { data.insert(data.end(), s.begin(), s.end()); };
    const auto                number = [&](std::uint32_t value, int bytes) { for(int i=0; i<bytes; ++i){ data.push_back(value>>(8*i));
} };
    text("RIFF");
    number(36 + sample.pcm.size() * 2, 4);
    text("WAVEfmt ");
    number(16, 4);
    number(1, 2);
    number(1, 2);
    number(rate, 4);
    number(rate * 2, 4);
    number(2, 2);
    number(16, 2);
    text("data");
    number(sample.pcm.size() * 2, 4);
    for (auto value : sample.pcm) {
        number(static_cast<std::uint16_t>(value), 2);
    }
    return data;
}
} // namespace

std::shared_ptr<audio::Bank> CreateBank(std::span<const std::uint8_t> hd, std::span<const std::uint8_t> bd) {
    auto parsed = retail::HdBank::Parse(hd);
    if (!parsed) {
        return nullptr;
    }
    Json root = {
        {"schema_version", 1            },
        {"kind",           "bank"       },
        {"programs",       Json::array()},
        {"sample_sets",    Json::array()},
        {"samples",        Json::array()},
        {"vags",           Json::array()}
    };
    for (const auto &p : parsed->programs) {
        Json splits = Json::array();
        for (const auto &s : p.splits) {
            splits.push_back({
                {"sample_set", s.sample_set},
                {"key_low",    s.key_low   },
                {"key_high",   s.key_high  },
                {"bend_low",   s.bend_low  },
                {"bend_high",  s.bend_high },
                {"volume",     s.volume    },
                {"pan",        s.pan       },
                {"transpose",  s.transpose },
                {"detune",     s.detune    }
            });
        }
        root["programs"].push_back({
            {"present",   p.present  },
            {"volume",    p.volume   },
            {"pan",       p.pan      },
            {"transpose", p.transpose},
            {"detune",    p.detune   },
            {"splits",    splits     }
        });
    }
    for (const auto &s : parsed->sample_sets) {
        root["sample_sets"].push_back({
            {"vel_curve", s.vel_curve},
            {"vel_low",   s.vel_low  },
            {"vel_high",  s.vel_high },
            {"samples",   s.samples  }
        });
    }
    for (const auto &s : parsed->samples) {
        root["samples"].push_back({
            {"vag",       s.vag      },
            {"vel_low",   s.vel_low  },
            {"vel_high",  s.vel_high },
            {"base_note", s.base_note},
            {"detune",    s.detune   },
            {"pan",       s.pan      },
            {"volume",    s.volume   },
            {"adsr1",     s.adsr1    },
            {"adsr2",     s.adsr2    },
            {"spu_attr",  s.spu_attr }
        });
    }
    auto mount = std::make_shared<Samples>();
    for (const auto &info : parsed->vags) {
        const auto sample = info.offset < bd.size() ? retail::DecodeVag(bd.subspan(info.offset)) : retail::VagSample{};
        if (sample.pcm.empty() || info.rate == 0) {
            root["vags"].push_back(nullptr);
            continue;
        }
        const auto path = mount->Name(mount->files.size());
        mount->files.push_back(Wav(sample, info.rate));
        root["vags"].push_back({
            {"wav",        path             },
            {"loop_start", sample.loop_start},
            {"loop_end",   sample.pcm.size()},
            {"loops",      sample.loops     },
            {"attr",       info.attr        }
        });
    }
    return audio::Bank::CreateFromJsonCandidates(root.dump(), [mount](const std::filesystem::path &path) {
        return std::vector<vfs::Ref>{vfs::Find(mount, path.generic_string())};
    });
}

std::shared_ptr<audio::SqFile> CreateSequence(std::span<const std::uint8_t> sq) {
    const auto parsed = retail::SqFile::Create(sq);
    if (!parsed) {
        return nullptr;
    }
    Json root = {
        {"schema_version", 1            },
        {"kind",           "sequence"   },
        {"songs",          Json::array()}
    };
    for (const auto &song : parsed->Songs()) {
        retail::SqReader reader(parsed->Events(song));
        Json             events = Json::array();
        while (true) {
            auto e = reader.Next();
            Json j = {
                {"delta", e.delta}
            };
            switch (e.kind) {
                case retail::SqEventKind::Channel:
                    j["kind"] = "channel";
                    j["status"] = e.status;
                    j["data"] = {e.data[0], e.data[1]};
                    break;
                case retail::SqEventKind::Meta:
                    j["kind"] = "meta";
                    j["meta"] = e.meta;
                    break;
                case retail::SqEventKind::SysEx:
                    j["kind"] = "sysex";
                    break;
                case retail::SqEventKind::End:
                    j["kind"] = "end";
                    break;
            }
            if (!e.payload.empty()) {
                j["payload"] = std::vector<std::uint8_t>(e.payload.begin(), e.payload.end());
            }
            events.push_back(std::move(j));
            if (e.kind == retail::SqEventKind::End) {
                break;
            }
        }
        root["songs"].push_back({
            {"division", song.division},
            {"events",   events       }
        });
    }
    return audio::SqFile::CreateFromJson(root.dump());
}
} // namespace chronicle_audio
