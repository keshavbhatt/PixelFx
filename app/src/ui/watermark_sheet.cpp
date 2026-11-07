#include "ui/watermark_sheet.h"

#include "services/qt_image_bridge.h"
#include "ui/color_swatch.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/message_line.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

// Tokens for the preview: this year and no camera, as most photos will say something close.
core::WatermarkTokens previewTokens()
{
    return {QDate::currentDate(), QObject::tr("Camera")};
}

QImage shaped(const QImage& photo, WatermarkPreview::Shape shape)
{
    if (photo.isNull() || shape == WatermarkPreview::Shape::Photo) {
        return photo;
    }
    // The middle of the photo in the other shape: 4:5 portrait or 1:1.
    const double ratio = shape == WatermarkPreview::Shape::Portrait ? 4.0 / 5.0 : 1.0;
    QSize size = photo.size();
    if (static_cast<double>(size.width()) / size.height() > ratio) {
        size.setWidth(static_cast<int>(std::lround(size.height() * ratio)));
    } else {
        size.setHeight(static_cast<int>(std::lround(size.width() / ratio)));
    }
    return photo.copy(
        QRect(QPoint((photo.width() - size.width()) / 2, (photo.height() - size.height()) / 2), size));
}

// A labelled slider row in `parent` that reads "3%", "70", "-30\u00B0"; returns its slider.
QSlider* sliderRow(QWidget* parent, const QString& name, int low, int high, int standard, const QString& unit)
{
    auto* row = new SliderRow(name, parent);
    row->setRange(low, high, standard);
    row->setFormat([unit](int ticks) { return QString::number(ticks) + unit; });
    return row->slider();
}

} // namespace

WatermarkPreview::WatermarkPreview(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(u"previewArea"_s);
    setAttribute(Qt::WA_StyledBackground);
    setMinimumSize(360, 300);
    setCursor(Qt::CrossCursor);
}

WatermarkPreview::~WatermarkPreview() = default;

void WatermarkPreview::setPhoto(const QImage& photo)
{
    m_photo = photo.convertToFormat(QImage::Format_RGB32);
    rebuild();
}

void WatermarkPreview::setShape(Shape shape)
{
    m_shape = shape;
    rebuild();
}

void WatermarkPreview::setWatermark(const core::Watermark& w)
{
    m_mark = w;
    rebuild();
}

QRect WatermarkPreview::photoRect() const
{
    if (m_shown.isNull()) {
        return {};
    }
    const QRect area = rect().adjusted(16, 16, -16, -16);
    return {QPoint(area.x() + ((area.width() - m_shown.width()) / 2),
                   area.y() + ((area.height() - m_shown.height()) / 2)),
            m_shown.size()};
}

QImage WatermarkPreview::composed() const
{
    return m_shown;
}

void WatermarkPreview::rebuild()
{
    const QImage source = shaped(m_photo, m_shape);
    const QSize room = rect().adjusted(16, 16, -16, -16).size();
    if (source.isNull() || room.isEmpty()) {
        m_shown = QImage();
        update();
        return;
    }
    m_shown = source.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                  .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage under = m_shown; // Auto reads the photo, not the mark being drawn
    QPainter painter(&m_shown);
    core::paintWatermark(painter, m_mark, m_shown.size(), previewTokens(),
                         [&under](const QRect& r) { return core::meanLuminance(under, r); });
    update();
}

void WatermarkPreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    if (!m_shown.isNull()) {
        p.drawImage(photoRect().topLeft(), m_shown);
    }
}

void WatermarkPreview::resizeEvent(QResizeEvent*)
{
    rebuild();
}

void WatermarkPreview::place(const QPointF& at)
{
    const QRect r = photoRect();
    if (r.isEmpty() || m_mark.repeat) {
        return;
    }
    const QPointF centre(std::clamp((at.x() - r.x()) / r.width(), 0.0, 1.0),
                         std::clamp((at.y() - r.y()) / r.height(), 0.0, 1.0));
    Q_EMIT placed(centre);
}

void WatermarkPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        place(event->position());
    }
}

void WatermarkPreview::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) != 0) {
        place(event->position());
    }
}

