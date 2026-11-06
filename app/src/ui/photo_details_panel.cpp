#include "ui/photo_details_panel.h"

#include "core/describe.h"
#include "core/photo_details.h"
#include "ui/elided_label.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kWidth = 340;
constexpr int kSides = 16;
constexpr int kLabelColumn = 96;
constexpr int kCopiedMs = 1500;
constexpr int kRowGap = 7;       ///< rows of 13 px text 25 px apart
constexpr int kUnderHeading = 3; ///< a section's first row this much further from its heading

QToolButton* flatIconButton(QWidget* parent, const QString& glyph, const QString& tip)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(u"flatIcon"_s);
    button->setIcon(icons::themed(glyph, tokens().muted));
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    return button;
}

} // namespace

PhotoDetailsPanel::PhotoDetailsPanel(QWidget* parent)
    : QFrame(parent)
    , m_scroll(new QScrollArea(this))
{
    setObjectName(u"photoDetails"_s);
    setAttribute(Qt::WA_StyledBackground);
    setAccessibleName(tr("Photo details"));
    setFixedWidth(kWidth);
    hide();

    QWidget* head = buildHead();

    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* copyAll = new IconTextButton(icons::themed(u"copy"_s, tokens().text), tr("Copy details"), this);
    copyAll->setObjectName(u"small"_s);
    copyAll->setToolTip(tr("Copy these details as text"));
    connect(copyAll, &QPushButton::clicked, this,
            [this] { QGuiApplication::clipboard()->setText(detailsText()); });
    auto* foot = new QHBoxLayout;
    foot->addWidget(copyAll);
    foot->addStretch();

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(head);
    column->addWidget(m_scroll, 1);
    column->addSpacing(kGapItems);
    column->addWidget(footBar(this, foot, kSides, kGapItems));
}

PhotoDetailsPanel::~PhotoDetailsPanel() = default;

QWidget* PhotoDetailsPanel::buildHead()
{
    auto* head = new QWidget(this);
    m_name = new ElidedLabel(head);
    m_name->setElideMode(Qt::ElideMiddle);
    m_name->setObjectName(u"detailsName"_s);
    m_folder = new PathLabel({}, head);
    m_folder->setObjectName(u"detailsPath"_s);
    m_folder->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred); // whole while there is room
    m_copyPath = copyButton(tr("Copy the photo's full path"), [this] {
        return QDir::toNativeSeparators(QFileInfo(m_info.path).absoluteFilePath());
    });
    m_copyPath->setParent(head);
    m_showFolder = new QPushButton(tr("Show in folder"), head);
    m_showFolder->setObjectName(u"link"_s);
    connect(m_showFolder, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_info.path).absolutePath()));
    });
    auto* close = flatIconButton(head, u"close"_s, tr("Close (I, Esc)"));
    connect(close, &QToolButton::clicked, this, [this] { setOpen(false); });
    // The folder is as wide as it is drawn, so Copy sits right after it (fitPath).
    auto* pathRow = new QHBoxLayout;
    pathRow->setSpacing(0);
    pathRow->addWidget(m_folder);
    pathRow->addSpacing(kGapTight);
    pathRow->addWidget(m_copyPath);
    pathRow->addSpacing(kGapControls);
    pathRow->addWidget(m_showFolder);
    pathRow->addStretch();
    m_pathRow = new QWidget(head);
    pathRow->setContentsMargins(0, 0, 0, 0);
    m_pathRow->setLayout(pathRow);
    m_pathRow->installEventFilter(this);
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(m_name);
    text->addWidget(m_pathRow);
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(kSides, 12, kGapItems, kGapSmall);
    headRow->setSpacing(kGapControls);
    headRow->addLayout(text, 1);
    headRow->addWidget(close, 0, Qt::AlignTop);
    return head;
}

void PhotoDetailsPanel::setDetails(const services::DocumentInfo& info, LocationOnExport onExport)
{
    if (info.path != m_info.path) {
        m_locationShown = false; // shown again only by choice, photo by photo
    }
    m_info = info;
    m_onExport = onExport;
    const QString name = QFileInfo(info.path).fileName();
    m_name->setText(name);
    m_name->setToolTip(name);
    m_folder->setPath(QFileInfo(info.path).absolutePath());
    rebuild();
    fitPath();
}

void PhotoDetailsPanel::showLocation(bool shown)
{
    if (shown != m_locationShown) {
        m_locationShown = shown;
        rebuild();
    }
}

void PhotoDetailsPanel::setAnchor(QWidget* anchor, QWidget* below)
{
    for (QWidget* w : {m_anchor.data(), m_below.data(), parentWidget()}) {
        if (w != nullptr) {
            w->removeEventFilter(this);
        }
    }
    m_anchor = anchor;
    m_below = below;
    for (QWidget* w : {anchor, below, parentWidget()}) {
        if (w != nullptr) {
            w->installEventFilter(this);
        }
    }
    place();
}

