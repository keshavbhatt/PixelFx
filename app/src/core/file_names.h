#pragma once

#include <QString>

namespace pixelfx::core {

/// `name`, typed by a person, as the base of a file name on every system the app runs on: no
/// path separators or characters Windows refuses, no leading dot (a hidden file), no trailing
/// dot or space, not a name Windows keeps for a device (CON, NUL, COM1...), at most `longest`
/// characters and 240 bytes (file systems allow 255, the extension needs the rest), never half
/// a character. `fallback` when nothing is left.
[[nodiscard]] QString fileSafeName(const QString& name, const QString& fallback, qsizetype longest = 80);

} // namespace pixelfx::core