WatermarkSheet::WatermarkSheet(const QImage& photo, const QString& current, const QString& folder,
                               QWidget* parent)
    : Sheet(parent)
    , m_library(folder)
    , m_photo(photo)
{
    setWindowTitle(tr("Watermark"));
    setModal(true);
    setSize(Size::Editor);

    QWidget* head = buildHeader();

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(buildControls());
    body->addWidget(buildPreview(), 1);

    auto* foot = buildFooter();

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(head);
    column->addLayout(body, 1);
    column->addWidget(footBar(this, foot));

    m_preview->setPhoto(photo);
    connect(m_list, &QComboBox::activated, this, [this](int index) {
        const QString path = m_list->itemData(index).toString();
        if (path.isEmpty()) {
            core::Watermark fresh;
            fresh.name = tr("My watermark");
            load(fresh, QString());
        } else if (const auto w = core::WatermarkLibrary::load(path)) {
            load(*w, path);
        }
    });
    refreshList(current);
    const auto start = current.isEmpty() ? std::nullopt : core::WatermarkLibrary::load(current);
    if (start) {
        load(*start, current);
    } else {
        core::Watermark fresh;
        fresh.name = tr("My watermark");
        load(fresh, QString());
    }
}

WatermarkSheet::~WatermarkSheet() = default;

QWidget* WatermarkSheet::buildHeader()
{
    m_list = new QComboBox(this);
    m_list->setFixedWidth(200);
    m_list->setAccessibleName(tr("Saved watermarks"));
    auto* saveAs = new QPushButton(tr("Save as..."), this);
    saveAs->setObjectName(u"small"_s);
    connect(saveAs, &QPushButton::clicked, this, &WatermarkSheet::saveAs);
    return sheetHeaderBar(this, sheetTitleRow(this, tr("Watermark"), {m_list, saveAs}));
}

QLayout* WatermarkSheet::buildFooter()
{
    m_message = new MessageLine(this);
    m_message->setNote(tr("Applied on export only; the editor view never shows it."));
    const FootButtons buttons = footButtons(this, tr("Use watermark"), {m_message}, true);
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttons.primary, &QPushButton::clicked, this, &WatermarkSheet::useWatermark);
    return buttons.row;
}

QWidget* WatermarkSheet::buildLogoPage()
{
    auto* logoPage = new QWidget(m_kindPages);
    m_logoName = new QLabel(logoPage);
    m_logoName->setObjectName(u"muted"_s);
    auto* chooseLogo = new QPushButton(tr("Choose logo..."), logoPage);
    chooseLogo->setObjectName(u"small"_s);
    connect(chooseLogo, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Choose a logo"), QString(),
                                                          tr("Logos (*.png *.svg *.webp)"));
        if (!file.isEmpty()) {
            m_mark.logo = file;
            m_logoName->setText(QFileInfo(file).fileName());
            sync();
        }
    });
    auto* logoHint = new QLabel(tr("A PNG or SVG with a transparent background works best."), logoPage);
    logoHint->setObjectName(u"faint"_s);
    logoHint->setWordWrap(true);
    auto* logoColumn = new QVBoxLayout(logoPage);
    logoColumn->setContentsMargins(0, 0, 0, 0);
    logoColumn->setSpacing(kGapRelated);
    auto* logoRow = new QHBoxLayout;
    logoRow->setSpacing(kGapRelated);
    logoRow->addWidget(m_logoName, 1);
    logoRow->addWidget(chooseLogo);
    logoColumn->addLayout(logoRow);
    logoColumn->addWidget(logoHint);
    return logoPage;
}

QWidget* WatermarkSheet::buildKindRow(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* panel = box;
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapItems);
    // Text or Logo.
    m_kind = new QButtonGroup(panel);
    auto* kinds = new QHBoxLayout;
    kinds->setSpacing(0);
    const std::array<std::pair<QString, QString>, 2> kindNames = {
        {{tr("Text"), u"type"_s}, {tr("Logo"), u"image"_s}}};
    for (std::size_t i = 0; i < kindNames.size(); ++i) {
        auto* b = new IconTextButton(icons::toggle(kindNames.at(i).second, tokens().muted, tokens().accent),
                                     kindNames.at(i).first, panel);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        m_kind->addButton(b, static_cast<int>(i));
        kinds->addWidget(b, 1);
    }

    m_kindPages = new QStackedWidget(panel);
    auto* textPage = new QWidget(m_kindPages);
    m_text = new QLineEdit(textPage);
    m_text->setAccessibleName(tr("Watermark text"));
    auto* tokensHint = new QLabel(tr("Tokens: {year}, {camera}, {date}"), textPage);
    tokensHint->setObjectName(u"faint"_s);
    auto* textColumn = new QVBoxLayout(textPage);
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(kGapRelated);
    textColumn->addWidget(m_text);
    textColumn->addWidget(tokensHint);
    auto* logoPage = buildLogoPage();
    m_kindPages->addWidget(textPage);
    m_kindPages->addWidget(logoPage);
    column->addLayout(kinds);
    column->addWidget(m_kindPages);
    return box;
}

