// SPDX-License-Identifier: Apache-2.0
#include "pfx/settings.h"

#include <algorithm>

namespace pfx {

std::string_view groupOf(std::string_view key)
{
    const auto dot = key.find('.');
    return dot == std::string_view::npos ? key : key.substr(0, dot);
}

void Settings::set(std::string key, Value value)
{
    m_values.insert_or_assign(std::move(key), std::move(value));
}

void Settings::erase(std::string_view key)
{
    if (const auto it = m_values.find(key); it != m_values.end()) {
        m_values.erase(it);
    }
}

bool Settings::contains(std::string_view key) const
{
    return m_values.contains(key);
}

const Value* Settings::find(std::string_view key) const
{
    const auto it = m_values.find(key);
    return it == m_values.end() ? nullptr : &it->second;
}

double Settings::number(std::string_view key, double fallback) const
{
    const Value* v = find(key);
    if (v == nullptr) {
        return fallback;
    }
    if (const auto* d = std::get_if<double>(v)) {
        return *d;
    }
    return fallback;
}

bool Settings::boolean(std::string_view key, bool fallback) const
{
    const Value* v = find(key);
    if (const auto* b = v == nullptr ? nullptr : std::get_if<bool>(v)) {
        return *b;
    }
    return fallback;
}

std::string Settings::text(std::string_view key, std::string_view fallback) const
{
    const Value* v = find(key);
    if (const auto* s = v == nullptr ? nullptr : std::get_if<std::string>(v)) {
        return *s;
    }
    return std::string(fallback);
}

Curve Settings::curve(std::string_view key) const
{
    const Value* v = find(key);
    if (const auto* c = v == nullptr ? nullptr : std::get_if<Curve>(v)) {
        return *c;
    }
    return {};
}

Spots Settings::spots(std::string_view key) const
{
    const Value* v = find(key);
    if (const auto* c = v == nullptr ? nullptr : std::get_if<Spots>(v)) {
        return *c;
    }
    return {};
}

std::vector<std::string> Settings::keys() const
{
    std::vector<std::string> out;
    out.reserve(m_values.size());
    for (const auto& [key, value] : m_values) {
        out.push_back(key);
    }
    return out;
}

std::vector<std::string> Settings::groups() const
{
    std::vector<std::string> out;
    for (const auto& [key, value] : m_values) {
        const std::string group(groupOf(key));
        if (out.empty() || out.back() != group) {
            out.push_back(group);
        }
    }
    const auto duplicates = std::ranges::unique(out);
    out.erase(duplicates.begin(), duplicates.end());
    return out;
}

Settings Settings::onlyGroups(const std::vector<std::string>& groups) const
{
    Settings out;
    for (const auto& [key, value] : m_values) {
        if (std::ranges::find(groups, groupOf(key)) != groups.end()) {
            out.set(key, value);
        }
    }
    return out;
}

Settings Settings::overlaidWith(const Settings& top) const
{
    Settings out = *this;
    for (const auto& [key, value] : top.m_values) {
        out.set(key, value);
    }
    return out;
}

} // namespace pfx
