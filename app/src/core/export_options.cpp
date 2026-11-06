#include "core/export_options.h"

#include "core/file_names.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

QString customSizeId(const CustomSize& size)
{
    return u"mine:"_s + size.name;
}

QSize frameSize(const ExportOptions& options, const QSize& photo)
{
    if (const SizePreset* preset = sizePreset(options.sizePreset)) {
        return exactSize(*preset, photo);
    }
    if (options.customSize.width() > 0 && options.customSize.height() > 0) {
        return options.customSize;
    }
    return {};
}

QSize outputSize(const ExportOptions& options, QSize photo)
{
    int longEdge = options.longEdge;
    if (const SizePreset* preset = sizePreset(options.sizePreset)) {
        longEdge = preset->longEdge;
    }
    if (const QSize frame = frameSize(options, photo); frame.isValid()) {
        return frame;
    }
    // One side of a custom size: the other follows the photo's shape.
    if (photo.width() > 0 && photo.height() > 0 &&
        (options.customSize.width() > 0 || options.customSize.height() > 0)) {
        const double scale = options.customSize.width() > 0
                                 ? static_cast<double>(options.customSize.width()) / photo.width()
                                 : static_cast<double>(options.customSize.height()) / photo.height();
        return {std::max(1, static_cast<int>(std::lround(photo.width() * scale))),
                std::max(1, static_cast<int>(std::lround(photo.height() * scale)))};
    }
    const int edge = std::max(photo.width(), photo.height());
    if (edge > 0 && longEdge > 0 && (edge > longEdge || !options.noEnlarge)) {
        const double scale = static_cast<double>(longEdge) / edge;
        photo = QSize(std::max(1, static_cast<int>(std::lround(photo.width() * scale))),
                      std::max(1, static_cast<int>(std::lround(photo.height() * scale))));
    }
    return photo;
}

QString ratioName(const QSize& size)
{
    if (size.width() <= 0 || size.height() <= 0) {
        return {};
    }
    const double r = static_cast<double>(size.width()) / size.height();
    static constexpr std::array<std::pair<int, int>, 8> kCommon = {
        {{1, 1}, {5, 4}, {4, 3}, {3, 2}, {16, 10}, {16, 9}, {2, 1}, {21, 9}}};
    for (const auto& [a, b] : kCommon) {
        if (std::abs(r - (static_cast<double>(a) / b)) < 0.02 * r) {
            return u"%1:%2"_s.arg(a).arg(b);
        }
        if (std::abs(r - (static_cast<double>(b) / a)) < 0.02 * r) {
            return u"%1:%2"_s.arg(b).arg(a);
        }
    }
    return u"%1:1"_s.arg(r, 0, 'f', 2);
}

QString sizeDetail(const SizePreset& preset, const QSize& photo)
{
    if (preset.longEdge > 0) {
        //: A size list row: "2048 long edge".
        return QObject::tr("%1 long edge").arg(preset.longEdge);
    }
    const QSize exact = exactSize(preset, photo.isValid() ? photo : QSize(preset.width, preset.height));
    const QString pixels = u"%1 x %2"_s.arg(exact.width()).arg(exact.height());
    // Prints are named by their paper; their shape goes without saying.
    return preset.followsPhoto ? pixels : u"%1 \u00B7 %2"_s.arg(pixels, ratioName(exact));
}

QString sizeDetail(const CustomSize& size)
{
    const QSize s = size.size;
    if (s.width() > 0 && s.height() > 0) {
        return u"%1 x %2 \u00B7 %3"_s.arg(s.width()).arg(s.height()).arg(ratioName(s));
    }
    //: A custom size with one side: "2000 wide", "1500 high".
    return s.width() > 0 ? QObject::tr("%1 wide").arg(s.width()) : QObject::tr("%1 high").arg(s.height());
}

NameFacts nameFacts(const pfx::Metadata& metadata)
{
    NameFacts facts;
    const pfx::ExifSummary exif = pfx::readExif(metadata.exif);
    // Exif writes "YYYY:MM:DD HH:MM:SS".
    facts.taken = QDate::fromString(QString::fromStdString(exif.dateTaken).left(10), u"yyyy:MM:dd"_s);
    facts.camera = QString::fromStdString(exif.model.empty() ? exif.make : exif.model).trimmed();
    return facts;
}

