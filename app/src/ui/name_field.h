#pragma once

#include "core/export_options.h"

#include <QHash>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QGridLayout;

namespace pixelfx::ui {

/// A file name template: the field, a + Token menu that inserts at
/// the cursor, and under it the first three names it gives ("IMG_0412.HEIC → trip-001.jpg"),
/// "and N more", and a line when every photo would get the same name.
class NameField : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(NameField)

public:
    /// Makes the file name for `source` under `nameTemplate`.
    using Namer = std::function<QString(const QString& nameTemplate, const QString& source,
                                        const core::NameFacts& facts)>;

    explicit NameField(QWidget* parent = nullptr);
    ~NameField() override;

    void setTemplate(const QString& nameTemplate);
    [[nodiscard]] QString nameTemplate() const;
    /// The tokens + Token offers (core::nameTokens by default).
    void setTokens(const QStringList& tokens);
    /// The list the names are for: its first files, how many there are, and how a name is made.
    void setFiles(const QStringList& files, Namer namer);
    /// Shows the names again (an option they depend on changed).
    void refresh();
    [[nodiscard]] QLineEdit* field() const { return m_field; }
    [[nodiscard]] QPushButton* tokenButton() const { return m_token; }
    /// The names on show, as "source → name" rows.
    [[nodiscard]] QStringList shownNames() const;
    [[nodiscard]] bool showsClash() const;
    /// `count` photos would be saved over their originals (If it exists: Overwrite): a line says
    /// so; 0 hides it.
    void setOverwrites(int count);
    [[nodiscard]] bool showsOverwrites() const;

Q_SIGNALS:
    /// The template was edited (the field left, or a token inserted).
    void edited(const QString& nameTemplate);

private:
    void showTokenMenu();
    /// An alert icon and `text` in warning colour, laid in `line`; returns the text.
    static QLabel* warningLine(QWidget* line, const QString& text);
    [[nodiscard]] core::NameFacts factsOf(int index);
    [[nodiscard]] static QString meaningOf(const QString& token);

    QLineEdit* m_field;
    QPushButton* m_token;
    QGridLayout* m_rows;
    QLabel* m_more;
    QWidget* m_clash;
    QWidget* m_overwrites;
    QLabel* m_overwritesText = nullptr;
    QStringList m_tokens;
    QStringList m_files;
    Namer m_namer;
    QHash<QString, core::NameFacts> m_facts; ///< what each file's Exif says, read once
};

} // namespace pixelfx::ui
