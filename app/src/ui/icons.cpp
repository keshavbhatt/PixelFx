#include "ui/icons.h"

#include "ui/style.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QLabel>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

namespace pixelfx::ui::icons {

namespace {

QByteArray tintedSvg(const QString& name, const QColor& color)
{
    QFile file(u":/icons/ui/"_s + name + u".svg"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svg = file.readAll();
    svg.replace("#000000", color.name(QColor::HexRgb).toLatin1());
    return svg;
}

QPixmap render(const QByteArray& svg, int size, qreal dpr)
{
    if (size <= 0) {
        return {}; // a button with no room for its icon asks for none
    }
    QSvgRenderer renderer(svg);
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    if (renderer.isValid()) {
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(0, 0, size, size));
    }
    return pm;
}

QString cacheKey(const QString& name, const QColor& color, int size, qreal dpr)
{
    return name + u'|' + color.name(QColor::HexArgb) + u'|' + QString::number(size) + u'|' +
           QString::number(dpr);
}

} // namespace

QPixmap pixmap(const QString& name, const QColor& color, int size, qreal devicePixelRatio)
{
    static QHash<QString, QPixmap> cache;
    const QString key = cacheKey(name, color, size, devicePixelRatio);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }
    const QPixmap pm = render(tintedSvg(name, color), size, devicePixelRatio);
    cache.insert(key, pm);
    return pm;
}

namespace {

// Draws a glyph when asked, in the colour of the token it was made with, so icons follow a
// theme switch; colours outside the tokens stay as given.
class GlyphEngine : public QIconEngine
{
public:
    GlyphEngine(QString name, const QColor& normal, const QColor& disabled, const QColor& on)
        : m_name(std::move(name))
        , m_normal(normal, tokenIndex(normal))
        , m_disabled(disabled, -1)
        , m_on(on, on.isValid() ? tokenIndex(on) : -1)
        , m_disabledFollows(!disabled.isValid() || disabled.rgb() == normal.rgb())
    {
        if (!m_disabledFollows) {
            m_disabled.second = tokenIndex(disabled);
        }
    }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() != nullptr ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        return icons::pixmap(m_name, colourFor(mode, state), std::min(size.width(), size.height()), scale);
    }

    [[nodiscard]] QIconEngine* clone() const override { return new GlyphEngine(*this); }
    [[nodiscard]] QString key() const override { return QStringLiteral("pixelfx-glyph"); }

private:
    using Tinted = std::pair<QColor, int>; // the colour given, and its token (-1 for none)

    static QColor now(const Tinted& t) { return t.second >= 0 ? tokenColor(t.second) : t.first; }

    [[nodiscard]] QColor colourFor(QIcon::Mode mode, QIcon::State state) const
    {
        if (mode == QIcon::Disabled) {
            if (!m_disabledFollows) {
                return now(m_disabled);
            }
            QColor c = now(m_normal);
            c.setAlpha(90);
            return c;
        }
        if (state == QIcon::On && m_on.first.isValid()) {
            return now(m_on);
        }
        return now(m_normal);
    }

    QString m_name;
    Tinted m_normal;
    Tinted m_disabled;
    Tinted m_on;
    bool m_disabledFollows = true;
};

} // namespace

QIcon themed(const QString& name, const QColor& color, const QColor& disabledColor)
{
    return QIcon(new GlyphEngine(name, color, disabledColor, {}));
}

QIcon toggle(const QString& name, const QColor& color, const QColor& onColor, const QColor& disabledColor)
{
    return QIcon(new GlyphEngine(name, color, disabledColor, onColor));
}

void setGlyph(QLabel* label, const QString& name, const QColor& color, int size)
{
    label->setProperty("glyph", name);
    label->setProperty("glyphColor", color);
    label->setProperty("glyphSize", size);
    label->setPixmap(pixmap(name, color, size, label->devicePixelRatioF()));
}

QIcon brand()
{
    return QIcon(u":/icons/pixelfx.svg"_s);
}

QString tintedFile(const QString& name, const QColor& color)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/icons"_s;
    QDir().mkpath(dir);
    const QString path = dir + u'/' + name + u'-' + color.name(QColor::HexRgb).mid(1) + u".svg"_s;
    if (!QFile::exists(path)) {
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(tintedSvg(name, color));
            file.commit();
        }
    }
    return path;
}

} // namespace pixelfx::ui::icons
