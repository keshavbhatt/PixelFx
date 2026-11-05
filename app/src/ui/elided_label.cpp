#include "ui/elided_label.h"

#include <QDir>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

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
    const QString shown = fontMetrics().elidedText(text(), m_mode, area.width());
    style()->drawItemText(&painter, area, static_cast<int>(alignment()), palette(), isEnabled(), shown,
                          foregroundRole());
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

QString PathLabel::shortPath(const QString& path)
{
    const QString home = QDir::homePath();
    const bool inHome = path == home || path.startsWith(home + u'/');
    return QDir::toNativeSeparators(inHome ? u"~"_s + path.mid(home.size()) : path);
}

} // namespace pixelfx::ui
