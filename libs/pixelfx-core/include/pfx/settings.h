// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pfx {

struct CurvePoint
{
    double x = 0.0;
    double y = 0.0;
    bool operator==(const CurvePoint&) const = default;
};
using Curve = std::vector<CurvePoint>;

using Value = std::variant<double, bool, std::string, Curve>;

// The settings of a photo (a recipe) or part of them (a preset), keyed by dotted paths such
// as "light.exposure" or "detail.sharpen.amount". The first segment is the group, as in
// DOCS/presets.md section 2. A missing key means "the operation's default" in a recipe and
// "leave the photo's own value" in a preset. Values are in the units the registry declares.
class Settings
{
public:
    void set(std::string key, Value value);
    void erase(std::string_view key);
    [[nodiscard]] bool contains(std::string_view key) const;
    [[nodiscard]] const Value* find(std::string_view key) const;
    [[nodiscard]] double number(std::string_view key, double fallback) const;
    [[nodiscard]] bool boolean(std::string_view key, bool fallback) const;
    [[nodiscard]] std::string text(std::string_view key, std::string_view fallback) const;
    [[nodiscard]] Curve curve(std::string_view key) const; // empty when absent

    [[nodiscard]] bool isEmpty() const { return m_values.empty(); }
    [[nodiscard]] std::vector<std::string> keys() const;
    [[nodiscard]] std::vector<std::string> groups() const;

    // Only the keys whose group is in `groups` (what Save as preset does with the ticked groups).
    [[nodiscard]] Settings onlyGroups(const std::vector<std::string>& groups) const;
    // These settings with every key of `top` written over them (applying a preset).
    [[nodiscard]] Settings overlaidWith(const Settings& top) const;

    bool operator==(const Settings&) const = default;

private:
    std::map<std::string, Value, std::less<>> m_values;
};

[[nodiscard]] std::string_view groupOf(std::string_view key);

} // namespace pfx
