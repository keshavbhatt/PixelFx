#include "core/photo_details.h"

#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QObject>
#include <QRegularExpression>
#include <QSize>
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

namespace {

// The make as people say it: "Fujifilm" for "FUJIFILM", "Nikon" for "NIKON CORPORATION",
// "Olympus" for "OLYMPUS IMAGING CORP.". Makes not known here stay as written.
QString makeName(const QString& make)
{
    static const QRegularExpression kCompany(
        uR"(\s*,?\s+(CORPORATION|CORP\.?|CO\.?,?\s*LTD\.?|COMPANY,?\s*LTD\.?|IMAGING|CAMERA|AG|INC\.?|LTD\.?)$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    QString shorter = make;
    while (true) {
        const QString next = QString(shorter).remove(kCompany).trimmed();
        if (next == shorter || next.isEmpty()) {
            break;
        }
        shorter = next;
    }
    static const QHash<QString, QString> kKnown = {
        {u"FUJIFILM"_s, u"Fujifilm"_s},
        {u"NIKON"_s, u"Nikon"_s},
        {u"SONY"_s, u"Sony"_s},
        {u"PENTAX"_s, u"Pentax"_s},
        {u"OLYMPUS"_s, u"Olympus"_s},
        {u"OM DIGITAL SOLUTIONS"_s, u"OM System"_s},
        {u"RICOH"_s, u"Ricoh"_s},
        {u"LEICA"_s, u"Leica"_s},
        {u"SAMSUNG"_s, u"Samsung"_s},
        {u"HUAWEI"_s, u"Huawei"_s},
        {u"XIAOMI"_s, u"Xiaomi"_s},
        {u"HASSELBLAD"_s, u"Hasselblad"_s},
        {u"SIGMA"_s, u"Sigma"_s},
        {u"KODAK"_s, u"Kodak"_s},
        {u"CASIO"_s, u"Casio"_s},
        {u"MINOLTA"_s, u"Minolta"_s},
        {u"KONICA MINOLTA"_s, u"Konica Minolta"_s},
        {u"GOPRO"_s, u"GoPro"_s},
    };
    return kKnown.value(shorter.toUpper(), shorter);
}

} // namespace

QString cameraName(const pfx::ExifSummary& exif)
{
    const QString make = makeName(QString::fromStdString(exif.make).trimmed());
    const QString model = QString::fromStdString(exif.model).trimmed();
    // Cameras often repeat the make in the model ("Canon" and "Canon EOS R5", "NIKON Z 6"): said
    // once, the way the make is said.
    const QString firstWord = make.section(u' ', 0, 0);
    if (!firstWord.isEmpty() && model.startsWith(firstWord, Qt::CaseInsensitive)) {
        return QString(make + model.mid(firstWord.size())).trimmed();
    }
    return QStringList{make, model}.join(u' ').trimmed();
}

QString exposureSettings(const pfx::ExifSummary& exif)
{
    QStringList parts;
    if (const QString s = shutter(exif.exposureTime); !s.isEmpty()) {
        parts << s;
    }
    if (exif.fNumber > 0.0) {
        parts << u"f/"_s + shortNumber(exif.fNumber);
    }
    if (exif.iso > 0) {
        parts << u"ISO %1"_s.arg(exif.iso);
    }
    return parts.join(u" · "_s);
}

QString focalLength(const pfx::ExifSummary& exif)
{
    return exif.focalLength > 0.0 ? u"%1 mm"_s.arg(std::lround(exif.focalLength)) : QString();
}

QDateTime takenAt(const pfx::ExifSummary& exif)
{
    return QDateTime::fromString(QString::fromStdString(exif.dateTaken).trimmed(), u"yyyy:MM:dd HH:mm:ss"_s);
}

QString takenText(const QDateTime& taken)
{
    if (!taken.isValid()) {
        return {};
    }
    const QLocale locale;
    //: When a photo was taken: "3 October 2026, 18:42".
    return QObject::tr("%1, %2").arg(locale.toString(taken.date(), u"d MMMM yyyy"_s),
                                     locale.toString(taken.time(), QLocale::ShortFormat));
}

QString formatName(const QString& loader, const QString& path, bool raw)
{
    QString format = loader.endsWith(u"load"_s) ? loader.chopped(4).toUpper() : loader;
    const QString suffix = QFileInfo(path).suffix().toUpper();
    if (format == u"HEIF"_s && !suffix.isEmpty()) {
        format = suffix; // one loader for HEIC, HEIF and AVIF: name the file's own kind
    } else if (raw) {
        format = u"RAW (%1)"_s.arg(suffix); // camera raw: say which maker's format
    }
    return format;
}

QString sizeWithMegapixels(const QSize& size)
{
    const double megapixels = static_cast<double>(size.width()) * size.height() / 1e6;
    const QString amount = megapixels < 10.0 ? QLocale().toString(megapixels, 'f', 1)
                                             : QLocale().toString(std::lround(megapixels));
    //: A photo's size: width x height (megapixels), "6000 x 4000 (24 MP)".
    return QObject::tr("%1 x %2 (%3 MP)").arg(size.width()).arg(size.height()).arg(amount);
}

namespace {

QString degrees(double value, const QString& positive, const QString& negative)
{
    return u"%1\u00B0 %2"_s.arg(QString::number(std::abs(value), 'f', 5), value < 0.0 ? negative : positive);
}

} // namespace

QString coordinatesText(double latitude, double longitude)
{
    //: Compass letters after coordinates: north, south, east, west.
    return degrees(latitude, QObject::tr("N"), QObject::tr("S")) + u", "_s +
           degrees(longitude, QObject::tr("E"), QObject::tr("W"));
}

QString coordinatesForCopy(double latitude, double longitude)
{
    return QString::number(latitude, 'f', 5) + u", "_s + QString::number(longitude, 'f', 5);
}

} // namespace pixelfx::core
