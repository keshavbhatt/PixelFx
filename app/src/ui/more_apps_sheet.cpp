#include "ui/more_apps_sheet.h"

#include "ui/style.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <MoreAppsView.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {
constexpr int kColumns = 3;
} // namespace

MoreAppsSheet::MoreAppsSheet(const QString& place, QWidget* parent)
    : Sheet(parent)
{
    setWindowTitle(tr("More apps from Ktechpit"));
    auto* view = new ktechpit::MoreAppsView(place, kColumns, this);
    view->setIntro(tr("Other apps by the developer of PixelFx. Get opens your software centre."));
    auto* status = new QLabel(view->status(), this);
    status->setObjectName(u"credit"_s);
    connect(view, &ktechpit::MoreAppsView::statusChanged, status, &QLabel::setText);

    const FootButtons buttons = footButtons(this, tr("Close"), {status});
    connect(buttons.primary, &QPushButton::clicked, this, &QDialog::accept);
    QVBoxLayout* layout = sheetLayout(this, buttons.row);
    layout->setSpacing(kGapItems);
    layout->addLayout(sheetTitleRow(this, tr("More apps from Ktechpit")));
    layout->addWidget(view, 1);
    setSize(Size::Wide);
    setResizable(QSize(820, 420));
}

MoreAppsSheet::~MoreAppsSheet() = default;

} // namespace pixelfx::ui
