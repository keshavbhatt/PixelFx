#include "ui/welcome_page.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

WelcomePage::WelcomePage(QWidget* parent)
    : QWidget(parent)
    , m_dropZone(new QFrame(this))
{
    setObjectName(u"welcome"_s);
    setAttribute(Qt::WA_StyledBackground);
    m_dropZone->setObjectName(u"dropZone"_s);
    m_dropZone->setMaximumWidth(760);

    auto* glyph = new QLabel(m_dropZone);
    glyph->setPixmap(icons::pixmap(u"images"_s, tokens().muted, 48, devicePixelRatioF()));
    glyph->setAlignment(Qt::AlignCenter);
    auto* title = new QLabel(tr("Drop photos here"), m_dropZone);
    title->setObjectName(u"welcomeTitle"_s);
    title->setAlignment(Qt::AlignCenter);
    auto* hint = new QLabel(tr("Or open one to edit."), m_dropZone);
    hint->setObjectName(u"welcomeHint"_s);
    hint->setAlignment(Qt::AlignCenter);
    auto* open = new QPushButton(icons::themed(u"open"_s, Qt::white), tr("Open images"), m_dropZone);
    open->setObjectName(u"primary"_s);
    open->setToolTip(tr("Open images (Ctrl+O)"));
    connect(open, &QPushButton::clicked, this, &WelcomePage::openRequested);
    auto* formats = new QLabel(
        tr("JPEG, PNG, TIFF and WebP. More formats, camera RAW included, are on the way."), m_dropZone);
    formats->setObjectName(u"faint"_s);
    formats->setAlignment(Qt::AlignCenter);
    formats->setWordWrap(true);

    auto* zone = new QVBoxLayout(m_dropZone);
    zone->setContentsMargins(20, 44, 20, 40);
    zone->setSpacing(8);
    zone->addWidget(glyph);
    zone->addWidget(title);
    zone->addWidget(hint);
    zone->addSpacing(10);
    zone->addWidget(open, 0, Qt::AlignHCenter);
    zone->addSpacing(6);
    zone->addWidget(formats);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 32, 32, 32);
    outer->addStretch();
    outer->addWidget(m_dropZone, 0, Qt::AlignHCenter);
    outer->addStretch();
    m_dropZone->setMinimumWidth(520);
}

void WelcomePage::setDragActive(bool active)
{
    m_dropZone->setProperty("dragActive", active);
    m_dropZone->style()->unpolish(m_dropZone);
    m_dropZone->style()->polish(m_dropZone);
}

} // namespace pixelfx::ui
