#include "core/file_names.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

QString fileSafeName(const QString& name, const QString& fallback)
{
    constexpr qsizetype kLongest = 80;
    static const QRegularExpression kUnsafe(u"[\\\\/:*?\"<>|\\x00-\\x1F]"_s);
    static const QRegularExpression kDevice(u"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"_s,
                                            QRegularExpression::CaseInsensitiveOption);
    QString s = name.trimmed();
    s.replace(kUnsafe, u"-"_s);
    while (s.startsWith(u'.')) {
        s.remove(0, 1);
    }
    if (s.size() > kLongest) {
        s.truncate(kLongest);
        if (s.back().isHighSurrogate()) {
            s.chop(1); // the first half of a character, an emoji for one
        }
    }
    while (s.endsWith(u'.') || s.endsWith(u' ')) {
        s.chop(1);
    }
    if (s.isEmpty()) {
        return fallback;
    }
    // Windows keeps these for devices, whatever follows the first dot ("con.pfxpreset").
    if (const QString device = s.section(u'.', 0, 0); kDevice.match(device).hasMatch()) {
        s.insert(device.size(), u'-');
    }
    return s;
}

} // namespace pixelfx::core
