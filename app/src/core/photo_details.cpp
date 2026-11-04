#include "core/photo_details.h"

#include <QLocale>
#include <QStringList>

#include <cmath>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

// 8, 2.8, 5.6: one decimal at most, none when it is whole.
QString shortNumber(double value)
{
    const double rounded = std::round(value * 10.0) / 10.0;
    return QString::number(rounded, 'f', std::abs(rounded - std::round(rounded)) < 1e-9 ? 0 : 1);
}

QString shutter(double seconds)
{
    if (seconds <= 0.0) {
        return {};
    }
    if (seconds < 0.3) { // 1/250 s; 1/3 s and longer read better as seconds below
        return u"1/%1 s"_s.arg(std::lround(1.0 / seconds));
    }
    return u"%1 s"_s.arg(shortNumber(seconds));
}

} // namespace

QString exposureLine(const pfx::ExifSummary& exif)
{
    QStringList parts;
    if (exif.iso > 0) {
        parts << u"ISO %1"_s.arg(exif.iso);
    }
    if (exif.fNumber > 0.0) {
        parts << u"f/"_s + shortNumber(exif.fNumber);
    }
    if (const QString s = shutter(exif.exposureTime); !s.isEmpty()) {
        parts << s;
    }
    if (exif.focalLength > 0.0) {
        parts << u"%1 mm"_s.arg(std::lround(exif.focalLength));
    }
    return parts.join(u" · "_s);
}

QString cameraName(const pfx::ExifSummary& exif)
{
    QString make = QString::fromStdString(exif.make).trimmed();
    const QString model = QString::fromStdString(exif.model).trimmed();
    // Cameras often repeat the make in the model ("Canon" and "Canon EOS R5").
    const QString firstWord = make.section(u' ', 0, 0);
    if (!firstWord.isEmpty() && model.startsWith(firstWord, Qt::CaseInsensitive)) {
        make.clear();
    }
    return QStringList{make, model}.join(u' ').trimmed();
}

} // namespace pixelfx::core
