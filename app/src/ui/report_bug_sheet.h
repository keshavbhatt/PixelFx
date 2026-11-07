#pragma once

#include "core/bug_report.h"
#include "core/crash_handler.h"
#include "ui/sheet.h"

#include <optional>

class QCheckBox;
class QLayout;
class QPlainTextEdit;
class QVBoxLayout;

namespace pixelfx::ui {

/// Report a bug: what happened in the person's words, and what they choose to attach (after a
/// crash, what PixelFx was doing; Diagnostics; the log's last lines with names left out), each
/// with Show for its exact text. Open the issue tracker fills in a new issue in the browser, or
/// puts a report too long for that on the clipboard; Copy report copies it. Nothing is sent from
/// here.
class ReportBugSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReportBugSheet)

public:
    /// `crash`: what the last run left when it crashed; attached and said at the top when given.
    explicit ReportBugSheet(const std::optional<core::crash::Report>& crash, QWidget* parent = nullptr);
    ~ReportBugSheet() override;

    void setWhatHappened(const QString& text);
    /// What would be sent: the text and what is ticked.
    [[nodiscard]] core::BugReport report() const;
    /// The attachments' check boxes, in order (for tests).
    [[nodiscard]] QList<QCheckBox*> attachments() const;

Q_SIGNALS:
    /// The report went on the clipboard to be pasted into the issue (too long to fill in).
    void copiedForPasting();

private:
    /// "What happened" over its field.
    QLayout* buildWhatHappened();
    /// The attachments, the crash's first when `crashed`.
    QLayout* buildAttached(bool crashed);
    /// A ticked attachment named `name`, `note` under it, and Show for `text`.
    QCheckBox* addAttachment(QVBoxLayout* column, const QString& name, const QString& note,
                             const QString& text);
    void openTracker();

    QPlainTextEdit* m_text = nullptr;
    QCheckBox* m_crash = nullptr;
    QCheckBox* m_diagnostics = nullptr;
    QCheckBox* m_log = nullptr;
    QString m_crashText;
    QString m_diagnosticsText;
    QStringList m_logLines;
};

} // namespace pixelfx::ui
