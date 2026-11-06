#include "ui/elided_label.h"

#include <QDir>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

ElidedLabel::ElidedLabel(QWidget* parent)
    : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

ElidedLabel::~ElidedLabel() = default;

QSize ElidedLabel::minimumSizeHint() const
{
    return {0, QLabel::minimumSizeHint().height()};
}

void ElidedLabel::setElideMode(Qt::TextElideMode mode)
{
    m_mode = mode;
    update();
}

void ElidedLabel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
    const QRect area = contentsRect();
    const QString shown = shownText(area.width());
    style()->drawItemText(&painter, area, static_cast<int>(alignment()), palette(), isEnabled(), shown,
                          foregroundRole());
}

QString ElidedLabel::shownText(int width) const
{
    return fontMetrics().elidedText(text(), m_mode, width);
}

PathLabel::PathLabel(QString emptyText, QWidget* parent)
    : ElidedLabel(parent)
    , m_emptyText(std::move(emptyText))
{
    setElideMode(Qt::ElideMiddle);
    setText(m_emptyText);
}

PathLabel::~PathLabel() = default;

void PathLabel::setPath(const QString& path)
{
    m_path = path;
    setText(path.isEmpty() ? m_emptyText : shortPath(path));
    setToolTip(QDir::toNativeSeparators(path));
}

namespace {

// "~/Pictures/PixelFx" for a path in the home folder, with "/" separators.
QString homeRelative(const QString& path)
{
    const QString clean = QDir::fromNativeSeparators(path);
    const QString home = QDir::homePath();
    const bool inHome = clean == home || clean.startsWith(home + u'/');
    return inHome ? u"~"_s + clean.mid(home.size()) : clean;
}

} // namespace

QString PathLabel::shortPath(const QString& path)
{
    return QDir::toNativeSeparators(homeRelative(path));
}

QString PathLabel::fitted(const QString& path, const QFontMetrics& metrics, int width)
{
    const auto fits = [&metrics, width](const QString& text) {
        return metrics.horizontalAdvance(text) <= width;
    };
    const QString whole = QDir::toNativeSeparators(path);
    QStringList parts = path.split(u'/');
    if (parts.size() > 2 && parts.last().isEmpty()) {
        parts.removeLast(); // a trailing separator
    }
    if (fits(whole) || parts.size() < 3) {
        return metrics.elidedText(whole, Qt::ElideMiddle, width);
    }
    const QString& last = parts.last();
    const QStringList middle = parts.mid(1, parts.size() - 2);
    const QString head = QDir::toNativeSeparators(parts.first() + u"/\u2026/"_s);
    // The folders nearest the end matter most: drop them from the start of the middle.
    for (qsizetype keep = middle.size() - 1; keep > 0; --keep) {
        const QString shown =
            head + QDir::toNativeSeparators((middle.mid(middle.size() - keep) << last).join(u'/'));
        if (fits(shown)) {
            return shown;
        }
    }
    if (fits(head + last)) {
        return head + last;
    }
    return head +
           metrics.elidedText(last, Qt::ElideMiddle, std::max(0, width - metrics.horizontalAdvance(head)));
}

int PathLabel::shownWidth(int width) const
{
    return fontMetrics().horizontalAdvance(shownText(width)) + contentsMargins().left() +
           contentsMargins().right();
}

QString PathLabel::shownText(int width) const
{
    if (m_path.isEmpty()) {
        return ElidedLabel::shownText(width);
    }
    return fitted(homeRelative(m_path), fontMetrics(), width);
}

} // namespace pixelfx::ui
