#include "ui/welcome_page.h"

#include "services/thumbnailer.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

WelcomePage::WelcomePage(QWidget* parent)
    : QWidget(parent)
    , m_dropZone(new QFrame(this))
    , m_recent(new QWidget(this))
    , m_cards(new QHBoxLayout)
    , m_thumbnailer(new services::Thumbnailer(this))
{
    setObjectName(u"welcome"_s);
    setAttribute(Qt::WA_StyledBackground);
    buildDropZone();
    buildRecent();
    connect(m_thumbnailer, &services::Thumbnailer::ready, this, &WelcomePage::showThumbnail);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 32, 32, 32);
    outer->addStretch(2);
    outer->addWidget(m_dropZone, 0, Qt::AlignHCenter);
    outer->addWidget(m_recent, 0, Qt::AlignHCenter);
    outer->addStretch(3);
}

void WelcomePage::buildDropZone()
{
    m_dropZone->setObjectName(u"dropZone"_s);
    m_dropZone->setMaximumWidth(760);

    auto* glyph = new QLabel(m_dropZone);
    glyph->setPixmap(icons::pixmap(u"images"_s, tokens().muted, 48, devicePixelRatioF()));
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setFixedHeight(62);
    glyph->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    auto* title = new QLabel(tr("Drop photos here"), m_dropZone);
    title->setObjectName(u"welcomeTitle"_s);
    title->setAlignment(Qt::AlignCenter);
    auto* hint = new QLabel(
        tr("Or open one to edit, or a whole folder to apply a look or preset to every image at once."),
        m_dropZone);
    hint->setObjectName(u"welcomeHint"_s);
    hint->setAlignment(Qt::AlignCenter);
    auto* open = new IconTextButton(icons::themed(u"open"_s, Qt::white), tr("Open images"), m_dropZone);
    open->setObjectName(u"primary"_s);
    open->setToolTip(tr("Open images (Ctrl+O)"));
    connect(open, &QPushButton::clicked, this, &WelcomePage::openRequested);
    auto* formats = new QLabel(formatsText(pfx::readableExtensions()) + u' ' +
                                   tr("Paste from the clipboard with Ctrl+V."),
                               m_dropZone);
    formats->setObjectName(u"faint"_s);
    formats->setAlignment(Qt::AlignCenter);
    formats->setWordWrap(true);

    auto* zone = new QVBoxLayout(m_dropZone);
    zone->setContentsMargins(20, 44, 20, 40);
    zone->setSpacing(kGapControls);
    zone->addWidget(glyph);
    zone->addWidget(title);
    zone->addWidget(hint);
    zone->addSpacing(kGapItems);
    auto* openFolder =
        new IconTextButton(icons::themed(u"folder"_s, tokens().text), tr("Open folder"), m_dropZone);
    openFolder->setToolTip(tr("Every photo in a folder, ready for Batch"));
    connect(openFolder, &QPushButton::clicked, this, &WelcomePage::openFolderRequested);
    auto* batch = new IconTextButton(icons::themed(u"stack"_s, tokens().text), tr("Batch edit"), m_dropZone);
    batch->setToolTip(tr("One recipe for many photos (B)"));
    connect(batch, &QPushButton::clicked, this, &WelcomePage::batchRequested);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(kGapItems);
    buttons->addStretch();
    buttons->addWidget(open);
    buttons->addWidget(openFolder);
    buttons->addWidget(batch);
    buttons->addStretch();
    zone->addLayout(buttons);
    zone->addSpacing(kGapRelated);
    zone->addWidget(formats);
}

void WelcomePage::buildRecent()
{
    auto* recentTitle = new QLabel(tr("Recent"), m_recent);
    recentTitle->setObjectName(u"recentTitle"_s);
    auto* clear = new QPushButton(tr("Clear"), m_recent);
    clear->setObjectName(u"link"_s);
    clear->setToolTip(tr("Forget the recent photos (their edits stay)"));
    clear->setCursor(Qt::PointingHandCursor);
    connect(clear, &QPushButton::clicked, this, &WelcomePage::clearRecentRequested);
    auto* heading = new QHBoxLayout;
    heading->setSpacing(kGapControls);
    heading->addWidget(recentTitle);
    heading->addStretch();
    heading->addWidget(clear);
    m_cards->setSpacing(kGapGroups);
    m_cards->addStretch();
    auto* recentColumn = new QVBoxLayout(m_recent);
    recentColumn->setContentsMargins(0, 24, 0, 0);
    recentColumn->setSpacing(kGapItems);
    recentColumn->addLayout(heading);
    recentColumn->addLayout(m_cards);
    m_recent->hide();
}

