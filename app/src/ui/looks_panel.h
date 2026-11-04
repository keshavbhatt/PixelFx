#pragma once

#include "core/looks.h"

#include <QImage>
#include <QWidget>

#include <vector>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QLineEdit;
class QSlider;
class QVBoxLayout;

namespace pixelfx::ui {

/// One look as a picture of the photo with it (mock editor.html .looks .tile).
class LookTile : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LookTile)

public:
    LookTile(QString id, QString name, bool mine, QWidget* parent = nullptr);
    ~LookTile() override;

    [[nodiscard]] const QString& id() const { return m_id; }
    [[nodiscard]] const QString& name() const { return m_name; }
    void setImage(const QImage& image);
    void setSelected(bool selected);
    [[nodiscard]] bool isSelected() const { return m_selected; }

Q_SIGNALS:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QString m_id;
    QString m_name;
    bool m_mine = false;
    bool m_selected = false;
    QImage m_image;
};

/// The Looks tab of the left panel (mock editor.html): search, category chips, the user's
/// looks and the built-in ones as tiles of the photo, and the strength of the applied look.
class LooksPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LooksPanel)

public:
    explicit LooksPanel(QWidget* parent = nullptr);
    ~LooksPanel() override;

    void setEntries(std::vector<core::LookEntry> entries);
    [[nodiscard]] const std::vector<core::LookEntry>& entries() const { return m_entries; }
    /// The photo's look and strength, from its settings (after undo, opening, a preset).
    void setCurrent(const QString& id, double strength);
    [[nodiscard]] const QString& current() const { return m_current; }
    /// The picture for entry `index`; index -1 is "Original" (no look).
    void setThumbnail(int index, const QImage& image);
    [[nodiscard]] std::vector<LookTile*> visibleTiles() const;
    [[nodiscard]] QSlider* strengthSlider() const { return m_strength; }

Q_SIGNALS:
    /// A tile was chosen: its id (empty for Original) and the look's own strength.
    void lookChosen(const QString& id, int strength);
    /// The strength slider moves (live) and is let go (a history step).
    void strengthMoved(int strength);
    void strengthReleased(int strength);
    void importRequested();

private:
    void rebuild();
    QWidget* buildStrength();
    void addSection(const QString& title, QWidget* extra, const std::vector<int>& indexes, bool withOriginal);
    [[nodiscard]] bool matches(const core::LookEntry& e) const;

    std::vector<core::LookEntry> m_entries;
    std::vector<QImage> m_thumbnails;
    QImage m_original;
    QString m_current;
    QLineEdit* m_search = nullptr;
    QButtonGroup* m_chips = nullptr;
    QWidget* m_listBox = nullptr;
    QVBoxLayout* m_list = nullptr;
    QSlider* m_strength = nullptr;
    QLabel* m_strengthValue = nullptr;
    QLabel* m_strengthName = nullptr;
};

} // namespace pixelfx::ui
