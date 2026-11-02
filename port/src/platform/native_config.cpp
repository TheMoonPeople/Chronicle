#include "platform/native_config.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "data/data_read.hpp"
#include "platform/datapacks.hpp"

namespace dc::platform::native_config {

namespace {

void CheckKind(const Json &document, std::string_view kind) {
    if (!document.is_object()) {
        throw std::runtime_error("document is not an object");
    }
    const auto version = document.find("schema_version");
    if (version == document.end() || !version->is_number_integer()) {
        throw std::runtime_error("schema_version is missing");
    }
    if (*version != kSchemaVersion) {
        throw std::runtime_error(std::format("schema_version {} is not this version's {}", version->dump(), kSchemaVersion));
    }
    const auto found = document.find("kind");
    if (found == document.end() || !found->is_string() || found->get_ref<const std::string &>() != kind) {
        throw std::runtime_error(std::format("document is not a {} document", kind));
    }
}

} // namespace

bool Read(std::string_view relative, std::string_view kind, const std::function<void(const Json &)> &parse) {
    const std::string path = std::format("gameplay/{}", relative);
    auto              use = [&](std::string_view display, bool read, const std::vector<std::uint8_t> &bytes) {
        const Json document = read ? Json::parse(bytes, nullptr, false) : Json(Json::value_t::discarded);
        try {
            if (document.is_discarded()) {
                throw std::runtime_error("document is not JSON");
            }
            CheckKind(document, kind);
            parse(document);
            return true;
        } catch (const std::exception &exception) {
            Report(display, exception.what());
            return false;
        }
    };
    for (const vfs::Ref &file : datapacks::Find(path)) {
        std::vector<std::uint8_t> bytes;
        const bool                read = vfs::Read(file, bytes);
        if (use(file.Display(), read, bytes)) {
            return true;
        }
    }
    // The data's own, which dcdata writes from the documents tracked in port/gameplay. Only read once the
    // data is indexed, since indexing stops the program when there is no data.
    std::vector<std::uint8_t> bytes;
    return data::DataReadIndexed() && data::DataReadReadFile(path, bytes) && use(path, true, bytes);
}

void Report(std::string_view path, std::string_view message) {
    std::println(stderr, "config: {}: {}", path, message);
}

const Json &At(const Json &object, const char *key) {
    if (!object.is_object()) {
        throw std::runtime_error(std::format("expected an object holding {}", key));
    }
    const auto found = object.find(key);
    if (found == object.end()) {
        throw std::runtime_error(std::format("missing {}", key));
    }
    return *found;
}

int Int(const Json &object, const char *key) {
    const Json &value = At(object, key);
    if (!value.is_number_integer()) {
        throw std::runtime_error(std::format("{} is not an integer", key));
    }
    const bool fits = value.is_number_unsigned()
                          ? std::cmp_less_equal(value.get<std::uint64_t>(), std::numeric_limits<int>::max())
                          : std::in_range<int>(value.get<std::int64_t>());
    if (!fits) {
        throw std::runtime_error(std::format("{} is out of range", key));
    }
    return value.get<int>();
}

float Float(const Json &value) {
    if (!value.is_number()) {
        throw std::runtime_error("expected a number");
    }
    const auto number = static_cast<float>(value.get<double>());
    if (!std::isfinite(number)) {
        throw std::runtime_error("non-finite number");
    }
    return number;
}

const Json &Array(const Json &object, const char *key) {
    const Json &value = At(object, key);
    if (!value.is_array()) {
        throw std::runtime_error(std::format("{} is not an array", key));
    }
    return value;
}

} // namespace dc::platform::native_config
