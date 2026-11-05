#pragma once

#include <QLabel>

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

private:
    Qt::TextElideMode m_mode = Qt::ElideRight;
};

/// A folder or file path on one line: the home folder as "~", the middle elided so the last
/// folder stays readable, the full path in the tooltip.
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

private:
    QString m_emptyText;
    QString m_path;
};

} // namespace pixelfx::ui