QWidget* WatermarkSheet::buildControls()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(u"sheetSide"_s);
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setFixedWidth(340);
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(kPanelPadding, 12, kPanelPadding, 12);
    column->setSpacing(kGapItems);

    column->addWidget(buildKindRow(panel));
    connect(m_kind, &QButtonGroup::idClicked, this, [this](int id) {
        m_kindPages->setCurrentIndex(id);
        sync();
    });
    connect(m_text, &QLineEdit::textEdited, this, &WatermarkSheet::sync);

    column->addWidget(buildColorRow());

    const core::Watermark defaults;
    const auto percent = [](double share) { return static_cast<int>(std::lround(share * 100)); };
    m_size = sliderRow(panel, tr("Size"), 1, 20, percent(defaults.size), u"%"_s);
    m_opacity = sliderRow(panel, tr("Opacity"), 0, 100, percent(defaults.opacity), QString());
    m_margin = sliderRow(panel, tr("Margin"), 0, 10, percent(defaults.margin), u"%"_s);
    auto* sliders = new QVBoxLayout;
    sliders->setSpacing(kGapSmall);
    for (const QSlider* slider : {m_size, m_opacity, m_margin}) {
        sliders->addWidget(slider->parentWidget());
        connect(slider, &QSlider::valueChanged, this, &WatermarkSheet::sync);
    }
    column->addLayout(sliders);

    column->addWidget(buildPositionRow());

    auto* note = new QLabel(tr("Size and margin are shares of the photo's long edge, so the mark looks "
                               "the same on every photo in a batch."),
                            panel);
    note->setObjectName(u"faint"_s);
    note->setWordWrap(true);
    column->addWidget(note);
    column->addStretch();
    return panel;
}

QWidget* WatermarkSheet::buildColorRow()
{
    auto* row = new QWidget(this);
    m_font = new QFontComboBox(row);
    m_font->setAccessibleName(tr("Font"));
    m_font->setMinimumWidth(120);
    connect(m_font, &QFontComboBox::currentFontChanged, this, &WatermarkSheet::sync);
    m_colors = new QButtonGroup(row);
    auto* autoButton = new QPushButton(tr("Auto"), row);
    autoButton->setObjectName(u"chip"_s);
    autoButton->setCheckable(true);
    autoButton->setToolTip(tr("Light or dark, whichever reads on the area under the mark"));
    m_colors->addButton(autoButton, static_cast<int>(core::Watermark::ColorMode::Auto));
    const auto swatch = [this, row](const QBrush& fill, const QString& name,
                                    core::Watermark::ColorMode mode) {
        auto* s = new ColorSwatch(fill, name, row);
        m_colors->addButton(s, static_cast<int>(mode));
        return s;
    };
    auto* white = swatch(QColor(Qt::white), tr("White"), core::Watermark::ColorMode::White);
    auto* black = swatch(QColor(0x11, 0x11, 0x11), tr("Black"), core::Watermark::ColorMode::Black);
    m_custom = swatch(ColorSwatch::allHues(), tr("Pick a colour"), core::Watermark::ColorMode::Custom);
    connect(m_custom, &QToolButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_mark.color, this, tr("Watermark colour"));
        if (c.isValid()) {
            m_mark.color = c;
        }
        sync();
    });
    connect(m_colors, &QButtonGroup::idClicked, this, [this](int id) {
        if (id != static_cast<int>(core::Watermark::ColorMode::Custom)) {
            sync();
        }
    });
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kGapRelated);
    layout->addWidget(m_font, 1);
    layout->addWidget(autoButton);
    layout->addWidget(white);
    layout->addWidget(black);
    layout->addWidget(m_custom);
    return row;
}

