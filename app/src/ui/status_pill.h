#pragma once

#include <QFrame>

class QLabel;

namespace pixelfx::ui {

/// A small rounded status: "Saved", "Waiting", "Not saved", "Saved 39%". Its colour follows its
/// kind; hidden while it has no text.
class StatusPill : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatusPill)

public:
    enum class Kind
    {
        Plain,
        Accent,
        Ok,
        Bad,
    };

    explicit StatusPill(QWidget* parent = nullptr);
    ~StatusPill() override;

    /// `glyph` is an icon name, or empty for none.
    void setStatus(Kind kind, const QString& text, const QString& glyph = {});
    [[nodiscard]] QString text() const;

private:
    QLabel* m_glyph;
    QLabel* m_text;
};

} // namespace pixelfx::ui
