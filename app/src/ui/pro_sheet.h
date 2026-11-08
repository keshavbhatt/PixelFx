#pragma once

#include "core/licensing/pro_features.h"
#include "ui/sheet.h"

#include <QFrame>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QVBoxLayout;

namespace pixelfx::services {
class LicenseService;
} // namespace pixelfx::services

namespace pixelfx::ui {

/// The quiet "Pro" mark beside a paid control: an outline with muted text, never the accent.
/// `feature` names it for the tooltip and screen readers ("Watermarks").
[[nodiscard]] QLabel* proMark(const QString& feature, QWidget* parent);

/// A rounded box on the hover colour with a glyph, a bold line and a muted line, and room under
/// them for more (the trial's meter, the Export sheet's list): the shape Pro always takes.
class ProStatusBox : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProStatusBox)

public:
    explicit ProStatusBox(QWidget* parent = nullptr);
    ~ProStatusBox() override;

    void setGlyph(const QString& glyph, const QColor& color);
    void setTitle(const QString& title);
    void setText(const QString& text);
    /// Where extra rows go, under the text.
    [[nodiscard]] QVBoxLayout* extra() const { return m_extra; }

private:
    QLabel* m_glyph = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_text = nullptr;
    QVBoxLayout* m_extra = nullptr;
};

/// PixelFx Pro, from the main menu and from every prompt's What Pro adds: where things stand
/// (the trial and its days, Free, or Pro), Free and Pro side by side, the Account ID with Copy
/// and Check again, the Ktechpit Pass, Manage purchases and Choose a plan. Follows the licence
/// live, so Check again shows its answer in place.
class ProSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProSheet)

public:
    /// `source` is what opened it, sent on to the checkout.
    ProSheet(services::LicenseService& license, QWidget* parent = nullptr,
             core::licensing::Source source = core::licensing::Source::Account);
    ~ProSheet() override;

    /// The words of the status box, for tests.
    [[nodiscard]] QString statusTitle() const;
    [[nodiscard]] bool showsPlans() const;

private:
    void refresh();

    services::LicenseService& m_license;
    ProStatusBox* m_status = nullptr;
    QProgressBar* m_meter = nullptr;
    QWidget* m_plans = nullptr;
    QLineEdit* m_account = nullptr;
    QLabel* m_accountNote = nullptr;
    QWidget* m_pass = nullptr;
    QPushButton* m_choose = nullptr;
};

} // namespace pixelfx::ui