QWidget* WatermarkSheet::buildPositionRow()
{
    auto* row = new QWidget(this);
    auto* label = new QLabel(tr("Position"), row);
    label->setObjectName(u"faint"_s);
    auto* gridBox = new QFrame(row);
    gridBox->setObjectName(u"positionBox"_s);
    auto* grid = new QGridLayout(gridBox);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setSpacing(kGapSmall);
    m_anchors = new QButtonGroup(row);
    static const std::array<const char*, 9> names = {
        QT_TR_NOOP("Top left"),    QT_TR_NOOP("Top"),    QT_TR_NOOP("Top right"),
        QT_TR_NOOP("Left"),        QT_TR_NOOP("Middle"), QT_TR_NOOP("Right"),
        QT_TR_NOOP("Bottom left"), QT_TR_NOOP("Bottom"), QT_TR_NOOP("Bottom right")};
    for (int i = 0; i < 9; ++i) {
        auto* cell = new QToolButton(gridBox);
        cell->setObjectName(u"positionCell"_s);
        cell->setCheckable(true);
        cell->setFixedSize(26, 26);
        cell->setToolTip(tr(names.at(static_cast<std::size_t>(i))));
        cell->setAccessibleName(cell->toolTip());
        m_anchors->addButton(cell, i);
        grid->addWidget(cell, i / 3, i % 3);
    }
    connect(m_anchors, &QButtonGroup::idClicked, this, [this] {
        m_mark.position.reset(); // a cell undoes a free placement
        sync();
    });

    m_repeat = new QCheckBox(tr("Repeat across the photo"), row);
    m_shadow = new QCheckBox(tr("Soft shadow"), row);
    m_repeat->setObjectName(u"switch"_s);
    m_shadow->setObjectName(u"switch"_s);
    connect(m_repeat, &QCheckBox::toggled, this, &WatermarkSheet::sync);
    connect(m_shadow, &QCheckBox::toggled, this, &WatermarkSheet::sync);
    // The angle matters only when the mark repeats.
    m_angle = sliderRow(row, tr("Angle"), -90, 90, static_cast<int>(std::lround(core::Watermark().angle)),
                        u"\u00B0"_s);
    m_angleRow = m_angle->parentWidget();
    connect(m_angle, &QSlider::valueChanged, this, &WatermarkSheet::sync);
    auto* options = new QVBoxLayout;
    options->setSpacing(kGapRelated);
    options->addWidget(m_repeat);
    options->addWidget(m_shadow);
    options->addWidget(m_angleRow);
    options->addStretch();
    auto* columns = new QHBoxLayout;
    columns->setSpacing(kGapColumns);
    columns->addWidget(gridBox, 0, Qt::AlignTop);
    columns->addLayout(options, 1);
    // "Position" heads both columns, so the switches start level with the grid.
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kGapSmall);
    layout->addWidget(label);
    layout->addLayout(columns);
    return row;
}

QWidget* WatermarkSheet::buildPreview()
{
    auto* side = new QWidget(this);
    m_preview = new WatermarkPreview(side);
    connect(m_preview, &WatermarkPreview::placed, this, [this](QPointF centre) {
        m_mark.position = centre;
        if (QAbstractButton* b = m_anchors->checkedButton()) {
            m_anchors->setExclusive(false); // a free place has no cell
            b->setChecked(false);
            m_anchors->setExclusive(true);
        }
        sync();
    });
    auto* bar = new QWidget(side);
    bar->setObjectName(u"previewBar"_s);
    bar->setAttribute(Qt::WA_StyledBackground);
    auto* on = new QLabel(tr("Preview on"), bar);
    on->setObjectName(u"faint"_s);
    auto* shapes = new QButtonGroup(bar);
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(kPanelPadding, 8, kPanelPadding, 8);
    barLayout->setSpacing(0);
    barLayout->addWidget(on);
    barLayout->addSpacing(kGapRelated);
    const std::array<QString, 4> shapeNames = {tr("This photo"), tr("Portrait"), tr("Square"),
                                               tr("Batch photos")};
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(shapeNames.at(static_cast<std::size_t>(i)), bar);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        shapes->addButton(b, i);
        barLayout->addWidget(b);
    }
    m_batchButton = qobject_cast<QPushButton*>(shapes->button(3));
    m_batchButton->setToolTip(tr("Each click shows the next photo of the batch"));
    m_batchButton->hide(); // until there are batch photos
    connect(shapes, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == 3) {
            showNextBatchPhoto();
            return;
        }
        m_preview->setPhoto(m_photo);
        m_preview->setShape(static_cast<WatermarkPreview::Shape>(id));
    });
    barLayout->addStretch();
    auto* drag = new QLabel(tr("Drag the mark to place it freely"), bar);
    drag->setObjectName(u"faint"_s);
    barLayout->addWidget(drag);
    auto* column = new QVBoxLayout(side);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_preview, 1);
    column->addWidget(bar);
    return side;
}

