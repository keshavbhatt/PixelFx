// SPDX-License-Identifier: Apache-2.0
#include "ops/ops.h"
#include "pfx/registry.h"

#include <doctest/doctest.h>
#include <set>
#include <string>

TEST_CASE("every registered operation is implemented, and every implementation is registered")
{
    std::set<std::string> registered;
    for (const auto& op : pfx::registry().ops()) {
        registered.insert(op.id);
    }
    std::set<std::string> implemented;
    for (const auto id : pfx::detail::implementedOpIds()) {
        implemented.insert(std::string(id));
    }
    CHECK(registered == implemented);
}

TEST_CASE("operations are in stage order and parameter keys are unique and in their op's group")
{
    std::set<std::string> keys;
    int lastStage = -1;
    for (const auto& op : pfx::registry().ops()) {
        CHECK(op.stage > lastStage);
        lastStage = op.stage;
        for (const auto& p : op.params) {
            CHECK(keys.insert(p.key).second);
            CHECK(std::string(pfx::groupOf(p.key)) == op.group);
            if (p.type == pfx::ParamType::Number) {
                CHECK(p.minimum <= p.defaultValue);
                CHECK(p.defaultValue <= p.maximum);
            }
            if (p.type == pfx::ParamType::Choice) {
                CHECK_FALSE(p.choices.empty());
            }
        }
    }
}

TEST_CASE("default values render as the identity")
{
    pfx::Settings defaults;
    for (const auto& op : pfx::registry().ops()) {
        for (const auto& p : op.params) {
            defaults.set(p.key, pfx::registry().defaultValue(p));
        }
    }
    for (const auto& impl :
         {pfx::detail::geometryActive, pfx::detail::exposureActive, pfx::detail::toneActive,
          pfx::detail::whiteBalanceActive, pfx::detail::vibranceSaturationActive, pfx::detail::curvesActive,
          pfx::detail::denoiseActive, pfx::detail::clarityActive, pfx::detail::sharpenActive}) {
        CHECK_FALSE(impl(defaults));
    }
}

TEST_CASE("a curve whose points meet once clamped is dropped, not kept broken")
{
    pfx::Settings s;
    s.set("curves.rgb", pfx::Curve{{0, 0}, {0.3, 0.2}, {5.7, 0.8}, {9, 1}});
    const auto lines = pfx::registry().normalize(s);
    CHECK_FALSE(s.contains("curves.rgb"));
    CHECK_FALSE(lines.empty());
}

TEST_CASE("looks may use every operation but those that move pixels or need the photo's facts")
{
    for (const pfx::OpSpec& op : pfx::registry().ops()) {
        CAPTURE(op.id);
        const bool notInLooks = op.id == "geometry" || op.id == "heal" || op.id == "lens_profile";
        CHECK(op.lookStep == !notInLooks);
    }
}
