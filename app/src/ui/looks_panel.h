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

class PhotoTile;
class SliderRow;

/// The Looks tab of the left panel: search, category chips, the user's
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
    [[nodiscard]] std::vector<PhotoTile*> visibleTiles() const;
    [[nodiscard]] QSlider* strengthSlider() const;
    /// A look's preview shows on the photo (after the wait on its tile).
    [[nodiscard]] bool isPreviewing() const;
    /// Ends the preview and any wait for one.
    void endPreview();

Q_SIGNALS:
    /// A tile was chosen: its id (empty for Original) and the look's own strength.
    void lookChosen(const QString& id, int strength);
    /// The strength slider moves (live) and is let go (a history step).
    void strengthMoved(int strength);
    void strengthReleased(int strength);
    void newLookRequested();
    /// After resting on a tile: show look `id` (empty: no look) at `strength` on the photo.
    void previewRequested(const QString& id, int strength);
    /// Back to the photo's own look.
    void previewEnded();
    void editLookRequested(const QString& id);
    void exportLookRequested(const QString& id);
    void deleteLookRequested(const QString& id);

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
    SliderRow* m_strength = nullptr;
    QLabel* m_appliedName = nullptr;
    class HoverPreview* m_hover = nullptr;
    std::vector<QString> m_tileIds; // the look of each tile on show, by HoverPreview index
    /// The look's own strength, the one a click applies.
    [[nodiscard]] int defaultStrength(const QString& id) const;
};

} // namespace pixelfx::ui
