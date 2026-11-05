#include "core/settings_clipboard.h"

#include <pfx/document.h>

namespace pixelfx::core {

namespace {

// What belongs to one photo and never travels: its crop and turns, and its healed spots.
bool ownToPhoto(const std::string& key)
{
    const std::string_view group = pfx::groupOf(key);
    return group == "geometry" || group == "heal";
}

pfx::Settings withoutGeometry(const pfx::Settings& settings)
{
    pfx::Settings out;
    for (const std::string& key : settings.keys()) {
        if (!ownToPhoto(key)) {
            out.set(key, *settings.find(key));
        }
    }
    return out;
}

constexpr auto kUnnamed = "Copied settings";

} // namespace

QString settingsToClipboard(const pfx::Settings& settings, const QString& from)
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Preset;
    doc.name = from.isEmpty() ? std::string(kUnnamed) : from.toStdString();
    doc.settings = withoutGeometry(settings);
    return QString::fromStdString(pfx::serializeDocument(doc));
}

std::optional<pfx::Settings> settingsFromClipboard(const QString& text)
{
    if (!text.trimmed().startsWith(u'{')) {
        return std::nullopt; // not JSON: no need to parse
    }
    const auto doc = pfx::parseDocument(text.toStdString());
    if (!doc ||
        (doc.value().kind != pfx::DocumentKind::Preset && doc.value().kind != pfx::DocumentKind::Recipe)) {
        return std::nullopt;
    }
    return withoutGeometry(doc.value().settings);
}

QString clipboardSource(const QString& text)
{
    if (!settingsFromClipboard(text)) {
        return {};
    }
    const std::string name = pfx::parseDocument(text.toStdString()).value().name;
    return name == kUnnamed ? QString() : QString::fromStdString(name);
}

pfx::Settings pasted(const pfx::Settings& current, const pfx::Settings& copied)
{
    pfx::Settings out = withoutGeometry(copied);
    for (const std::string& key : current.keys()) {
        if (ownToPhoto(key)) {
            out.set(key, *current.find(key));
        }
    }
    return out;
}

} // namespace pixelfx::core
