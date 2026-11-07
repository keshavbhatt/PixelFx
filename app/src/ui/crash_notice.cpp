#include "ui/crash_notice.h"

#include "ui/style.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

CrashNotice::CrashNotice(const core::crash::Report& crash, bool editsKept, QWidget* parent)
    : Sheet(parent)
{
    setWindowTitle(tr("PixelFx closed unexpectedly"));
    auto* title = new QLabel(tr("PixelFx closed unexpectedly"), this);
    title->setObjectName(u"sheetTitle"_s);
    QString what;
    if (crash.photo.isEmpty()) {
        what = tr("Last time, PixelFx closed while you were working.");
    } else {
        what = tr("Last time, PixelFx closed while you were working on <b>%1</b>.")
                   .arg(crash.photo.toHtmlEscaped());
    }
    what += u' ';
    what += crash.photo.isEmpty() || editsKept ? tr("Your edits up to the last change were kept.")
                                               : tr("Its edits could not be kept: Remember edits is off.");
    auto* first = new QLabel(what, this);
    first->setTextFormat(Qt::RichText);
    first->setWordWrap(true);
    auto* second = new QLabel(
        tr("A report helps find the cause. It opens in your browser on PixelFx's issue tracker, for you "
           "to read and send."),
        this);
    second->setObjectName(u"muted"_s);
    second->setWordWrap(true);
    const FootButtons buttons = footButtons(this, tr("Report the problem"), {}, true);
    buttons.cancel->setText(tr("Not now"));
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttons.primary, &QPushButton::clicked, this, &QDialog::accept);
    QVBoxLayout* column = sheetLayout(this, buttons.row);
    column->addWidget(title);
    column->addWidget(first);
    column->addWidget(second);
    setSize(Size::Narrow);
}

CrashNotice::~CrashNotice() = default;

} // namespace pixelfx::ui
