#include "ui/canvas.h"

#include "ui/icons.h"
#include "ui/straighten_dial.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <ranges>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {
constexpr int kMargin = 28;
constexpr double kCornerReach = 14.0; ///< how near a corner a press grabs it
constexpr double kEdgeReach = 8.0;
constexpr double kMinimumBox = 24.0;
constexpr int kCornerLength = 18; ///< mock crop.html: 18 px corners, 4 px thick, 3 px outside
constexpr int kCornerWidth = 4;
// Zoom steps in screen pixels per photo pixel; Fit sits wherever the window puts it.
constexpr std::array kZoomSteps = {0.0625, 0.125, 0.25, 1.0 / 3, 0.5, 2.0 / 3, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0};
constexpr double kWheelStep = 1.25;

Qt::CursorShape cursorFor(core::CropHandle handle)
{
    using enum core::CropHandle;
    switch (handle) {
    case TopLeft:
    case BottomRight:
        return Qt::SizeFDiagCursor;
    case TopRight:
    case BottomLeft:
        return Qt::SizeBDiagCursor;
    case Left:
    case Right:
        return Qt::SizeHorCursor;
    case Top:
    case Bottom:
        return Qt::SizeVerCursor;
    case Move:
        return Qt::SizeAllCursor;
    case None:
        break;
    }
    return Qt::ArrowCursor;
}
} // namespace

Canvas::Canvas(QWidget* parent)
    : QWidget(parent)
    , m_dial(new StraightenDial(this))
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAccessibleName(tr("Photo"));
    buildZoomBar();
    m_dial->hide();
}

void Canvas::startLevelLine()
{
    if (m_cropping) {
        m_levelLine = true;
        setCursor(Qt::CrossCursor);
    }
}

void Canvas::buildZoomBar()
{
    // Mock editor.html .zoombar: zoom out, the level, zoom in, a divider, 1:1, full screen.
    m_zoomBar = new QWidget(this);
    m_zoomBar->setObjectName(u"zoomBar"_s);
    m_zoomBar->setAttribute(Qt::WA_StyledBackground);
    const auto tool = [this](const QString& glyph, const QString& tip) {
        auto* b = new QToolButton(m_zoomBar);
        b->setObjectName(u"zoomButton"_s);
        b->setIcon(icons::themed(glyph, tokens().text, tokens().faint));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setAutoRaise(true);
        return b;
    };
    m_zoomOut = tool(u"zoom-out"_s, tr("Zoom out (Ctrl+Minus)"));
    m_zoomIn = tool(u"zoom-in"_s, tr("Zoom in (Ctrl+Plus)"));
    m_zoomLabel = new QLabel(tr("Fit"), m_zoomBar);
    m_zoomLabel->setObjectName(u"zoomLevel"_s);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setMinimumWidth(54);
    m_zoomLabel->setToolTip(tr("Fit the photo (Ctrl+0)"));
    m_actual = tool(QString(), tr("100% (Ctrl+1)"));
    m_actual->setText(u"1:1"_s);
    m_actual->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* fullScreen = tool(u"maximize"_s, tr("Full screen (F11)"));
    auto* divider = new QWidget(m_zoomBar);
    divider->setObjectName(u"zoomDivider"_s);
    divider->setAttribute(Qt::WA_StyledBackground);
    divider->setFixedSize(1, 18);
    connect(m_zoomOut, &QToolButton::clicked, this, &Canvas::zoomOut);
    connect(m_zoomIn, &QToolButton::clicked, this, &Canvas::zoomIn);
    connect(m_actual, &QToolButton::clicked, this, [this] { setZoom(1.0); });
    connect(fullScreen, &QToolButton::clicked, this, &Canvas::fullScreenRequested);
    auto* row = new QHBoxLayout(m_zoomBar);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(2);
    row->addWidget(m_zoomOut);
    row->addWidget(m_zoomLabel);
    row->addWidget(m_zoomIn);
    row->addSpacing(4);
    row->addWidget(divider);
    row->addSpacing(4);
    row->addWidget(m_actual);
    row->addWidget(fullScreen);
    m_zoomBar->hide();
}

void Canvas::updateZoomBar()
{
    m_zoomBar->setVisible(!m_image.isNull());
    m_zoomLabel->setText(zoomText());
    const bool zoomable = !m_cropping && !m_image.isNull();
    m_zoomOut->setEnabled(zoomable && !isFit());
    m_zoomIn->setEnabled(zoomable && scale() < kZoomSteps.back());
    m_actual->setEnabled(zoomable);
    m_zoomBar->adjustSize();
    m_zoomBar->move((width() - m_zoomBar->width()) / 2, height() - m_zoomBar->height() - 12);
    m_zoomBar->raise();
    // Mock crop.html: the ruler sits just above the zoom bar while cropping.
    m_dial->setVisible(m_cropping && !m_image.isNull());
    m_dial->move((width() - m_dial->width()) / 2, m_zoomBar->y() - m_dial->height() - 8);
    m_dial->raise();
}

void Canvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    clampCentre();
    updateZoomBar();
}

QSizeF Canvas::zoomedSize() const
{
    const double dpr = devicePixelRatioF();
    return {m_fullSize.width() * m_zoom / dpr, m_fullSize.height() * m_zoom / dpr};
}

double Canvas::scale() const
{
    if (!isFit() && !m_cropping) {
        return m_zoom;
    }
    const QRect fit = fitRect();
    return m_fullSize.width() > 0 ? fit.width() * devicePixelRatioF() / m_fullSize.width() : 1.0;
}

QString Canvas::zoomText() const
{
    return (isFit() || m_cropping) ? tr("Fit") : u"%1%"_s.arg(qRound(m_zoom * 100));
}

void Canvas::clampCentre()
{
    if (isFit()) {
        m_centre = {0.5, 0.5};
        return;
    }
    const QSizeF size = zoomedSize();
    const auto clampAxis = [](double centre, double view, double extent) {
        if (extent <= view) {
            return 0.5; // smaller than the window: centred
        }
        const double half = view / 2.0 / extent;
        return std::clamp(centre, half, 1.0 - half);
    };
    m_centre = {clampAxis(m_centre.x(), width(), size.width()),
                clampAxis(m_centre.y(), height(), size.height())};
}

void Canvas::setZoom(double scale, QPointF anchor)
{
    if (m_cropping || m_image.isNull() || m_fullSize.isEmpty()) {
        return;
    }
    const QPointF middle(width() / 2.0, height() / 2.0);
    if (anchor.isNull()) {
        anchor = middle;
    }
    // The photo point under the anchor stays under it.
    const QRectF before(photoRect());
    const QPointF point((anchor.x() - before.left()) / std::max(1.0, before.width()),
                        (anchor.y() - before.top()) / std::max(1.0, before.height()));
    const double fitScale = [this] {
        const QRect fit = fitRect();
        return fit.width() * devicePixelRatioF() / std::max(1, m_fullSize.width());
    }();
    m_zoom = scale <= fitScale * 1.001 ? 0.0 : std::min(scale, kZoomSteps.back());
    if (!isFit()) {
        const QSizeF size = zoomedSize();
        m_centre = {point.x() - ((anchor.x() - middle.x()) / size.width()),
                    point.y() - ((anchor.y() - middle.y()) / size.height())};
    }
    clampCentre();
    if (m_compare) {
        setCursor(Qt::SplitHCursor);
    } else {
        setCursor(isFit() ? Qt::ArrowCursor : Qt::OpenHandCursor);
    }
    updateZoomBar();
    update();
    Q_EMIT zoomChanged();
}

void Canvas::zoomToFit()
{
    setZoom(0.0);
}

void Canvas::zoomIn()
{
    const double now = scale();
    for (const double step : kZoomSteps) {
        if (step > now * 1.01) {
            setZoom(step);
            return;
        }
    }
}

void Canvas::zoomOut()
{
    const double now = scale();
    for (const double step : std::views::reverse(kZoomSteps)) {
        if (step < now * 0.99) {
            setZoom(step);
            return;
        }
    }
    zoomToFit();
}

QRectF Canvas::visibleFraction() const
{
    const QRectF photo(photoRect());
    if (photo.isEmpty()) {
        return {};
    }
    const QRectF shown = photo.intersected(QRectF(rect()));
    return {(shown.left() - photo.left()) / photo.width(), (shown.top() - photo.top()) / photo.height(),
            shown.width() / photo.width(), shown.height() / photo.height()};
}

void Canvas::setDetail(const QImage& detail)
{
    m_detail = detail;
    update();
}

bool Canvas::needsDetail() const
{
    // Past the preview's own resolution the preview would be enlarged: a sharper picture helps.
    return !m_image.isNull() && !m_cropping && !isFit() &&
           scale() * m_fullSize.width() > m_image.width() * 1.05;
}

void Canvas::wheelEvent(QWheelEvent* event)
{
    if (m_cropping || m_image.isNull()) {
        return;
    }
    const double notches = event->angleDelta().y() / 120.0;
    if (notches == 0.0) {
        return;
    }
    setZoom(scale() * std::pow(kWheelStep, notches), event->position());
    event->accept();
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_cropping || m_compare || event->button() != Qt::LeftButton) {
        return;
    }
    // Fit and 100% at the spot clicked, as photo viewers do.
    isFit() ? setZoom(1.0, event->position()) : zoomToFit();
}

