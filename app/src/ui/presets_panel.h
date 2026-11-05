#pragma once

#include "core/presets.h"

#include <QImage>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QVBoxLayout;

class QButtonGroup;
class QStackedWidget;

namespace pixelfx::ui {

class LooksPanel;

class PresetRow;

/// The Presets tab of the left panel: the user's presets by group,
/// imported ones, the built-in ones; search, import, and Save current as preset. A click
/// applies a preset, hovering previews it on the photo, the row menu manages it. The Looks tab
/// arrives with looks (M4).
class PresetsPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresetsPanel)

public:
    explicit PresetsPanel(QWidget* parent = nullptr);
    ~PresetsPanel() override;

    /// Shows `entries`, grouped as given, filtered by the search text.
    void setEntries(std::vector<core::PresetEntry> entries);
    [[nodiscard]] const std::vector<core::PresetEntry>& entries() const { return m_entries; }
    /// A small picture of the photo with the preset at `index` of entries() applied.
    void setThumbnail(std::size_t index, const QImage& image);
    [[nodiscard]] QList<PresetRow*> rows() const;
    /// With several photos selected the panel says where a click goes ("Applies to 4 photos").
    void setTargetCount(int count);

Q_SIGNALS:
    void applyRequested(const pixelfx::core::PresetEntry& entry);
    /// Hovering a row (null `entry`: the pointer left the rows).
    void previewRequested(const pixelfx::core::PresetEntry* entry);
    void saveRequested();
    void importRequested();
    void renameRequested(const pixelfx::core::PresetEntry& entry);
    void updateRequested(const pixelfx::core::PresetEntry& entry);
    void exportRequested(const pixelfx::core::PresetEntry& entry);
    void duplicateRequested(const pixelfx::core::PresetEntry& entry);
    void showFileRequested(const pixelfx::core::PresetEntry& entry);
    void deleteRequested(const pixelfx::core::PresetEntry& entry);

public:
    /// The Looks tab's panel.
    [[nodiscard]] LooksPanel* looksPanel() const { return m_looks; }
    /// 0 Looks, 1 Presets.
    void showTab(int index);
    [[nodiscard]] int currentTab() const;

private:
    QHBoxLayout* buildHead();
    QHBoxLayout* buildTabs();
    void rebuild();
    void showMenu(PresetRow* row);

    std::vector<core::PresetEntry> m_entries;
    std::vector<QImage> m_thumbnails;
    QLineEdit* m_search = nullptr;
    QLabel* m_target = nullptr;
    QWidget* m_listBox = nullptr;
    QVBoxLayout* m_list = nullptr;
    LooksPanel* m_looks = nullptr;
    QStackedWidget* m_pages = nullptr;
    QButtonGroup* m_tabs = nullptr;
    int m_targetCount = 1;
};

/// One preset: its picture on the photo, name, what it holds, and a menu button.
class PresetRow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresetRow)

public:
    PresetRow(std::size_t index, const core::PresetEntry& entry, QWidget* parent = nullptr);
    ~PresetRow() override;
    [[nodiscard]] std::size_t index() const { return m_index; }
    [[nodiscard]] const QString& name() const { return m_name; }
    void setThumbnail(const QImage& image);
    [[nodiscard]] QWidget* menuButton() const { return m_more; }

Q_SIGNALS:
    void clicked();
    void hovered(bool inside);
    void menuRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    std::size_t m_index;
    QString m_name;
    QImage m_thumbnail;
    bool m_hover = false;
    QWidget* m_more = nullptr;
};

} // namespace pixelfx::ui
