#pragma once

#include <QWidget>

class QLabel;

namespace pixelfx::ui {

class ElidedLabel;

/// A one-line message at the left of a sheet's footer: the sheet's note ("Applied on export only"),
/// or for a while what just happened ("Left out of the .cube: Grain") or went wrong ("That LUT
/// could not be read"), with an icon. The whole text is its tooltip when it does not fit.
class MessageLine : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MessageLine)

public:
    explicit MessageLine(QWidget* parent = nullptr);
    ~MessageLine() override;

    /// The text shown when there is no message (may be empty).
    void setNote(const QString& note);
    void showInfo(const QString& text);
    void showError(const QString& text);
    /// Back to the note.
    void clear();
    [[nodiscard]] QString text() const;
    [[nodiscard]] bool isError() const { return m_kind == Kind::Error; }

private:
    enum class Kind
    {
        Note,
        Info,
        Error
    };
    void show(Kind kind, const QString& text);

    Kind m_kind = Kind::Note;
    QString m_note;
    QLabel* m_icon;
    ElidedLabel* m_text;
};

} // namespace pixelfx::ui