Canvas::~Canvas() = default;

void Canvas::setImage(const QImage& image, const QSize& fullSize)
{
    m_image = image;
    const QSize full = fullSize.isValid() ? fullSize : image.size();
    if (full != m_fullSize) {
        m_detail = QImage(); // another photo, or another shape after a crop
    }
    m_fullSize = full;
    m_status.clear();
    m_error.clear();
    clampCentre();
    updateZoomBar();
    update();
}

void Canvas::showImage(const QImage& image)
{
    m_image = image;
    update();
}

void Canvas::setCompare(bool on)
{
    m_compare = on;
    setCursor(on ? Qt::SplitHCursor : Qt::ArrowCursor);
    update();
}

void Canvas::setOriginal(const QImage& original)
{
    m_original = original;
    update();
}

void Canvas::setSplit(double split)
{
    m_split = std::clamp(split, 0.0, 1.0);
    update();
}

void Canvas::paintCompare(QPainter& painter, const QRect& target)
{
    const int x = target.left() + static_cast<int>(m_split * target.width());
    painter.save();
    painter.setClipRect(QRect(target.left(), target.top(), x - target.left(), target.height()));
    painter.drawImage(target, m_original);
    painter.restore();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawLine(x, target.top(), x, target.bottom());
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    const QPoint knob(x, target.center().y());
    painter.drawEllipse(knob, 15, 15);
    // A left-right arrow, as in editor-compare.html.
    painter.setBrush(QColor(0x22, 0x22, 0x22));
    painter.drawPolygon(QPolygon({knob + QPoint(-9, 0), knob + QPoint(-3, -5), knob + QPoint(-3, 5)}));
    painter.drawPolygon(QPolygon({knob + QPoint(9, 0), knob + QPoint(3, -5), knob + QPoint(3, 5)}));
    const auto tag = [&painter](const QRect& box, const QString& text) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 140));
        painter.drawRoundedRect(box, 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(box, Qt::AlignCenter, text);
    };
    const QFontMetrics fm(font());
    const QString before = tr("Before");
    const QString after = tr("After");
    tag(QRect(target.left() + 10, target.top() + 10, fm.horizontalAdvance(before) + 16, fm.height() + 6),
        before);
    const int afterWidth = fm.horizontalAdvance(after) + 16;
    tag(QRect(target.right() - 10 - afterWidth, target.top() + 10, afterWidth, fm.height() + 6), after);
}

void Canvas::setCropMode(bool on)
{
    if (on) {
        m_zoom = 0.0; // the crop box works on the whole photo
        m_centre = {0.5, 0.5};
        m_detail = QImage();
    }
    m_cropping = on;
    m_levelLine = false;
    m_drawingLine = false;
    updateZoomBar();
    m_handle = core::CropHandle::None;
    setMouseTracking(on);
    setCursor(Qt::ArrowCursor);
    update();
}

void Canvas::setCrop(const QRectF& crop)
{
    m_crop = crop;
    update();
}

void Canvas::setCropRatio(double ratio)
{
    m_cropRatio = ratio;
}

void Canvas::setCropOverlay(CropOverlay overlay)
{
    m_overlay = overlay;
    update();
}

QRectF Canvas::cropBox() const
{
    const QRectF target(photoRect());
    return {target.left() + (m_crop.x() * target.width()), target.top() + (m_crop.y() * target.height()),
            m_crop.width() * target.width(), m_crop.height() * target.height()};
}

core::CropHandle Canvas::cropHandleAt(QPointF pos) const
{
    using enum core::CropHandle;
    const QRectF box = cropBox();
    if (!m_cropping || box.isEmpty()) {
        return None;
    }
    const auto nearPoint = [pos](QPointF corner) {
        return std::abs(pos.x() - corner.x()) <= kCornerReach &&
               std::abs(pos.y() - corner.y()) <= kCornerReach;
    };
    if (nearPoint(box.topLeft())) {
        return TopLeft;
    }
    if (nearPoint(box.topRight())) {
        return TopRight;
    }
    if (nearPoint(box.bottomLeft())) {
        return BottomLeft;
    }
    if (nearPoint(box.bottomRight())) {
        return BottomRight;
    }
    const bool inX = pos.x() >= box.left() && pos.x() <= box.right();
    const bool inY = pos.y() >= box.top() && pos.y() <= box.bottom();
    if (inY && std::abs(pos.x() - box.left()) <= kEdgeReach) {
        return Left;
    }
    if (inY && std::abs(pos.x() - box.right()) <= kEdgeReach) {
        return Right;
    }
    if (inX && std::abs(pos.y() - box.top()) <= kEdgeReach) {
        return Top;
    }
    if (inX && std::abs(pos.y() - box.bottom()) <= kEdgeReach) {
        return Bottom;
    }
    return (inX && inY) ? Move : None;
}

