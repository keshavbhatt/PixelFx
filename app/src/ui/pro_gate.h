#pragma once

#include "core/licensing/pro_features.h"
#include "ui/sheet.h"

#include <QObject>
#include <QPointer>

class QLabel;
class QPushButton;

namespace pixelfx::services {
class LicenseService;
} // namespace pixelfx::services

namespace pixelfx::ui {

/// A Pro prompt: shown only when a paid feature is used on Free. It names the job in its title
/// ("Batch 47 photos with Pro"), says what Free allows and what Pro adds, and offers the free way
/// out when there is one, then Choose a plan. Bought already? Check again asks the server, and an
/// answer of Pro lets the job go ahead.
class ProPrompt : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProPrompt)

public:
    /// What the person chose; exec() returns one of these.
    enum Outcome
    {
        Closed = QDialog::Rejected,
        FreeWay = 2,     ///< the free way out ("Process the first 10")
        ChosePlan = 3,   ///< the checkout was opened
        WhatProAdds = 4, ///< the PixelFx Pro sheet is wanted
        NowPro = 5,      ///< Check again found a purchase
    };

    ProPrompt(services::LicenseService& license, core::licensing::Source source, const QString& title,
              const QString& text, QWidget* parent = nullptr);
    ~ProPrompt() override;

    /// The free way out's button ("Process the first 10"); none unless set.
    void setFreeWay(const QString& label);
    /// A second, muted line under the text (before "Bought already? Check again.").
    void setNote(const QString& note);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString text() const;
    [[nodiscard]] core::licensing::Source source() const { return m_source; }
    [[nodiscard]] QPushButton* freeWayButton() const { return m_freeWay; }

private:
    void refreshNote();

    services::LicenseService& m_license;
    core::licensing::Source m_source;
    QLabel* m_title = nullptr;
    QLabel* m_text = nullptr;
    QLabel* m_note = nullptr;
    QString m_noteText;
    QPushButton* m_freeWay = nullptr;
};

/// The one place the UI asks whether a Pro feature may run (FEATURES K2): with Pro or in the trial
/// everything goes; on Free a ProPrompt says why not and offers the way on. Without a licence (in
/// tests) everything goes. The checkout and the PixelFx Pro sheet open from here too.
class ProGate : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProGate)

public:
    /// The jobs that count photos.
    enum class Count
    {
        Batch,     ///< Process
        BatchRest, ///< Process the rest, after a run that did not reach every photo
        Export,    ///< Export with several photos selected
        Paste,     ///< paste settings, a preset or a look to the selected photos
    };

    ProGate(services::LicenseService* license, QWidget* window);
    ~ProGate() override;

    [[nodiscard]] services::LicenseService* license() const { return m_license; }
    /// Bought, or in the trial (or no licence at all).
    [[nodiscard]] bool hasPro() const;

    /// How many of `count` photos the job may handle now: all of them, the free number when the
    /// person takes the free way, or 0 when they did not go on. `what` names what a Paste puts
    /// on the photos ("Preset Warm") when it is not pasted settings.
    [[nodiscard]] int admitPhotos(Count job, int count, const QString& what = {});
    /// The prompt admitPhotos shows, not yet shown; the caller owns it. Needs a licence.
    [[nodiscard]] ProPrompt* newCountPrompt(Count job, int count, const QString& what = {}) const;
    /// Whether a job that uses Pro may run: `title` names it ("Share this look as a file with
    /// Pro"), `text` says what Pro adds.
    [[nodiscard]] bool admit(core::licensing::Source source, const QString& title, const QString& text);
    /// Whether saving with `uses` may go ahead (Batch, Copy to clipboard): the prompt lists them.
    [[nodiscard]] bool admitUses(core::licensing::Source source, const QString& title,
                                 const QList<core::licensing::ProUse>& uses);

    /// The PixelFx Pro sheet; `source` goes on to the checkout.
    void showProSheet(core::licensing::Source source = core::licensing::Source::Account);
    /// The checkout in the browser, saying which prompt sent the buyer.
    void openCheckout(core::licensing::Source source);

Q_SIGNALS:
    /// A prompt is about to show (tests answer it here).
    void prompting(pixelfx::ui::ProPrompt* prompt);

private:
    /// Shows `prompt` and returns its Outcome; What Pro adds opens the PixelFx Pro sheet after it.
    int run(ProPrompt& prompt);

    services::LicenseService* m_license = nullptr;
    QPointer<QWidget> m_window;
};

/// "Camera RAW photo · DSC_0412.NEF": a Pro item and which one, one line each.
[[nodiscard]] QString describeUses(const QList<core::licensing::ProUse>& uses);

} // namespace pixelfx::ui
