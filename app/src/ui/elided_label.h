#pragma once

#include <QLabel>

class QFontMetrics;

namespace pixelfx::ui {

/// A one-line label that ends its text with "..." when it is narrower than the text. It asks for
/// no width of its own.
class ElidedLabel : public QLabel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ElidedLabel)

public:
    explicit ElidedLabel(QWidget* parent = nullptr);
    ~ElidedLabel() override;

    [[nodiscard]] QSize minimumSizeHint() const override;
    /// Where the "..." goes; the end by default.
    void setElideMode(Qt::TextElideMode mode);

protected:
    void paintEvent(QPaintEvent* event) override;
    /// The text as drawn in `width` px: cut at the elide mode's place when it does not fit.
    [[nodiscard]] virtual QString shownText(int width) const;

private:
    Qt::TextElideMode m_mode = Qt::ElideRight;
};

/// A folder or file path on one line: the home folder as "~"; when it is too long, its start and
/// its last part are kept with as many of the folders nearest the end as fit; the full path in
/// the tooltip.
class PathLabel : public ElidedLabel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PathLabel)

public:
    /// \a emptyText is shown when the path is empty.
    explicit PathLabel(QString emptyText = {}, QWidget* parent = nullptr);
    ~PathLabel() override;

    void setPath(const QString& path);
    [[nodiscard]] QString path() const { return m_path; }

    /// "~/Pictures/PixelFx" for a path in the home folder, in the platform's separators.
    [[nodiscard]] static QString shortPath(const QString& path);
    /// `path` (with "/" separators) in `width` px of `metrics`, in the platform's separators:
    /// whole when it fits; else its start ("~", "/", "C:") and its last part, with as many of the
    /// folders before the last as fit and "…" for the rest ("~/…/2026/Lake trip"); a last part too
    /// long by itself is cut in its middle.
    [[nodiscard]] static QString fitted(const QString& path, const QFontMetrics& metrics, int width);
    /// How wide the path is drawn when the label is `width` px wide: for a control right after it.
    [[nodiscard]] int shownWidth(int width) const;

protected:
    [[nodiscard]] QString shownText(int width) const override;

private:
    QString m_emptyText;
    QString m_path;
};

} // namespace pixelfx::ui
