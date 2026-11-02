#include "data/data_mount.hpp"

#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "data/data_read.hpp"

namespace dc::data {

namespace {

// Entries are the names looked up so far, so an index stays valid for the mount's lifetime as the
// mount requires; the data reader does the finding and reading.
class Mount final : public vfs::Mount {
public:
    std::size_t Count() const override {
        std::scoped_lock lock(mutex_);
        return names_.size();
    }

    std::string Name(std::size_t index) const override {
        std::scoped_lock lock(mutex_);
        return index < names_.size() ? names_[index] : std::string();
    }

    std::string Display(std::size_t index) const override { return std::format("data/{}", Name(index)); }

    std::optional<std::size_t> Find(std::string_view relative) const override {
        if (!vfs::SafeRelative(relative) || !DataFileExists(relative)) {
            return std::nullopt;
        }
        const std::string key = vfs::FoldKey(relative);
        std::scoped_lock  lock(mutex_);
        if (const auto found = indexes_.find(key); found != indexes_.end()) {
            return found->second;
        }
        names_.emplace_back(relative);
        return indexes_.emplace(key, names_.size() - 1).first->second;
    }

    std::optional<std::uint64_t> Size(std::size_t index) const override {
        const std::string name = Name(index);
        if (name.empty() || !DataFileExists(name)) {
            return std::nullopt;
        }
        return DataFileSize(name);
    }

    bool Read(std::size_t index, std::vector<std::uint8_t> &bytes, std::uint64_t max_bytes) const override {
        const std::optional<std::uint64_t> size = Size(index);
        return size && *size <= max_bytes && DataReadReadFile(Name(index), bytes);
    }

    bool ReadInto(std::size_t index, void *buffer, std::size_t size) const override {
        std::vector<std::uint8_t> bytes;
        if (!Read(index, bytes, size) || bytes.size() != size) {
            return false;
        }
        std::memcpy(buffer, bytes.data(), size);
        return true;
    }

    // The data is indexed afresh when it changes, so its generation names its contents.
    std::string Stamp(std::size_t index) const override {
        return std::format("{}:{}:{}", Name(index), Size(index).value_or(0), DataReadGeneration());
    }

    std::vector<vfs::Member> List(std::string_view directory) const override {
        std::string prefix(directory);
        if (!prefix.empty() && !prefix.ends_with('/')) {
            prefix += '/';
        }
        const std::string                     folded = vfs::FoldKey(prefix);
        std::vector<vfs::Member>              members;
        std::unordered_map<std::string, bool> seen;
        for (const std::string &path : DataReadListFiles()) {
            if (!vfs::FoldKey(path).starts_with(folded)) {
                continue;
            }
            const std::string_view rest = std::string_view(path).substr(prefix.size());
            const std::size_t      slash = rest.find('/');
            const std::string      name(rest.substr(0, slash));
            if (!name.empty() && seen.emplace(vfs::FoldKey(name), true).second) {
                members.push_back({.name = name, .directory = slash != std::string_view::npos});
            }
        }
        return members;
    }

private:
    mutable std::mutex                                   mutex_;
    mutable std::vector<std::string>                     names_;
    mutable std::unordered_map<std::string, std::size_t> indexes_;
};

} // namespace

std::shared_ptr<const vfs::Mount> DataMount() {
    // The data is indexed afresh when the player picks other data, so its mount is too.
    static std::mutex                        mutex;
    static std::shared_ptr<const vfs::Mount> mount;
    static std::uint64_t                     generation = 0;
    std::scoped_lock                         lock(mutex);
    if (mount == nullptr || generation != DataReadGeneration()) {
        mount = std::make_shared<Mount>();
        generation = DataReadGeneration();
    }
    return mount;
}

} // namespace dc::data
