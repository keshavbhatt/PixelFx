#include "core/export_options.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

QString exportFileName(const QString& source, const ExportOptions& options, const QSize& outputSize,
                       const QDate& date)
{
    const QString base = QFileInfo(source).completeBaseName();
    QString name = options.nameTemplate;
    name.replace(u"{name}"_s, base);
    name.replace(u"{date}"_s, date.toString(u"yyyy-MM-dd"_s));
    name.replace(u"{w}"_s, QString::number(outputSize.width()));
    name.replace(u"{h}"_s, QString::number(outputSize.height()));
    static const QRegularExpression kUnsafe(uR"([\\/:*?"<>|\x00-\x1f])"_s);
    name.replace(kUnsafe, u"-"_s);
    name = name.trimmed();
    if (name.isEmpty() || name == u"."_s || name == u".."_s) {
        name = base;
    }
    return name + u'.' + options.extension;
}

QString exportTarget(const QString& source, const ExportOptions& options, const QSize& outputSize,
                     const QDate& date)
{
    const QString folder = options.folder.isEmpty() ? QFileInfo(source).absolutePath() : options.folder;
    const QString fileName = exportFileName(source, options, outputSize, date);
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