QStringList nameTokens()
{
    return {u"{name}"_s, u"{n}"_s, u"{taken}"_s, u"{date}"_s, u"{camera}"_s, u"{w}x{h}"_s, u"{preset}"_s};
}

bool namesDiffer(const QString& nameTemplate)
{
    return nameTemplate.contains(u"{name}"_s) || nameTemplate.contains(u"{n}"_s);
}

QString exportFileName(const QString& source, const ExportOptions& options, const QSize& outputSize,
                       const QDate& date, const NameFacts& facts)
{
    const QString base = QFileInfo(source).completeBaseName();
    QString name = options.nameTemplate;
    name.replace(u"{name}"_s, base);
    const auto width = static_cast<int>(QString::number(std::max(1, facts.count)).size());
    name.replace(u"{n}"_s, u"%1"_s.arg(facts.number, width, 10, QLatin1Char('0')));
    name.replace(u"{taken}"_s, (facts.taken.isValid() ? facts.taken : date).toString(u"yyyy-MM-dd"_s));
    name.replace(u"{camera}"_s, facts.camera);
    name.replace(u"{date}"_s, date.toString(u"yyyy-MM-dd"_s));
    name.replace(u"{w}"_s, QString::number(outputSize.width()));
    name.replace(u"{h}"_s, QString::number(outputSize.height()));
    name.replace(u"{preset}"_s,
                 options.recipeName.trimmed().isEmpty() ? u"edit"_s : options.recipeName.trimmed());
    constexpr qsizetype kLongest = 200; // a long template over a long name still fits
    return fileSafeName(name, fileSafeName(base, u"photo"_s, kLongest), kLongest) + u'.' + options.extension;
}

QString exportTarget(const QString& source, const ExportOptions& options, const QSize& outputSize,
                     const QDate& date, const NameFacts& facts)
{
    const QString folder = options.folder.isEmpty() ? QFileInfo(source).absolutePath() : options.folder;
    const QString fileName = exportFileName(source, options, outputSize, date, facts);
    const QDir dir(folder);
    QString target = dir.absoluteFilePath(fileName);
    if (!QFileInfo::exists(target)) {
        return target;
    }
    switch (options.ifExists) {
    case ExportOptions::IfExists::Overwrite:
        return target;
    case ExportOptions::IfExists::Skip:
        return {};
    case ExportOptions::IfExists::AddNumber:
        break;
    }
    const QFileInfo info(fileName);
    for (int n = 2; n < 10000; ++n) {
        target = dir.absoluteFilePath(u"%1 (%2).%3"_s.arg(info.completeBaseName()).arg(n).arg(info.suffix()));
        if (!QFileInfo::exists(target)) {
            return target;
        }
    }
    return {};
}

QString ifExistsKey(ExportOptions::IfExists value)
{
    switch (value) {
    case ExportOptions::IfExists::Overwrite:
        return u"overwrite"_s;
    case ExportOptions::IfExists::Skip:
        return u"skip"_s;
    case ExportOptions::IfExists::AddNumber:
        break;
    }
    return u"number"_s;
}

ExportOptions::IfExists ifExistsFromKey(const QString& key)
{
    if (key == u"overwrite"_s) {
        return ExportOptions::IfExists::Overwrite;
    }
    if (key == u"skip"_s) {
        return ExportOptions::IfExists::Skip;
    }
    return ExportOptions::IfExists::AddNumber;
}

namespace {

constexpr std::array<std::pair<pfx::OutputSharpen, const char*>, 3> kSharpenKeys = {{
    {pfx::OutputSharpen::None, "none"},
    {pfx::OutputSharpen::Screen, "screen"},
    {pfx::OutputSharpen::Print, "print"},
}};

constexpr std::array<std::pair<pfx::OutputSpace, const char*>, 4> kColorSpaceKeys = {{
    {pfx::OutputSpace::Srgb, "srgb"},
    {pfx::OutputSpace::DisplayP3, "display-p3"},
    {pfx::OutputSpace::AdobeRgb, "adobe-rgb"},
    {pfx::OutputSpace::Original, "original"},
}};

template <typename Value, std::size_t N>
QString keyOf(const std::array<std::pair<Value, const char*>, N>& keys, Value value)
{
    for (const auto& [v, k] : keys) {
        if (v == value) {
            return QString::fromLatin1(k);
        }
    }
    return QString::fromLatin1(keys.front().second);
}

template <typename Value, std::size_t N>
Value valueOf(const std::array<std::pair<Value, const char*>, N>& keys, const QString& key)
{
    for (const auto& [v, k] : keys) {
        if (key == QLatin1StringView(k)) {
            return v;
        }
    }
    return keys.front().first;
}

} // namespace

