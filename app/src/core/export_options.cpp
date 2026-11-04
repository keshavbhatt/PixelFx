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

} // namespace pixelfx::core