void Canvas::mousePressEvent(QMouseEvent* event)
{
    if (m_cropping && m_levelLine && event->button() == Qt::LeftButton) {
        m_drawingLine = true;
        m_lineStart = event->position();
        m_lineEnd = m_lineStart;
        return;
    }
    if (m_cropping) {
        if (event->button() == Qt::LeftButton) {
            m_handle = cropHandleAt(event->position());
            m_pressPos = event->position();
            m_pressBox = cropBox();
        }
        return;
    }
    const bool pan = !isFit() && ((event->button() == Qt::LeftButton && !m_compare) ||
                                  event->button() == Qt::MiddleButton);
    if (pan) {
        m_panning = true;
        m_panStart = event->position();
        m_centreStart = m_centre;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (m_compare && event->button() == Qt::LeftButton && photoRect().isValid()) {
        m_dragging = true;
        mouseMoveEvent(event);
    }
}

void Canvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drawingLine) {
        m_lineEnd = event->position();
        update();
        return;
    }
    if (m_cropping) {
        if (m_handle == core::CropHandle::None) {
            setCursor(cursorFor(cropHandleAt(event->position())));
            return;
        }
        const QRectF target(photoRect());
        if (target.isEmpty()) {
            return;
        }
        // Shift keeps the box's shape when the ratio is free (mock: the strip's hint).
        const bool keep = m_cropRatio <= 0 && event->modifiers().testFlag(Qt::ShiftModifier);
        const double ratio = keep ? m_pressBox.width() / std::max(1.0, m_pressBox.height()) : m_cropRatio;
        const QRectF box =
            core::dragCrop(m_pressBox, m_handle, event->position() - m_pressPos, target, ratio, kMinimumBox);
        m_crop = QRectF((box.x() - target.x()) / target.width(), (box.y() - target.y()) / target.height(),
                        box.width() / target.width(), box.height() / target.height());
        update();
        Q_EMIT cropEdited(m_crop);
        return;
    }
    if (m_panning) {
        const QSizeF size = zoomedSize();
        const QPointF moved = event->position() - m_panStart;
        m_centre = {m_centreStart.x() - (moved.x() / size.width()),
                    m_centreStart.y() - (moved.y() / size.height())};
        clampCentre();
        update();
        Q_EMIT zoomChanged();
        return;
    }
    if (m_dragging) {
        const QRect target = photoRect();
        setSplit((event->position().x() - target.left()) / std::max(1, target.width()));
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drawingLine) {
        m_lineEnd = event->position(); // the release decides where the line ends
    }
    m_dragging = false;
    m_handle = core::CropHandle::None;
    if (m_drawingLine) {
        m_drawingLine = false;
        m_levelLine = false;
        setCursor(Qt::ArrowCursor);
        const QPointF d = m_lineEnd - m_lineStart;
        update();
        if (std::hypot(d.x(), d.y()) >= 12.0) { // a click is not a line
            Q_EMIT levelLineDrawn(std::atan2(d.y(), d.x()) * 180.0 / std::numbers::pi);
        }
        return;
    }
    if (m_panning) {
        m_panning = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void Canvas::paintOverlay(QPainter& painter, const QRectF& box) const
{
    QList<double> stops;
    switch (m_overlay) {
    case CropOverlay::Thirds:
        stops = {1.0 / 3, 2.0 / 3};
        break;
    case CropOverlay::Grid:
        for (int i = 1; i < 6; ++i) {
            stops << i / 6.0;
        }
        break;
    case CropOverlay::Golden:
        stops = {0.382, 0.618};
        break;
    case CropOverlay::Diagonal: {
        const double d = std::min(box.width(), box.height());
        painter.drawLine(box.topLeft(), box.topLeft() + QPointF(d, d));
        painter.drawLine(box.topRight(), box.topRight() + QPointF(-d, d));
        painter.drawLine(box.bottomLeft(), box.bottomLeft() + QPointF(d, -d));
        painter.drawLine(box.bottomRight(), box.bottomRight() + QPointF(-d, -d));
        return;
    }
    case CropOverlay::None:
        return;
    }
    for (const double f : stops) {
        const double x = box.left() + (f * box.width());
        const double y = box.top() + (f * box.height());
        painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
    }
}

void Canvas::paintCrop(QPainter& painter, const QRect& target)
{
    const QRectF box = cropBox();
    // The rest of the photo dims (mock: brightness .45).
    QPainterPath outside;
    outside.addRect(QRectF(target));
    outside.addRect(box);
    painter.fillPath(outside, QColor(0, 0, 0, 140));
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(255, 255, 255, 140), 1));
    paintOverlay(painter, box);
    painter.setPen(QPen(QColor(0, 0, 0, 100), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box.adjusted(-1, -1, 1, 1));
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(box);
    // Corner handles.
    const QRect b = box.toAlignedRect();
    constexpr int kOut = 3;
    constexpr int kLen = kCornerLength;
    constexpr int kWide = kCornerWidth;
    const int left = b.left() - kOut;
    const int top = b.top() - kOut;
    const int right = b.right() + kOut + 1;
    const int bottom = b.bottom() + kOut + 1;
    const QList<QRect> bars = {{left, top, kLen, kWide},
                               {left, top, kWide, kLen},
                               {right - kLen, top, kLen, kWide},
                               {right - kWide, top, kWide, kLen},
                               {left, bottom - kWide, kLen, kWide},
                               {left, bottom - kLen, kWide, kLen},
                               {right - kLen, bottom - kWide, kLen, kWide},
                               {right - kWide, bottom - kLen, kWide, kLen}};
    for (const QRect& bar : bars) {
        painter.fillRect(bar, Qt::white);
    }
    painter.restore();
}

