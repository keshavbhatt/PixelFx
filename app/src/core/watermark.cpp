#include "core/watermark.h"

#include "core/file_names.h"
#include "core/photo_details.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <pfx/color.h>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

constexpr int kVersion = 1;

double longEdge(const QSizeF& s)
{
    return std::max(s.width(), s.height());
}

QFont markFont(const Watermark& w, double pixelHeight)
{
    // No family chosen: the system's general font, which every platform has.
    QFont font = w.font.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::GeneralFont) : QFont(w.font);
    if (w.font.isEmpty()) {
        font.setStyleHint(QFont::SansSerif);
    }
    font.setWeight(QFont::DemiBold);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixelHeight))));
    return font;
}

// The logo, drawn to fit `size` (SVG at that size, so it stays sharp).
QImage logoImage(const QString& path, const QSize& size)
{
    if (size.isEmpty()) {
        return {};
    }
    if (path.endsWith(u".svg"_s, Qt::CaseInsensitive)) {
        QSvgRenderer svg(path);
        if (!svg.isValid()) {
            return {};
        }
        QImage out(size, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter p(&out);
        svg.render(&p);
        return out;
    }
    const QImage source(path);
    return source.isNull() ? QImage() : source.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QSizeF logoNaturalSize(const QString& path)
{
    if (path.endsWith(u".svg"_s, Qt::CaseInsensitive)) {
        const QSvgRenderer svg(path);
        return svg.isValid() ? QSizeF(svg.defaultSize()) : QSizeF();
    }
    const QImageReader reader(path);
    return {reader.size()};
}

// The mark's size in pixels on a photo of `photo`: its height is `size` of the long edge.
QSizeF markSize(const Watermark& w, const QSizeF& photo, const QString& text)
{
    const double height = w.size * longEdge(photo);
    if (w.kind == Watermark::Kind::Logo) {
        const QSizeF natural = logoNaturalSize(w.logo);
        if (natural.isEmpty()) {
            return {};
        }
        return {height * natural.width() / natural.height(), height};
    }
    const QFontMetricsF metrics(markFont(w, height));
    return {metrics.horizontalAdvance(text), metrics.height()};
}

QJsonObject positionJson(const std::optional<QPointF>& p)
{
    return p ? QJsonObject{{u"x"_s, p->x()}, {u"y"_s, p->y()}} : QJsonObject();
}

} // namespace

WatermarkTokens tokensFrom(const pfx::ExifSummary& exif, const QDate& fallback)
{
    WatermarkTokens t;
    t.date = QDate::fromString(QString::fromStdString(exif.dateTaken).left(10), u"yyyy:MM:dd"_s);
    if (!t.date.isValid()) {
        t.date = fallback;
    }
    t.camera = cameraName(exif);
    return t;
}

QString expandTokens(const QString& text, const WatermarkTokens& tokens)
{
    QString out = text;
    out.replace(u"{year}"_s, tokens.date.isValid() ? QString::number(tokens.date.year()) : QString());
    out.replace(u"{date}"_s, tokens.date.isValid() ? tokens.date.toString(Qt::ISODate) : QString());
    out.replace(u"{camera}"_s, tokens.camera);
    return out.trimmed();
}

double meanLuminance(const QImage& image, const QRect& rect)
{
    const QRect area = rect & image.rect();
    if (area.isEmpty()) {
        return 0.0;
    }
    const QImage rgb = image.copy(area).convertToFormat(QImage::Format_RGB888);
    // A table per byte, then Rec.709 weights (the preview is sRGB).
    static const auto table = [] {
        std::array<double, 256> t{};
        for (std::size_t i = 0; i < t.size(); ++i) {
            t.at(i) = pfx::color::srgbDecode(static_cast<double>(i) / 255.0);
        }
        return t;
    }();
    double sum = 0.0;
    for (int y = 0; y < rgb.height(); ++y) {
        const uchar* p = rgb.constScanLine(y);
        for (int x = 0; x < rgb.width(); ++x, p += 3) {
            sum += (0.2126 * table.at(p[0])) + (0.7152 * table.at(p[1])) + (0.0722 * table.at(p[2]));
        }
    }
    return sum / (static_cast<double>(rgb.width()) * rgb.height());
}

QColor autoColor(double luminance)
{
    // White and black give the same contrast ratio at a luminance of about 0.18 (middle grey).
    const double white = 1.05 / (luminance + 0.05);
    const double black = (luminance + 0.05) / 0.05;
    return white >= black ? QColor(Qt::white) : QColor(0x11, 0x11, 0x11);
}

std::vector<QRectF> placements(const Watermark& w, const QSizeF& photo, const QSizeF& mark)
{
    if (mark.isEmpty() || photo.isEmpty()) {
        return {};
    }
    if (w.repeat) {
        // A grid over the photo's diagonal, so the turned grid still covers the corners.
        const double diagonal = std::hypot(photo.width(), photo.height());
        // At least 1/40 of the diagonal apart: a sliver of a logo must not become millions of
        // copies.
        const double stepX = std::max(mark.width() * 1.6, diagonal / 40.0);
        const double stepY = std::max(mark.height() * 4.0, diagonal / 40.0);
        std::vector<QRectF> out;
        const QPointF centre(photo.width() / 2, photo.height() / 2);
        const int columns = static_cast<int>(std::ceil(diagonal / stepX / 2)) + 1;
        const int rows = static_cast<int>(std::ceil(diagonal / stepY / 2)) + 1;
        for (int r = -rows; r <= rows; ++r) {
            const double shift = (r % 2 == 0) ? 0.0 : stepX / 2; // bricks, not a plain grid
            for (int c = -columns; c <= columns; ++c) {
                const QPointF at = centre + QPointF((c * stepX) + shift, r * stepY);
                out.emplace_back(at - QPointF(mark.width() / 2, mark.height() / 2), mark);
            }
        }
        return out;
    }
    const double margin = w.margin * longEdge(photo);
    if (w.position) {
        const QPointF c(w.position->x() * photo.width(), w.position->y() * photo.height());
        QRectF r(c - QPointF(mark.width() / 2, mark.height() / 2), mark);
        // Kept inside the photo.
        r.moveLeft(std::clamp(r.left(), 0.0, std::max(0.0, photo.width() - mark.width())));
        r.moveTop(std::clamp(r.top(), 0.0, std::max(0.0, photo.height() - mark.height())));
        return {r};
    }
    const int col = std::clamp(w.anchor, 0, 8) % 3;
    const int row = std::clamp(w.anchor, 0, 8) / 3;
    // Start, middle or end of a span, `margin` in from its edges.
    const auto along = [margin](int third, double span, double length) {
        if (third == 0) {
            return margin;
        }
        return third == 1 ? (span - length) / 2 : span - margin - length;
    };
    const double x = along(col, photo.width(), mark.width());
    const double y = along(row, photo.height(), mark.height());
    return {QRectF(QPointF(x, y), mark)};
}

void paintWatermark(QPainter& painter, const Watermark& w, const QSize& photo, const WatermarkTokens& tokens,
                    const LuminanceUnder& luminance)
{
    const QString text = expandTokens(w.text, tokens);
    if (w.kind == Watermark::Kind::Text && text.isEmpty()) {
        return;
    }
    const QSizeF mark = markSize(w, QSizeF(photo), text);
    const std::vector<QRectF> rects = placements(w, QSizeF(photo), mark);
    if (rects.empty()) {
        return;
    }
    painter.save();
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.setOpacity(std::clamp(w.opacity, 0.0, 1.0));
    if (w.repeat) {
        const QPointF centre(photo.width() / 2.0, photo.height() / 2.0);
        painter.translate(centre);
        painter.rotate(w.angle);
        painter.translate(-centre);
    }

    // The colour: chosen, or Auto from what lies under the mark (one choice for the whole grid).
    QColor color = w.color;
    if (w.colorMode == Watermark::ColorMode::White) {
        color = Qt::white;
    } else if (w.colorMode == Watermark::ColorMode::Black) {
        color = QColor(0x11, 0x11, 0x11);
    } else if (w.colorMode == Watermark::ColorMode::Auto) {
        const QRect under = w.repeat ? QRect(QPoint(0, 0), photo) : rects.front().toAlignedRect();
        color = autoColor(luminance ? luminance(under) : 0.0);
    }
    const QColor shadow = color.lightnessF() > 0.5F ? QColor(0, 0, 0, 110) : QColor(255, 255, 255, 110);

    if (w.kind == Watermark::Kind::Logo) {
        const QImage logo = logoImage(w.logo, mark.toSize());
        for (const QRectF& r : rects) {
            painter.drawImage(r.topLeft(), logo);
        }
        painter.restore();
        return;
    }
    painter.setFont(markFont(w, w.size * longEdge(QSizeF(photo))));
    const double soft = std::max(1.0, mark.height() * 0.05);
    for (const QRectF& r : rects) {
        if (w.shadow) {
            // A soft shadow from a ring of faint copies: no blur pass, any size.
            painter.setPen(QColor(shadow.red(), shadow.green(), shadow.blue(), shadow.alpha() / 4));
            for (int i = 0; i < 8; ++i) {
                const double a = i * std::numbers::pi / 4;
                painter.drawText(
                    r.translated((std::cos(a) * soft) + (soft * 0.5), (std::sin(a) * soft) + soft),
                    Qt::AlignCenter | Qt::TextSingleLine, text);
            }
        }
        painter.setPen(color);
        painter.drawText(r, Qt::AlignCenter | Qt::TextSingleLine, text);
    }
    painter.restore();
}

WatermarkOverlay renderWatermark(const Watermark& w, const QSize& photo, const WatermarkTokens& tokens,
                                 const LuminanceUnder& luminance)
{
    const QString text = expandTokens(w.text, tokens);
    const QSizeF mark = markSize(w, QSizeF(photo), text);
    const std::vector<QRectF> rects = placements(w, QSizeF(photo), mark);
    if (rects.empty() || (w.kind == Watermark::Kind::Text && text.isEmpty())) {
        return {};
    }
    // Only the part of the photo the mark touches (all of it when repeating), with room for the
    // shadow.
    QRect area(QPoint(0, 0), photo);
    if (!w.repeat) {
        const int pad = static_cast<int>(std::ceil(mark.height() * 0.2)) + 2;
        area = rects.front().toAlignedRect().adjusted(-pad, -pad, pad, pad) & QRect(QPoint(0, 0), photo);
    }
    if (area.isEmpty()) {
        return {};
    }
    WatermarkOverlay out{QImage(area.size(), QImage::Format_RGBA8888_Premultiplied), area.topLeft()};
    out.image.fill(Qt::transparent);
    QPainter painter(&out.image);
    painter.translate(-area.topLeft());
    paintWatermark(painter, w, photo, tokens, luminance);
    return out;
}

QByteArray watermarkToJson(const Watermark& w)
{
    static const QStringList kinds = {u"text"_s, u"logo"_s};
    static const QStringList colors = {u"auto"_s, u"white"_s, u"black"_s, u"custom"_s};
    QJsonObject body{
        {u"type"_s, kinds.at(static_cast<int>(w.kind))},
        {u"text"_s, w.text},
        {u"logo"_s, QFileInfo(w.logo).fileName()}, // beside the file, in the library
        {u"font"_s, w.font},
        {u"colorMode"_s, colors.at(static_cast<int>(w.colorMode))},
        {u"color"_s, w.color.name(QColor::HexRgb)},
        {u"size"_s, w.size},
        {u"opacity"_s, w.opacity},
        {u"margin"_s, w.margin},
        {u"anchor"_s, w.anchor},
        {u"repeat"_s, w.repeat},
        {u"angle"_s, w.angle},
        {u"shadow"_s, w.shadow},
    };
    if (w.position) {
        body.insert(u"position"_s, positionJson(w.position));
    }
    const QJsonObject root{
        {u"format"_s, u"pixelfx"_s}, {u"kind"_s, u"watermark"_s}, {u"version"_s, kVersion},
        {u"name"_s, w.name},         {u"body"_s, body},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<Watermark> watermarkFromJson(const QByteArray& json, QString* error)
{
    const auto fail = [error](const QString& why) -> std::optional<Watermark> {
        if (error != nullptr) {
            *error = why;
        }
        return std::nullopt;
    };
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parse);
    const QJsonObject root = doc.object();
    if (parse.error != QJsonParseError::NoError || root.value(u"format"_s).toString() != u"pixelfx"_s ||
        root.value(u"kind"_s).toString() != u"watermark"_s) {
        return fail(QCoreApplication::translate("Watermark", "This is not a PixelFx watermark."));
    }
    if (root.value(u"version"_s).toInt() > kVersion) {
        return fail(QCoreApplication::translate("Watermark", "This watermark was made by a newer PixelFx."));
    }
    const QJsonObject b = root.value(u"body"_s).toObject();
    Watermark w;
    w.name = root.value(u"name"_s).toString().trimmed();
    w.kind = b.value(u"type"_s).toString() == u"logo"_s ? Watermark::Kind::Logo : Watermark::Kind::Text;
    w.text = b.value(u"text"_s).toString(w.text).left(500);
    w.logo = b.value(u"logo"_s).toString();
    w.font = b.value(u"font"_s).toString();
    const QString mode = b.value(u"colorMode"_s).toString();
    static const QStringList modes = {u"auto"_s, u"white"_s, u"black"_s, u"custom"_s};
    w.colorMode = static_cast<Watermark::ColorMode>(std::max(0, static_cast<int>(modes.indexOf(mode))));
    w.color = QColor::fromString(b.value(u"color"_s).toString());
    if (!w.color.isValid()) {
        w.color = Qt::white;
    }
    // Values out of range are brought into it rather than refused.
    w.size = std::clamp(b.value(u"size"_s).toDouble(w.size), 0.005, 0.25);
    w.opacity = std::clamp(b.value(u"opacity"_s).toDouble(w.opacity), 0.0, 1.0);
    w.margin = std::clamp(b.value(u"margin"_s).toDouble(w.margin), 0.0, 0.2);
    w.anchor = std::clamp(b.value(u"anchor"_s).toInt(w.anchor), 0, 8);
    w.repeat = b.value(u"repeat"_s).toBool(false);
    w.angle = std::clamp(b.value(u"angle"_s).toDouble(w.angle), -90.0, 90.0);
    w.shadow = b.value(u"shadow"_s).toBool(true);
    if (const QJsonObject p = b.value(u"position"_s).toObject(); p.contains(u"x"_s) && p.contains(u"y"_s)) {
        w.position = QPointF(std::clamp(p.value(u"x"_s).toDouble(), 0.0, 1.0),
                             std::clamp(p.value(u"y"_s).toDouble(), 0.0, 1.0));
    }
    return w;
}

WatermarkLibrary::WatermarkLibrary(QString folder)
    : m_folder(std::move(folder))
{}

QString WatermarkLibrary::defaultFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/watermarks"_s;
}

std::optional<Watermark> WatermarkLibrary::load(const QString& path)
{
    constexpr qint64 kMaxBytes = qint64{1024} * 1024; // a watermark is a few hundred bytes
    QFile file(path);
    if (file.size() > kMaxBytes || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    auto w = watermarkFromJson(file.readAll());
    if (w) {
        if (w->name.isEmpty()) {
            w->name = QFileInfo(path).completeBaseName();
        }
        // The logo lives beside the file: only its name is stored, never a path elsewhere.
        if (!w->logo.isEmpty()) {
            w->logo = QFileInfo(path).absolutePath() + u'/' + QFileInfo(w->logo).fileName();
        }
    }
    return w;
}

std::vector<WatermarkLibrary::Entry> WatermarkLibrary::entries() const
{
    std::vector<Entry> out;
    const QDir dir(m_folder);
    for (const QFileInfo& file : dir.entryInfoList({u"*.pfxwatermark"_s}, QDir::Files)) {
        if (auto w = load(file.absoluteFilePath())) {
            out.push_back({file.absoluteFilePath(), std::move(*w)});
        }
    }
    std::ranges::sort(out, [](const Entry& a, const Entry& b) {
        return a.watermark.name.localeAwareCompare(b.watermark.name) < 0;
    });
    return out;
}

QString WatermarkLibrary::save(const Watermark& w, const QString& path) const
{
    if (w.name.trimmed().isEmpty() || !QDir().mkpath(m_folder)) {
        return {};
    }
    // A new watermark gets a file name of its own; an edited one keeps its file.
    QString target = path;
    if (target.isEmpty()) {
        const QString base = m_folder + u'/' + fileSafeName(w.name, u"Watermark"_s);
        target = base + u".pfxwatermark"_s;
        for (int n = 2; QFileInfo::exists(target); ++n) {
            target = base + u"-%1.pfxwatermark"_s.arg(n);
        }
    }
    Watermark stored = w;
    stored.name = w.name.trimmed();
    if (w.kind == Watermark::Kind::Logo && !w.logo.isEmpty()) {
        // Copied in beside the file, so the original may move or go; written to a new name
        // first, so a failed copy never loses the logo there was.
        const QString logo = QFileInfo(target).absolutePath() + u'/' + QFileInfo(target).completeBaseName() +
                             u'.' + QFileInfo(w.logo).suffix().toLower();
        if (QFileInfo(w.logo).absoluteFilePath() != QFileInfo(logo).absoluteFilePath()) {
            const QString fresh = logo + u".new"_s;
            QFile::remove(fresh);
            if (!QFile::copy(w.logo, fresh)) {
                return {};
            }
            QFile::remove(logo);
            if (!QFile::rename(fresh, logo)) {
                return {};
            }
        }
        stored.logo = logo;
    }
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(watermarkToJson(stored));
    return file.commit() ? target : QString();
}

bool WatermarkLibrary::remove(const QString& path) const
{
    if (QFileInfo(path).absolutePath() != QFileInfo(m_folder).absoluteFilePath()) {
        return false;
    }
    const auto w = load(path);
    if (w && !w->logo.isEmpty()) {
        QFile::remove(w->logo);
    }
    return QFile::remove(path);
}

} // namespace pixelfx::core