void PhotoDetailsPanel::setOpen(bool open)
{
    if (open == isVisible()) {
        return;
    }
    if (open) {
        QWidget* focus = QApplication::focusWidget();
        m_focusBefore = focus != nullptr && !isAncestorOf(focus) ? focus : nullptr;
        place();
        show();
        raise();
        m_copyPath->setFocus(Qt::OtherFocusReason);
    } else {
        const bool hadFocus = isAncestorOf(QApplication::focusWidget());
        hide();
        if (hadFocus && m_focusBefore != nullptr) {
            m_focusBefore->setFocus(Qt::OtherFocusReason);
        }
    }
    Q_EMIT openChanged(open);
}

QString PhotoDetailsPanel::detailsText() const
{
    QStringList lines{QFileInfo(m_info.path).fileName(),
                      QDir::toNativeSeparators(QFileInfo(m_info.path).absoluteFilePath())};
    lines += m_lines;
    return lines.join(u'\n');
}

void PhotoDetailsPanel::fitPath()
{
    const int room = m_pathRow->width() - m_copyPath->sizeHint().width() - m_showFolder->sizeHint().width() -
                     kGapTight - kGapControls;
    m_folder->setFixedWidth(m_folder->shownWidth(std::max(0, room)));
}

bool PhotoDetailsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_pathRow && event->type() == QEvent::Resize) {
        fitPath();
        return false;
    }
    const bool moved =
        event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show;
    if (moved && isVisible() && watched != this) {
        place();
    }
    return QFrame::eventFilter(watched, event);
}

void PhotoDetailsPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        setOpen(false);
        return;
    }
    QFrame::keyPressEvent(event);
}

void PhotoDetailsPanel::place()
{
    const QWidget* parent = parentWidget();
    const QWidget* anchor = m_anchor.data();
    const QWidget* below = m_below.data();
    if (parent == nullptr || anchor == nullptr || below == nullptr || m_body == nullptr) {
        return;
    }
    const int left = anchor->mapTo(parent, QPoint(0, 0)).x();
    const int top = below->mapTo(parent, QPoint(0, below->height())).y() + kGapRelated;
    // As tall as its content at this width, scrolling when the window is shorter.
    const QLayout* column = layout();
    int wanted = column->contentsMargins().top() + column->contentsMargins().bottom();
    for (int i = 0; i < column->count(); ++i) {
        const QLayoutItem* item = column->itemAt(i);
        if (item->widget() == m_scroll) {
            const int body = m_body->layout()->totalHeightForWidth(kWidth - (2 * frameWidth()));
            wanted += body >= 0 ? body : m_body->sizeHint().height();
        } else {
            wanted += item->sizeHint().height();
        }
    }
    wanted += 2 * frameWidth();
    const int room = std::max(parent->height() - top - kGapGroups, 160);
    setGeometry(left, top, kWidth, std::min(wanted, room));
}

void PhotoDetailsPanel::rebuild()
{
    auto* body = new QWidget;
    m_column = new QVBoxLayout(body);
    m_column->setContentsMargins(0, 0, 0, 0);
    m_column->setSpacing(0);
    m_lines.clear();
    const services::DocumentInfo& info = m_info;

    addSection(tr("File"));
    addRow(tr("Size"), core::sizeWithMegapixels(info.size));
    QString format = core::formatName(info.format, info.path, info.raw);
    if (!info.raw) { // a RAW's bit depth would be its decoder's, not the camera's
        //: A file's format and bit depth: "JPEG, 8 bits".
        format = tr("%1, %2 bits").arg(format).arg(info.bitsPerSample);
        if (info.hasAlpha) {
            //: A format with an alpha channel: "PNG, 8 bits, transparency".
            format = tr("%1, transparency").arg(format);
        }
    }
    addRow(tr("Format"), format);
    addRow(tr("File size"), core::sizeText(QFileInfo(info.path).size()));
    if (!info.raw) {
        QString profile = tr("None, read as sRGB");
        if (info.hasProfile) {
            profile = info.profileName.isEmpty() ? tr("Embedded, without a name")
                                                 : tr("%1, embedded").arg(info.profileName);
        }
        addRow(tr("Color profile"), profile);
    }

    addSection(tr("Camera"));
    const QStringList camera = {core::cameraName(info.exif), QString::fromStdString(info.exif.lens).trimmed(),
                                core::exposureSettings(info.exif), core::focalLength(info.exif),
                                core::takenText(core::takenAt(info.exif))};
    if (std::ranges::all_of(camera, &QString::isEmpty)) {
        addNote(tr("No camera details in this file."));
    } else {
        const QStringList labels = {tr("Camera"), tr("Lens"), tr("Exposure"), tr("Focal length"),
                                    tr("Taken")};
        for (qsizetype i = 0; i < camera.size(); ++i) {
            if (!camera.at(i).isEmpty()) {
                addRow(labels.at(i), camera.at(i));
            }
        }
    }

    addSection(tr("Location"));
    addLocation();
    m_column->addStretch();

    m_scroll->setWidget(body); // the previous body goes with it
    m_body = body;
    if (isVisible()) {
        place();
    }
}