pfx::SaveOptions saveOptions(const ExportOptions& options, std::vector<std::uint8_t> originalProfile)
{
    pfx::SaveOptions save;
    save.quality = options.quality;
    save.bitsPerSample = options.bitsPerSample;
    save.keepMetadata = options.keepMetadata;
    save.keepLocation = options.keepLocation;
    save.dpi = options.dpi;
    save.space = options.colorSpace;
    save.originalProfile = std::move(originalProfile);
    return save;
}

QString sharpenKey(pfx::OutputSharpen value)
{
    return keyOf(kSharpenKeys, value);
}

pfx::OutputSharpen sharpenFromKey(const QString& key)
{
    return valueOf(kSharpenKeys, key);
}

QString colorSpaceKey(pfx::OutputSpace value)
{
    return keyOf(kColorSpaceKeys, value);
}

pfx::OutputSpace colorSpaceFromKey(const QString& key)
{
    return valueOf(kColorSpaceKeys, key);
}

const std::vector<SizePreset>& sizePresets()
{
    static const std::vector<SizePreset> list = {
        {"instagram-portrait", QT_TRANSLATE_NOOP("SizePresets", "Social"),
         QT_TRANSLATE_NOOP("SizePresets", "Instagram portrait"), 1080, 1350, 0, false},
        {"instagram-square", QT_TRANSLATE_NOOP("SizePresets", "Social"),
         QT_TRANSLATE_NOOP("SizePresets", "Instagram square"), 1080, 1080, 0, false},
        {"story", QT_TRANSLATE_NOOP("SizePresets", "Social"),
         QT_TRANSLATE_NOOP("SizePresets", "Instagram and Facebook story"), 1080, 1920, 0, false},
        {"facebook-post", QT_TRANSLATE_NOOP("SizePresets", "Social"),
         QT_TRANSLATE_NOOP("SizePresets", "Facebook post"), 0, 0, 2048, false},
        {"x-post", QT_TRANSLATE_NOOP("SizePresets", "Social"), QT_TRANSLATE_NOOP("SizePresets", "X post"),
         1600, 900, 0, false},
        {"web", QT_TRANSLATE_NOOP("SizePresets", "Screen"), QT_TRANSLATE_NOOP("SizePresets", "Web"), 0, 0,
         2048, false},
        {"email", QT_TRANSLATE_NOOP("SizePresets", "Screen"), QT_TRANSLATE_NOOP("SizePresets", "Email"), 0, 0,
         1600, false},
        {"wallpaper-4k", QT_TRANSLATE_NOOP("SizePresets", "Screen"),
         QT_TRANSLATE_NOOP("SizePresets", "4K wallpaper"), 3840, 2160, 0, false},
        {"print-10x15", QT_TRANSLATE_NOOP("SizePresets", "Print at 300 dpi"),
         QT_TRANSLATE_NOOP("SizePresets", "10 x 15 cm (4 x 6 in)"), 1800, 1200, 0, true},
        {"print-13x18", QT_TRANSLATE_NOOP("SizePresets", "Print at 300 dpi"),
         QT_TRANSLATE_NOOP("SizePresets", "13 x 18 cm (5 x 7 in)"), 2100, 1500, 0, true},
        {"print-20x25", QT_TRANSLATE_NOOP("SizePresets", "Print at 300 dpi"),
         QT_TRANSLATE_NOOP("SizePresets", "20 x 25 cm (8 x 10 in)"), 3000, 2400, 0, true},
    };
    return list;
}

const SizePreset* sizePreset(const QString& id)
{
    for (const SizePreset& p : sizePresets()) {
        if (id == QLatin1StringView(p.id)) {
            return &p;
        }
    }
    return nullptr;
}

QSize exactSize(const SizePreset& preset, const QSize& photo)
{
    if (preset.width <= 0 || preset.height <= 0) {
        return {};
    }
    QSize size(preset.width, preset.height);
    const bool photoPortrait = photo.height() > photo.width();
    const bool presetPortrait = preset.height > preset.width;
    if (preset.followsPhoto && photoPortrait != presetPortrait) {
        size.transpose();
    }
    return size;
}

} // namespace pixelfx::core
