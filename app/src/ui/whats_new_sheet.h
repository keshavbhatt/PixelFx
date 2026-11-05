#pragma once

#include "ui/sheet.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QTextBrowser;

namespace pixelfx::ui {

/// What's new: the running version's section of the bundled changelog,
/// written for people. The first few items show, then "The full list of changes" shows them all.
/// A Version list reads older sections; it is hidden while there is only one release.
class WhatsNewSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WhatsNewSheet)

public:
    /// Items shown before "The full list of changes".
    static constexpr int kHighlights = 5;

    /// `markdown` is the changelog; `version` the section to show first.
    WhatsNewSheet(const QString& markdown, const QString& version, bool showAfterUpdates,
                  QWidget* parent = nullptr);
    ~WhatsNewSheet() override;

    /// Shows the section of `version`.
    void showVersion(const QString& version);
    /// Shows every item of the section, not only the first few.
    void showAll();
    [[nodiscard]] bool showingAll() const { return m_all; }
    /// The check box "Show this after each update".
    [[nodiscard]] bool showAfterUpdates() const;
    /// The notes as shown, as HTML.
    [[nodiscard]] QString html() const;
    /// The bundled changelog (empty when missing).
    [[nodiscard]] static QString bundledChangelog();

private:
    void render();

    QString m_markdown;
    QString m_version;
    bool m_all = false;
    QComboBox* m_versions = nullptr;
    QTextBrowser* m_text = nullptr;
    QLabel* m_more = nullptr;
    QCheckBox* m_show = nullptr;
};

} // namespace pixelfx::ui
