#include "core/crash_handler.h"

#include <QDir>
#include <QFile>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <exception>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define PIXELFX_HAS_BACKTRACE 1
#endif
#endif
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define PIXELFX_HAS_DEMANGLE 1
#endif

using namespace Qt::StringLiterals;

namespace pixelfx::core::crash {

namespace {

// Filled before anything can crash, read by the handlers: fixed buffers, no allocation.
constexpr std::size_t kPathBytes = 4096;
constexpr std::size_t kPhotoBytes = 512;
#ifdef Q_OS_WIN
std::array<wchar_t, kPathBytes> crashFile{};
#else
std::array<char, kPathBytes> crashFile{};
#endif
std::array<char, kPhotoBytes> photoName{};
std::atomic<bool> crashing{false};

// Writes into a buffer the handler then writes out: digits by hand, no formatting library.
struct Line
{
    std::array<char, kPathBytes> text{};
    std::size_t used = 0;

    void add(const char* part)
    {
        for (std::size_t i = 0; part[i] != '\0' && used + 1 < text.size(); ++i) {
            text.at(used++) = part[i];
        }
    }
    void addNumber(unsigned long long value, unsigned base)
    {
        std::array<char, 24> digits{};
        std::size_t n = 0;
        do {
            const auto d = static_cast<unsigned>(value % base);
            digits.at(n++) = static_cast<char>(d < 10 ? '0' + d : 'A' + (d - 10));
            value /= base;
        } while (value != 0 && n < digits.size());
        while (n > 0 && used + 1 < text.size()) {
            text.at(used++) = digits.at(--n);
        }
    }
};

// The first lines of the crash file: the kind, the time and the photo.
Line header(const char* kind, unsigned long long code, bool hex)
{
    Line line;
    line.add("kind: ");
    line.add(kind);
    if (code != 0) {
        line.add(hex ? " 0x" : " ");
        line.addNumber(code, hex ? 16 : 10);
    }
    line.add("\ntime: ");
    line.addNumber(static_cast<unsigned long long>(std::time(nullptr)), 10);
    line.add("\nphoto: ");
    line.add(photoName.data());
    line.add("\nstack:\n");
    return line;
}

#ifdef Q_OS_WIN

void writeFile(const Line& line)
{
    const HANDLE file = CreateFileW(crashFile.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, line.text.data(), static_cast<DWORD>(line.used), &written, nullptr);
        CloseHandle(file);
    }
}

LONG WINAPI onException(EXCEPTION_POINTERS* info)
{
    if (!crashing.exchange(true)) {
        writeFile(header("exception", info->ExceptionRecord->ExceptionCode, true));
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

extern "C" void onAbort(int /*signal*/)
{
    if (!crashing.exchange(true)) {
        writeFile(header("abort", 0, false));
    }
}

#else

const char* signalName(int signal)
{
    switch (signal) {
    case SIGSEGV:
        return "SIGSEGV";
    case SIGBUS:
        return "SIGBUS";
    case SIGFPE:
        return "SIGFPE";
    case SIGILL:
        return "SIGILL";
    case SIGABRT:
        return "SIGABRT";
    default:
        return "signal";
    }
}

extern "C" void onSignal(int signal)
{
    if (!crashing.exchange(true)) {
        const int fd = ::open(crashFile.data(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            const Line line = header(signalName(signal), 0, false);
            [[maybe_unused]] const auto written = ::write(fd, line.text.data(), line.used);
#ifdef PIXELFX_HAS_BACKTRACE
            std::array<void*, 64> frames{};
            const int count = backtrace(frames.data(), static_cast<int>(frames.size()));
            backtrace_symbols_fd(frames.data(), count, fd);
#endif
            ::close(fd);
        }
    }
    // The default action now: a core dump where the system keeps them, and the exit code.
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

#endif

} // namespace

void install(const QString& folder)
{
    QDir().mkpath(folder);
    const QString file = QDir(folder).filePath(u"crash.txt"_s);
#ifdef Q_OS_WIN
    const std::wstring path = QDir::toNativeSeparators(file).toStdWString();
    std::copy_n(path.c_str(), std::min(path.size(), crashFile.size() - 1), crashFile.begin());
    SetUnhandledExceptionFilter(onException);
    std::signal(SIGABRT, onAbort);
#else
    const QByteArray path = QFile::encodeName(file);
    std::copy_n(path.constData(),
                std::min<std::size_t>(static_cast<std::size_t>(path.size()), crashFile.size() - 1),
                crashFile.begin());
#ifdef PIXELFX_HAS_BACKTRACE
    std::array<void*, 4> warm{};
    backtrace(warm.data(), static_cast<int>(warm.size())); // loads what it needs now, not mid-crash
#endif
    // A stack of its own, so a stack overflow can still be written down.
    static std::array<char, std::size_t{64} * 1024> altStack{};
    stack_t stack{};
    stack.ss_sp = altStack.data();
    stack.ss_size = altStack.size();
    sigaltstack(&stack, nullptr);
    struct sigaction action{};
    action.sa_handler = onSignal;
    action.sa_flags = static_cast<int>(SA_ONSTACK | SA_RESETHAND);
    sigemptyset(&action.sa_mask);
    for (const int signal : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT}) {
        sigaction(signal, &action, nullptr);
    }
#endif
    std::set_terminate([] { std::abort(); }); // an uncaught exception ends as an abort, written down
}

void setCurrentPhoto(const QString& name)
{
    const QByteArray utf8 = name.toUtf8();
    const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(utf8.size()), photoName.size() - 1);
    std::array<char, kPhotoBytes> next{};
    std::copy_n(utf8.constData(), n, next.begin());
    photoName = next;
}

QString defaultFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/crash"_s;
}

namespace {

// "libpixelfx_ui.so(_ZN7pixelfx2ui6Canvas5paintEv+0x2c) [0x7f...]" from a path: the library's
// name, the function demangled, no address.
QString frameLine(const QString& raw)
{
    static const QRegularExpression kFrame(
        u"^(?:.*[/\\\\])?([^/\\\\(\\s]+)\\(([^+)]*)(\\+0x[0-9a-fA-F]+)?\\)"_s);
    const QRegularExpressionMatch m = kFrame.match(raw.trimmed());
    if (!m.hasMatch()) {
        return raw.trimmed().section(u' ', 0, 0).section(QRegularExpression(u"[/\\\\]"_s), -1);
    }
    QString function = m.captured(2);
#ifdef PIXELFX_HAS_DEMANGLE
    if (!function.isEmpty()) {
        int status = 0;
        char* plain = abi::__cxa_demangle(function.toLatin1().constData(), nullptr, nullptr, &status);
        if (status == 0 && plain != nullptr) {
            function = QString::fromLatin1(plain);
        }
        std::free(plain); // NOLINT(cppcoreguidelines-no-malloc): __cxa_demangle allocates with malloc
    }
#endif
    return function.isEmpty() ? m.captured(1) : m.captured(1) + u": "_s + function;
}

QString kindText(const QString& written)
{
    const QString kind = written.section(u' ', 0, 0);
    if (kind == u"exception"_s) {
        return QObject::tr("an exception (%1)").arg(written.section(u' ', 1, 1));
    }
    if (kind == u"abort"_s || kind == u"SIGABRT"_s) {
        return QObject::tr("an abort");
    }
    return QObject::tr("a crash (%1)").arg(kind);
}

} // namespace

Report parse(const QString& text)
{
    Report out;
    bool inStack = false;
    for (const QString& line : text.split(u'\n', Qt::SkipEmptyParts)) {
        if (inStack) {
            out.stack << frameLine(line);
        } else if (line.startsWith(u"kind: "_s)) {
            out.kind = kindText(line.mid(6).trimmed());
        } else if (line.startsWith(u"time: "_s)) {
            bool ok = false;
            const qint64 seconds = line.mid(6).trimmed().toLongLong(&ok);
            out.when = ok ? QDateTime::fromSecsSinceEpoch(seconds) : QDateTime();
        } else if (line.startsWith(u"photo: "_s)) {
            out.photo = line.mid(7).trimmed();
        } else if (line.startsWith(u"stack:"_s)) {
            inStack = true;
        }
    }
    if (out.kind.isEmpty()) {
        out.kind = QObject::tr("a crash");
    }
    return out;
}

std::optional<Report> takeLastCrash(const QString& folder)
{
    const QDir dir(folder);
    QFile file(dir.filePath(u"crash.txt"_s));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QString text = QString::fromUtf8(file.readAll());
    file.close();
    // Kept once more beside it, so it can still be read, then never shown again.
    QFile::remove(dir.filePath(u"crash-previous.txt"_s));
    if (!file.rename(dir.filePath(u"crash-previous.txt"_s))) {
        file.remove();
    }
    return parse(text);
}

QString reportText(const Report& report, const QString& version, const QStringList& logBefore)
{
    QStringList lines;
    lines << QObject::tr("PixelFx %1 closed with %2.").arg(version, report.kind);
    if (report.when.isValid()) {
        lines << QObject::tr("When: %1").arg(report.when.toUTC().toString(Qt::ISODate));
    }
    if (!report.stack.isEmpty()) {
        lines << QString() << QObject::tr("Where:") << report.stack;
    }
    if (!logBefore.isEmpty()) {
        lines << QString() << QObject::tr("The log just before:") << logBefore;
    }
    return lines.join(u'\n');
}

} // namespace pixelfx::core::crash