QString WelcomePage::formatsText(const std::vector<std::string>& extensions)
{
    const auto has = [&extensions](const char* ext) {
        return std::ranges::find(extensions, std::string(ext)) != extensions.end();
    };
    QStringList names;
    const std::pair<const char*, QString> known[] = {
        {".jpg", u"JPEG"_s},  {".png", u"PNG"_s},  {".webp", u"WebP"_s},   {".avif", u"AVIF"_s},
        {".heic", u"HEIC"_s}, {".tif", u"TIFF"_s}, {".jxl", u"JPEG XL"_s}, {".dng", tr("camera RAW")}};
    for (const auto& [ext, name] : known) {
        if (has(ext)) {
            names << name;
        }
    }
    if (names.isEmpty()) {
        return {};
    }
    const QString last = names.takeLast();
    return names.isEmpty() ? last + u'.' : tr("%1 and %2.").arg(names.join(u", "_s), last);
}

void WelcomePage::resizeEvent(QResizeEvent* event)
{
    const int width = std::max(360, std::min(760, (this->width() * 92) / 100));
    m_dropZone->setFixedWidth(width);
    m_recent->setFixedWidth(width);
    QWidget::resizeEvent(event);
}

void WelcomePage::setSidecarFolder(const QString& folder)
{
    m_sidecarFolder = folder;
}

QList<RecentCard*> WelcomePage::recentCards() const
{
    return m_recent->findChildren<RecentCard*>();
}

void WelcomePage::setRecentFiles(const QStringList& files)
{
    qDeleteAll(recentCards());
    int shown = 0;
    for (const QString& file : files) {
        if (shown == kRecentShown) {
            break;
        }
        if (!QFileInfo::exists(file)) {
            continue;
        }
        auto* card = new RecentCard(file, m_recent);
        connect(card, &RecentCard::clicked, this, &WelcomePage::recentOpened);
        m_cards->insertWidget(shown, card);
        m_thumbnailer->request(file, 2 * RecentCard::kWidth, m_sidecarFolder);
        ++shown;
    }
    m_recent->setVisible(shown > 0);
}

void WelcomePage::showThumbnail(const services::Thumbnail& thumbnail) const
{
    for (RecentCard* card : recentCards()) {
        if (card->path() == thumbnail.path) {
            card->setThumbnail(thumbnail.image, thumbnail.edited);
        }
    }
}

WelcomePage::~WelcomePage() = default;

void WelcomePage::setDragActive(bool active)
{
    m_dropZone->setProperty("dragActive", active);
    m_dropZone->style()->unpolish(m_dropZone);
    m_dropZone->style()->polish(m_dropZone);
}

// ---------------------------------------------------------------------------------------------

RecentCard::RecentCard(const QString& path, QWidget* parent)
    : QWidget(parent)
    , m_path(path)
    , m_name(new QLabel(QFileInfo(path).fileName(), this))
    , m_date(new QLabel(this))
{
    setFixedWidth(kWidth);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(tr("Open %1").arg(QDir::toNativeSeparators(path)));
    setAccessibleName(tr("Open %1").arg(QFileInfo(path).fileName()));
    m_name->setObjectName(u"small"_s);
    m_name->setText(fontMetrics().elidedText(m_name->text(), Qt::ElideMiddle, kWidth));
    m_date->setObjectName(u"faint"_s);
    m_date->setText(dateText());
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, kPictureHeight + 4, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_name);
    column->addWidget(m_date);
}

RecentCard::~RecentCard() = default;

void RecentCard::setThumbnail(const QImage& image, const QDateTime& edited)
{
    m_image = image;
    m_edited = edited;
    m_date->setText(dateText());
    update();
}

QString RecentCard::dateText() const
{
    const auto day = [](const QDate& date) {
        const QDate today = QDate::currentDate();
        if (date == today) {
            return tr("today");
        }
        if (date == today.addDays(-1)) {
            return tr("yesterday");
        }
        return QLocale().toString(date, date.year() == today.year() ? u"d MMM"_s : u"d MMM yyyy"_s);
    };
    if (m_edited.isValid()) {
        return tr("Edited %1").arg(day(m_edited.date()));
    }
    const QDateTime modified = QFileInfo(m_path).lastModified();
    if (!modified.isValid()) {
        return {};
    }
    // A date alone, capitalised when it is a word ("Today").
    QString text = day(modified.date());
    if (!text.isEmpty() && text.at(0).isLower()) {
        text[0] = text.at(0).toUpper();
    }
    return text;
}

void RecentCard::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF box(0, 0, kWidth, kPictureHeight);
    QPainterPath rounded;
    rounded.addRoundedRect(box, 8, 8);
    p.setClipPath(rounded);
    p.fillRect(box, tokens().panel);
    if (!m_image.isNull()) {
        // Cover: fill the 3:2 box, cropping the longer side.
        const QSizeF source = QSizeF(box.size()).scaled(m_image.size(), Qt::KeepAspectRatio);
        const QRectF from((m_image.width() - source.width()) / 2, (m_image.height() - source.height()) / 2,
                          source.width(), source.height());
        p.drawImage(box, m_image, from);
    }
    p.setClipping(false);
    if (showsFocusRing(this)) {
        p.setPen(QPen(tokens().accent, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box.adjusted(1, 1, -1, -1), 8, 8);
    }
}

void RecentCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        Q_EMIT clicked(m_path);
    }
}

void RecentCard::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        Q_EMIT clicked(m_path);
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace pixelfx::ui
