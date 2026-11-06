#include "core/compress_job.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

bool CompressJob::operator==(const CompressJob& other) const
{
    return codec.jpeg == other.codec.jpeg && codec.jpegQuality == other.codec.jpegQuality &&
           codec.png == other.codec.png && codec.pngQuality == other.codec.pngQuality &&
           codec.removeExtras == other.codec.removeExtras && folder == other.folder &&
           nameTemplate == other.nameTemplate && replaceOriginals == other.replaceOriginals &&
           keepIfNotSmaller == other.keepIfNotSmaller;
}

QString defaultCompressFolder()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return (pictures.isEmpty() ? QDir::homePath() : pictures) + u"/PixelFx/Compressed"_s;
}

QString compressFileName(const QString& source, const CompressJob& job, const QDate& date,
                         const NameFacts& facts)
{
    ExportOptions naming;
    naming.nameTemplate = job.nameTemplate;
    naming.extension = QFileInfo(source).suffix();
    return exportFileName(source, naming, {}, date, facts);
}

QString compressTarget(const QString& source, const CompressJob& job, const QSet<QString>& ours,
                       const QDate& date, const NameFacts& facts, NameClaims* claims)
{
    const QFileInfo info(source);
    if (job.replaceOriginals) {
        return info.absoluteFilePath();
    }
    const QDir dir(job.folder.isEmpty() ? info.absolutePath() : job.folder);
    const QString fileName = compressFileName(source, job, date, facts);
    QString target = dir.absoluteFilePath(fileName);
    const auto taken = [&info, &ours, claims](const QString& path) {
        const QFileInfo t(path);
        if (t.absoluteFilePath() == info.absoluteFilePath() ||
            (t.exists() && !ours.contains(t.absoluteFilePath()))) {
            return true;
        }
        return claims != nullptr && !claims->claim(path); // another photo of this run has it
    };
    if (!taken(target)) {
        return target;
    }
    const QFileInfo name(fileName);
    for (int n = 2; n < 10000; ++n) {
        target = dir.absoluteFilePath(u"%1 (%2).%3"_s.arg(name.completeBaseName()).arg(n).arg(name.suffix()));
        if (!taken(target)) {
            return target;
        }
    }
    return {};
}

} // namespace pixelfx::core
