#include "core/describe.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace pixelfx::core {

namespace {

const char* const kContext = "Describe";

QString tr(const char* text)
{
    return QCoreApplication::translate(kContext, text);
}

struct Trait
{
    double strength; // how far past its threshold, to keep the strongest
    QString words;
};

void addIf(std::vector<Trait>& out, double value, double threshold, const char* words)
{
    if (value >= threshold) {
        out.push_back({value / threshold, tr(words)});
    }
}

void colourTraits(const pfx::Settings& s, std::vector<Trait>& out)
{
    const double temperature = s.number("color.wb.temperature", 0.0);
    addIf(out, temperature, 8.0, QT_TRANSLATE_NOOP("Describe", "warm"));
    addIf(out, -temperature, 8.0, QT_TRANSLATE_NOOP("Describe", "cool"));
    const double colour = std::max(s.number("color.saturation", 0.0), s.number("color.vibrance", 0.0) * 0.7);
    addIf(out, colour, 15.0, QT_TRANSLATE_NOOP("Describe", "rich colour"));
    addIf(out, -colour, 15.0, QT_TRANSLATE_NOOP("Describe", "muted colour"));
    for (const auto& [zone, word] :
         {std::pair{"shadows", QT_TRANSLATE_NOOP("Describe", "%1 shadows")},
          std::pair{"highlights", QT_TRANSLATE_NOOP("Describe", "%1 highlights")}}) {
        const std::string key = std::string("grading.") + zone;
        const double amount = s.number(key + ".amount", 0.0);
        if (amount >= 10.0) {
            out.push_back({amount / 10.0, tr(word).arg(hueName(s.number(key + ".hue", 0.0)))});
        }
    }
}

void toneTraits(const pfx::Settings& s, std::vector<Trait>& out)
{
    const double contrast = s.number("light.contrast", 0.0);
    addIf(out, contrast, 15.0, QT_TRANSLATE_NOOP("Describe", "more contrast"));
    addIf(out, -contrast, 15.0, QT_TRANSLATE_NOOP("Describe", "soft contrast"));
    const double lift = std::max(s.number("light.blacks", 0.0), s.number("effects.fade", 0.0));
    addIf(out, lift, 12.0, QT_TRANSLATE_NOOP("Describe", "faded blacks"));
    addIf(out, s.number("light.exposure", 0.0), 0.3, QT_TRANSLATE_NOOP("Describe", "brighter"));
    addIf(out, -s.number("light.exposure", 0.0), 0.3, QT_TRANSLATE_NOOP("Describe", "darker"));
    addIf(out, std::max(s.number("detail.clarity", 0.0), s.number("detail.texture", 0.0)), 15.0,
          QT_TRANSLATE_NOOP("Describe", "crisp"));
}

void effectTraits(const pfx::Settings& s, std::vector<Trait>& out)
{
    const double grain = s.number("effects.grain.amount", 0.0);
    if (grain >= 50.0) {
        out.push_back({grain / 25.0, tr(QT_TRANSLATE_NOOP("Describe", "heavy grain"))});
    } else if (grain >= 25.0) {
        out.push_back({grain / 25.0, tr(QT_TRANSLATE_NOOP("Describe", "grain"))});
    } else if (grain >= 8.0) {
        out.push_back({grain / 25.0, tr(QT_TRANSLATE_NOOP("Describe", "fine grain"))});
    }
    addIf(out, -s.number("effects.vignette.amount", 0.0), 12.0,
          QT_TRANSLATE_NOOP("Describe", "dark corners"));
}

} // namespace

QString hueName(double degrees)
{
    struct Band
    {
        double upTo;
        const char* name;
    };
    static constexpr std::array<Band, 9> kBands = {{
        {15, QT_TRANSLATE_NOOP("Describe", "red")},
        {45, QT_TRANSLATE_NOOP("Describe", "orange")},
        {70, QT_TRANSLATE_NOOP("Describe", "yellow")},
        {160, QT_TRANSLATE_NOOP("Describe", "green")},
        {210, QT_TRANSLATE_NOOP("Describe", "teal")},
        {255, QT_TRANSLATE_NOOP("Describe", "blue")},
        {290, QT_TRANSLATE_NOOP("Describe", "purple")},
        {340, QT_TRANSLATE_NOOP("Describe", "magenta")},
        {361, QT_TRANSLATE_NOOP("Describe", "red")},
    }};
    const double h = std::fmod(std::fmod(degrees, 360.0) + 360.0, 360.0);
    for (const Band& b : kBands) {
        if (h < b.upTo) {
            return tr(b.name);
        }
    }
    return tr(kBands.back().name);
}

QString describeSettings(const pfx::Settings& settings)
{
    std::vector<Trait> traits;
    if (settings.boolean("bw.enabled", false)) {
        traits.push_back({100.0, tr(QT_TRANSLATE_NOOP("Describe", "black and white"))});
    } else {
        colourTraits(settings, traits);
    }
    toneTraits(settings, traits);
    effectTraits(settings, traits);
    std::ranges::stable_sort(traits, [](const Trait& a, const Trait& b) { return a.strength > b.strength; });
    QStringList words;
    for (std::size_t i = 0; i < traits.size() && i < 4; ++i) {
        words << traits[i].words;
    }
    QString text = words.join(tr(QT_TRANSLATE_NOOP("Describe", ", ")));
    if (!text.isEmpty()) {
        text[0] = text[0].toUpper();
    }
    return text;
}

} // namespace pixelfx::core
