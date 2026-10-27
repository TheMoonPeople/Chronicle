#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <string_view>

/**
 * Gameplay documents (docs/ITEM_FRAMEWORK.md): JSON files under gameplay/ in a datapack, each an
 * object with the schema_version and kind ChronicleTwo's native documents use. A document that cannot
 * be used prints "config: <path>: <reason>" and the next pack's is tried.
 */
namespace dc::platform::native_config {

using Json = nlohmann::json;

/**
 * The schema version of every gameplay document.
 */
inline constexpr int kSchemaVersion = 2;

/**
 * Passes the documents at a path under gameplay/ to parse, highest-priority pack first, until one has
 * this schema version and kind and parse returns without throwing. Returns whether one did.
 */
bool Read(std::string_view relative, std::string_view kind, const std::function<void(const Json &)> &parse);

/**
 * Prints one diagnostic about a gameplay document.
 */
void Report(std::string_view path, std::string_view message);

/**
 * A required member of an object; throws std::runtime_error when it is missing.
 */
const Json &At(const Json &object, const char *key);

/**
 * A required member that is a 32-bit integer; throws std::runtime_error otherwise.
 */
int Int(const Json &object, const char *key);

/**
 * A finite number as a float; throws std::runtime_error otherwise.
 */
float Float(const Json &value);

/**
 * A required member that is an array; throws std::runtime_error otherwise.
 */
const Json &Array(const Json &object, const char *key);

} // namespace dc::platform::native_config