void WatermarkSheet::setBatchPhotos(const QStringList& paths)
{
    m_batchPhotos = paths;
    m_batchButton->setVisible(!paths.isEmpty());
}

void WatermarkSheet::showNextBatchPhoto()
{
    if (m_batchPhotos.isEmpty()) {
        return;
    }
    m_batchIndex = (m_batchIndex + 1) % static_cast<int>(m_batchPhotos.size());
    const QString path = m_batchPhotos.at(m_batchIndex);
    // Decoded at preview size on a worker; the newest click wins.
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, index = m_batchIndex] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        if (index == m_batchIndex && !image.isNull() && m_batchButton->isChecked()) {
            m_preview->setShape(WatermarkPreview::Shape::Photo);
            m_preview->setPhoto(image);
        }
    });
    watcher->setFuture(QtConcurrent::run([path] {
        const auto loaded = pfx::load(QFileInfo(path).absoluteFilePath().toStdString(), {1200});
        const auto pixels =
            loaded ? pfx::toSrgb8(loaded.value().image) : pfx::Result<pfx::Rgba8Image>(pfx::Error{});
        return pixels ? services::toQImage(pixels.value()) : QImage();
    }));
}

void WatermarkSheet::refreshList(const QString& select)
{
    m_list->clear();
    for (const auto& e : m_library.entries()) {
        m_list->addItem(e.watermark.name, e.path);
    }
    m_list->addItem(tr("New watermark"), QString());
    const int index = select.isEmpty() ? m_list->count() - 1 : m_list->findData(select);
    m_list->setCurrentIndex(std::max(0, index));
}

void WatermarkSheet::load(const core::Watermark& w, const QString& path)
{
    m_loading = true;
    m_mark = w;
    m_path = path;
    m_kind->button(static_cast<int>(w.kind))->setChecked(true);
    m_kindPages->setCurrentIndex(static_cast<int>(w.kind));
    m_text->setText(w.text);
    m_logoName->setText(w.logo.isEmpty() ? tr("No logo yet") : QFileInfo(w.logo).fileName());
    m_font->setCurrentFont(w.font.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::GeneralFont)
                                            : QFont(w.font));
    m_colors->button(static_cast<int>(w.colorMode))->setChecked(true);
    m_size->setValue(static_cast<int>(std::lround(w.size * 100)));
    m_opacity->setValue(static_cast<int>(std::lround(w.opacity * 100)));
    m_margin->setValue(static_cast<int>(std::lround(w.margin * 100)));
    if (w.position) {
        m_anchors->setExclusive(false);
        for (QAbstractButton* b : m_anchors->buttons()) {
            b->setChecked(false);
        }
        m_anchors->setExclusive(true);
    } else {
        m_anchors->button(w.anchor)->setChecked(true);
    }
    m_repeat->setChecked(w.repeat);
    m_shadow->setChecked(w.shadow);
    m_angle->setValue(static_cast<int>(std::lround(w.angle)));
    m_loading = false;
    sync();
}

void WatermarkSheet::sync()
{
    if (m_loading) {
        return;
    }
    if (m_message != nullptr && m_message->isError()) {
        m_message->clear(); // the edit may have put it right
    }
    m_mark.kind = static_cast<core::Watermark::Kind>(std::max(0, m_kind->checkedId()));
    m_mark.text = m_text->text();
    // The family as picked; the system sans is stored as empty so it follows the system.
    const QString family = m_font->currentFont().family();
    m_mark.font =
        family == QFontDatabase::systemFont(QFontDatabase::GeneralFont).family() ? QString() : family;
    m_mark.colorMode = static_cast<core::Watermark::ColorMode>(std::max(0, m_colors->checkedId()));
    m_mark.size = m_size->value() / 100.0;
    m_mark.opacity = m_opacity->value() / 100.0;
    m_mark.margin = m_margin->value() / 100.0;
    if (m_anchors->checkedId() >= 0) {
        m_mark.anchor = m_anchors->checkedId();
    }
    m_mark.repeat = m_repeat->isChecked();
    m_mark.shadow = m_shadow->isChecked();
    m_mark.angle = m_angle->value();
    m_angleRow->setVisible(m_mark.repeat);
    m_text->setEnabled(m_mark.kind == core::Watermark::Kind::Text);
    m_font->setEnabled(m_mark.kind == core::Watermark::Kind::Text);
    m_preview->setWatermark(m_mark);
}