void PhotoDetailsPanel::addSection(const QString& title)
{
    m_column->addWidget(sectionHeading(title, m_column->parentWidget(), kSides));
    m_rows = new QGridLayout;
    m_rows->setContentsMargins(kSides, kUnderHeading, kSides, 0);
    m_rows->setHorizontalSpacing(kGapItems);
    m_rows->setVerticalSpacing(kRowGap);
    m_rows->setColumnMinimumWidth(0, kLabelColumn);
    m_rows->setColumnStretch(1, 1);
    m_column->addLayout(m_rows);
    m_lines << QString() << title;
}

void PhotoDetailsPanel::addRow(const QString& label, const QString& value)
{
    QWidget* body = m_column->parentWidget();
    auto* name = new QLabel(label, body);
    name->setObjectName(u"fieldLabel"_s);
    auto* text = new QLabel(value, body);
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const int row = m_rows->rowCount();
    m_rows->addWidget(name, row, 0, Qt::AlignTop);
    m_rows->addWidget(text, row, 1, Qt::AlignTop);
    m_lines << label + u": "_s + value;
}

void PhotoDetailsPanel::addNote(const QString& text)
{
    auto* note = new QLabel(text, m_column->parentWidget());
    note->setObjectName(u"muted"_s);
    note->setWordWrap(true);
    m_rows->addWidget(note, m_rows->rowCount(), 0, 1, 2);
    m_lines << text;
}

void PhotoDetailsPanel::addLocation()
{
    const pfx::ExifSummary& exif = m_info.exif;
    if (!exif.hasLocation) {
        addNote(tr("No location in this file."));
        return;
    }
    QWidget* body = m_column->parentWidget();
    auto* line = new QHBoxLayout;
    line->setSpacing(kGapRelated);
    if (m_locationShown) {
        const QString shown = core::coordinatesText(exif.latitude, exif.longitude);
        auto* coordinates = new QLabel(shown, body);
        coordinates->setTextInteractionFlags(Qt::TextSelectableByMouse);
        line->addWidget(coordinates, 1);
        const double latitude = exif.latitude;
        const double longitude = exif.longitude;
        QToolButton* copy = copyButton(tr("Copy the coordinates"), [latitude, longitude] {
            return core::coordinatesForCopy(latitude, longitude);
        });
        copy->setParent(body);
        line->addWidget(copy);
        m_lines << shown;
    } else {
        line->addWidget(new QLabel(tr("Recorded in the file"), body), 1);
        m_lines << tr("Recorded in the file");
    }
    auto* toggle = new IconTextButton(icons::themed(m_locationShown ? u"eye-off"_s : u"eye"_s, tokens().text),
                                      m_locationShown ? tr("Hide") : tr("Show"), body);
    toggle->setObjectName(u"small"_s);
    toggle->setProperty("locationToggle", true);
    toggle->setToolTip(m_locationShown ? tr("Hide the coordinates") : tr("Show where it was taken"));
    // The rows are built again, the toggle with them: switch once its click is over, and give the
    // new toggle the focus the old one had.
    connect(toggle, &QPushButton::clicked, this, [this, toggle] {
        const bool focused = toggle->hasFocus();
        QTimer::singleShot(0, this, [this, focused] {
            showLocation(!m_locationShown);
            for (auto* button : m_body->findChildren<QPushButton*>()) {
                if (focused && button->property("locationToggle").toBool()) {
                    button->setFocus(Qt::TabFocusReason);
                }
            }
        });
    });
    line->addWidget(toggle);
    m_rows->addLayout(line, m_rows->rowCount(), 0, 1, 2);
    auto* note = new QLabel(locationOnExportText(), body);
    note->setObjectName(u"detailsNote"_s);
    note->setWordWrap(true);
    m_rows->addWidget(note, m_rows->rowCount(), 0, 1, 2);
}

QString PhotoDetailsPanel::locationOnExportText() const
{
    switch (m_onExport) {
    case LocationOnExport::Kept:
        return tr("Export keeps it: Keep location is on.");
    case LocationOnExport::LeftOut:
        break;
    case LocationOnExport::LeftOutWithMetadata:
        return tr("Export leaves it out, with the other camera details.");
    }
    return tr("Export leaves it out: Keep location is off.");
}

QToolButton* PhotoDetailsPanel::copyButton(const QString& tip, std::function<QString()> text)
{
    QToolButton* button = flatIconButton(this, u"copy"_s, tip);
    connect(button, &QToolButton::clicked, button, [button, tip, text = std::move(text)] {
        QGuiApplication::clipboard()->setText(text());
        button->setIcon(icons::themed(u"check"_s, tokens().muted));
        button->setToolTip(tr("Copied"));
        QTimer::singleShot(kCopiedMs, button, [button, tip] {
            button->setIcon(icons::themed(u"copy"_s, tokens().muted));
            button->setToolTip(tip);
        });
    });
    return button;
}

} // namespace pixelfx::ui
