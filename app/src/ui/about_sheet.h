#pragma once

#include <QDialog>

class QButtonGroup;
class QListWidget;
class QPlainTextEdit;
class QStackedWidget;

namespace pixelfx::ui {

/// About PixelFx: the version and the ways on (What's new, the source, a bug
/// report), the open-source notices with every licence text, and diagnostics to paste into a
/// bug report.
class AboutSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AboutSheet)

public:
    enum Page
    {
        AboutPage,
        NoticesPage,
        DiagnosticsPage
    };

    explicit AboutSheet(QWidget* parent = nullptr);
    ~AboutSheet() override;

    void showPage(Page page);
    [[nodiscard]] Page page() const;
    /// The diagnostics text as shown (and copied).
    [[nodiscard]] QString diagnostics() const;
    /// Shows the notice of component `row` (in core::notices() order).
    void showNotice(int row);
    [[nodiscard]] QString noticeShown() const;

Q_SIGNALS:
    void whatsNewRequested();

private:
    QWidget* buildAboutPage();
    QWidget* buildNoticesPage();
    QWidget* buildDiagnosticsPage();

    QButtonGroup* m_tabs = nullptr;
    QStackedWidget* m_pages = nullptr;
    QListWidget* m_components = nullptr;
    QPlainTextEdit* m_notice = nullptr;
    QPlainTextEdit* m_diagnostics = nullptr;
};

} // namespace pixelfx::ui
