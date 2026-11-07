#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

// When PixelFx dies of a fault, an abort or an unhandled exception, a handler writes what it can
// into `<folder>/crash.txt`: the kind of failure, the time, the call stack the system gives and
// the name of the photo on show. It writes only with calls that are safe while crashing; the
// next start reads the file (takeLastCrash).
namespace pixelfx::core::crash {

/// Installs the handlers. `folder` is made when missing.
void install(const QString& folder);
/// The photo on show, its file name only, for the notice on the next start; empty for none.
void setCurrentPhoto(const QString& name);

/// What the last run left when it crashed.
struct Report
{
    QString kind;      ///< "a crash (SIGSEGV)", "an abort", "an exception (0xC0000005)"
    QDateTime when;    ///< invalid when the file did not say
    QString photo;     ///< the photo on show, its name; empty when none
    QStringList stack; ///< one line per frame, library folders left out, names demangled
};

/// The crash file of the last run, read and moved aside so it is reported once; nullopt when the
/// last run did not crash.
[[nodiscard]] std::optional<Report> takeLastCrash(const QString& folder);
/// Reads a crash file's text. Pure, tested.
[[nodiscard]] Report parse(const QString& text);
/// What the report attaches: the kind, the time, the version and the stack (never the photo's
/// name), and `logBefore`, the log lines just before it closed.
[[nodiscard]] QString reportText(const Report& report, const QString& version, const QStringList& logBefore);
/// The folder for crash files: `<AppDataLocation>/crash`.
[[nodiscard]] QString defaultFolder();

} // namespace pixelfx::core::crash
