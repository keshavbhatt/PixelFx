#pragma once

#include <QComboBox>

namespace pixelfx::ui {

/// Where exported photos go, as a list (`export.html`, Save to): Next to the photo, the default
/// folder, the last other folder chosen, and Choose a folder..., which opens the folder picker.
/// An empty folder means next to the photo.
class SaveFolderList : public QComboBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SaveFolderList)

public:
    explicit SaveFolderList(QWidget* parent = nullptr);
    ~SaveFolderList() override;

    /// `folder` is the choice shown (empty: next to the photo); `recent` the last folder chosen
    /// that is not `defaultFolder`.
    void setChoices(const QString& defaultFolder, const QString& folder, const QString& recent);

    [[nodiscard]] const QString& folder() const { return m_folder; }
    /// The last folder chosen other than the default, kept in the list; empty for none.
    [[nodiscard]] const QString& recent() const { return m_recent; }

    /// "Pictures › PixelFx": a folder by its parent and its name.
    [[nodiscard]] static QString shortName(const QString& folder);

Q_SIGNALS:
    void folderChosen(const QString& folder);

protected:
    /// The choice shown shortened in the middle when it does not fit, so the folder's own name
    /// at its end stays; the open list shows each whole.
    void paintEvent(QPaintEvent* event) override;

private:
    void rebuild();
    void choose(int index);

    QString m_default;
    QString m_folder;
    QString m_recent;
};

} // namespace pixelfx::ui