void Canvas::setStatus(const QString& text)
{
    m_status = text;
    m_error.clear();
    update();
}

void Canvas::setError(const QString& text)
{
    m_error = text;
    m_status.clear();
    update();
}

QRect Canvas::fitRect() const
{
    if (m_image.isNull()) {
        return {};
    }
    // While cropping, the ruler and the zoom bar sit below the photo, not over it.
    const int below = m_cropping ? m_zoomBar->sizeHint().height() + m_dial->sizeHint().height() + 20 : 0;
    const QRect area = rect().adjusted(kMargin, kMargin, -kMargin, -kMargin - below);
    QSize size = m_image.size();
    size.scale(area.size(), Qt::KeepAspectRatio);
    if (size.width() > m_fullSize.width()) { // never enlarge past 100% of the photo at fit
        size = m_image.size().scaled(m_fullSize, Qt::KeepAspectRatio);
    }
    QRect target(QPoint(0, 0), size);
    target.moveCenter(area.center());
    return target;
}

QRect Canvas::photoRect() const
{
    if (isFit() || m_cropping) {
        return fitRect();
    }
    const QSizeF size = zoomedSize();
    const QPointF topLeft((width() / 2.0) - (m_centre.x() * size.width()),
                          (height() / 2.0) - (m_centre.y() * size.height()));
    return QRectF(topLeft, size).toAlignedRect();
}

void Canvas::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter painter(this);
    painter.fillRect(rect(), t.canvas);
    if (!m_image.isNull()) {
        const QRect target = photoRect();
        painter.fillRect(target.translated(0, 6).adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 90));
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const bool sharp = !m_detail.isNull() && needsDetail();
        const QImage& shown = sharp ? m_detail : m_image;
        // Only the part on screen: a 400% view never scales the whole picture.
        const QRectF visible = QRectF(target).intersected(QRectF(rect()));
        const double sx = shown.width() / static_cast<double>(target.width());
        const double sy = shown.height() / static_cast<double>(target.height());
        const QRectF source((visible.left() - target.left()) * sx, (visible.top() - target.top()) * sy,
                            visible.width() * sx, visible.height() * sy);
        painter.drawImage(visible, shown, source);
        if (m_cropping) {
            paintCrop(painter, target);
            if (m_drawingLine) {
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(QColor(0, 0, 0, 160), 4));
                painter.drawLine(m_lineStart, m_lineEnd);
                painter.setPen(QPen(Qt::white, 2));
                painter.drawLine(m_lineStart, m_lineEnd);
            }
        } else if (m_compare && !m_original.isNull()) {
            paintCompare(painter, target);
        }
    }
    const QString& line = m_error.isEmpty() ? m_status : m_error;
    if (!line.isEmpty()) {
        painter.setPen(m_error.isEmpty() ? t.muted : t.danger);
        painter.drawText(rect().adjusted(kMargin, kMargin, -kMargin, -kMargin),
                         Qt::AlignCenter | Qt::TextWordWrap, line);
    }
}

} // namespace pixelfx::ui
