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

// A healed spot (FEATURES E20, DOCS/presets.md group "heal"): the area around `x`, `y` is
// covered with the area around `sourceX`, `sourceY`, matched in tone to its surroundings.
// Positions are fractions of the upright photo's width and height before turns, straightening
// and the crop, so cropping later never moves a spot; `radius` is a fraction of the long edge.
// A brush stroke has two or more `stroke` points (fractions too, the first at x, y) and is
// healed along its length, the source moved by the same offset. `feather` and `opacity` are
// 0 to 100.
struct Spot
{
    double x = 0.5;
    double y = 0.5;
    double sourceX = 0.5;
    double sourceY = 0.5;
    double radius = 0.02;
    double feather = 50.0;
    double opacity = 100.0;
    std::vector<CurvePoint> stroke;
    bool operator==(const Spot&) const = default;
};
using Spots = std::vector<Spot>;

using Value = std::variant<double, bool, std::string, Curve, Spots>;

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
    [[nodiscard]] Spots spots(std::string_view key) const; // empty when absent

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