void WatermarkSheet::saveAs()
{
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Save watermark"), tr("Name"), QLineEdit::Normal, m_mark.name, &ok)
            .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    m_mark.name = name;
    const QString path = m_library.save(m_mark);
    if (path.isEmpty()) {
        m_message->showError(tr("The watermark could not be saved."));
    } else {
        refreshList(path);
        if (const auto saved = core::WatermarkLibrary::load(path)) {
            load(*saved, path);
        }
    }
}

bool WatermarkSheet::useWatermark()
{
    if (m_mark.kind == core::Watermark::Kind::Logo && m_mark.logo.isEmpty()) {
        m_message->showError(tr("Choose a logo first, or switch to Text."));
        return false;
    }
    if (m_mark.kind == core::Watermark::Kind::Text && m_mark.text.trimmed().isEmpty()) {
        m_message->showError(tr("Type the text first, or switch to Logo.")); // it would print nothing
        return false;
    }
    if (m_mark.name.trimmed().isEmpty()) {
        m_mark.name = tr("My watermark");
    }
    if (m_path.isEmpty()) {
        // A new watermark never replaces a saved one of the same name.
        QStringList taken;
        for (const auto& e : m_library.entries()) {
            taken << e.watermark.name;
        }
        const QString base = m_mark.name.trimmed();
        for (int n = 2; taken.contains(m_mark.name.trimmed()); ++n) {
            m_mark.name = u"%1 %2"_s.arg(base).arg(n);
        }
    }
    const QString saved = m_library.save(m_mark, m_path);
    if (saved.isEmpty()) {
        m_message->showError(tr("The watermark could not be saved."));
        return false;
    }
    m_path = saved;
    accept();
    return true;
}

WatermarkPicker::WatermarkPicker(const QString& current, QWidget* parent)
    : QWidget(parent)
    , m_box(new QComboBox(this))
    , m_edit(new QPushButton(tr("Edit..."), this))
    , m_folder(core::WatermarkLibrary::defaultFolder())
    , m_remembered(current)
{
    fitToPanel(m_box); // a long watermark name never widens the panel
    m_box->setAccessibleName(tr("Watermark"));
    m_edit->setObjectName(u"small"_s);
    refresh(current);
    connect(m_box, &QComboBox::activated, this, [this](int index) {
        if (m_box->itemData(index).toString() == u"new"_s) {
            edit(QString());
        } else {
            m_last = index;
            m_edit->setEnabled(!path().isEmpty());
            Q_EMIT changed();
        }
    });
    connect(m_edit, &QPushButton::clicked, this, [this] { edit(path()); });
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_box, 1);
    layout->addWidget(m_edit);
}

WatermarkPicker::~WatermarkPicker() = default;

QString WatermarkPicker::path() const
{
    const QString file = m_box->currentData().toString();
    return file == u"new"_s ? QString() : file;
}

void WatermarkPicker::setPath(const QString& file)
{
    refresh(file);
}

QStringList WatermarkPicker::names() const
{
    QStringList out;
    for (int i = 0; i < m_box->count(); ++i) {
        out << m_box->itemText(i);
    }
    return out;
}

void WatermarkPicker::setFolder(const QString& folder)
{
    const QString current = path();
    m_folder = folder;
    refresh(current.isEmpty() ? m_remembered : current);
}

void WatermarkPicker::refresh(const QString& select)
{
    m_box->clear();
    m_box->addItem(tr("None"), QString());
    for (const auto& e : core::WatermarkLibrary(m_folder).entries()) {
        m_box->addItem(e.watermark.name, e.path);
    }
    m_box->addItem(tr("New watermark..."), u"new"_s);
    // A remembered watermark that is gone falls back to None.
    m_last = select.isEmpty() ? 0 : std::max(0, m_box->findData(select));
    m_box->setCurrentIndex(m_last);
    m_edit->setEnabled(m_last > 0);
}

void WatermarkPicker::edit(const QString& file)
{
    WatermarkSheet sheet(m_preview, file, m_folder, this);
    sheet.setBatchPhotos(m_batchPhotos);
    if (sheet.exec() == QDialog::Accepted) {
        refresh(sheet.chosenPath());
        Q_EMIT changed();
    } else {
        m_box->setCurrentIndex(m_last); // "New watermark..." cancelled
    }
    m_edit->setEnabled(!path().isEmpty());
}

} // namespace pixelfx::ui
