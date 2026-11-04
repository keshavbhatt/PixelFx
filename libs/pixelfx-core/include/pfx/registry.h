// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/settings.h"

#include <string>
#include <string_view>
#include <vector>

namespace pfx {

enum class ParamType
{
    Number,
    Boolean,
    Choice,
    Points, // a curve
};

// One adjustable parameter. `key` is its full dotted settings key. Spatial parameters are
// `relative`: a fraction of the image's long edge, never pixels (ADR-004).
struct ParamSpec
{
    std::string key;
    ParamType type = ParamType::Number;
    double minimum = 0.0;
    double maximum = 0.0;
    double defaultValue = 0.0;
    std::string unit; // "", "EV", "long edge"
    bool relative = false;
    std::vector<std::string> choices; // for Choice; the first is the default
};

// An operation of the pipeline, registered once (ADR-005). `stage` is its place in the fixed
// order of DOCS/presets.md section 5; `pointwise` operations can be baked into a LUT.
struct OpSpec
{
    std::string id;
    std::string group;
    int version = 1;
    int stage = 0;
    bool pointwise = true;
    std::vector<ParamSpec> params;
};

class Registry
{
public:
    [[nodiscard]] const std::vector<OpSpec>& ops() const { return m_ops; }
    [[nodiscard]] const OpSpec* op(std::string_view id) const;
    [[nodiscard]] const ParamSpec* param(std::string_view key) const;

    // Clamps out-of-range numbers, drops wrong types, keeps unknown keys (forward
    // compatibility). Returns one line per change, for the user-facing report.
    std::vector<std::string> normalize(Settings& settings) const;

    // The registry's identity curve and defaults, for UIs that show untouched controls.
    [[nodiscard]] static Value defaultValue(const ParamSpec& spec);

    explicit Registry(std::vector<OpSpec> ops);

private:
    std::vector<OpSpec> m_ops;
};

// The library's registry. Immutable after the first call.
[[nodiscard]] const Registry& registry();

} // namespace pfx
