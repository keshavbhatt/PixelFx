#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

class QTimer;

namespace pixelfx::ui {

class PhotoTile;

/// The hover preview of the Looks and Presets tabs (editor-presets-catalog.html): resting on a
/// tile for kWait previews it, a bar filling along the tile's picture meanwhile; once a preview
/// shows, the next tile takes kNext; leaving the tiles ends it a moment later, so moving between
/// neighbours does not flash back to the photo.
class HoverPreview : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HoverPreview)

public:
    explicit HoverPreview(QObject* parent = nullptr);
    ~HoverPreview() override;

    /// The pointer (or the keyboard focus) came onto `tile`, the panel's item `index`, or left it.
    void hover(PhotoTile* tile, int index, bool inside);
    /// Ends the preview and any wait for one (a click, Escape, the list rebuilt).
    void end();
    [[nodiscard]] bool isPreviewing() const { return m_previewing; }

    static constexpr int kWait = 350;
    static constexpr int kNext = 150;

Q_SIGNALS:
    /// Show item `index` on the photo.
    void started(int index);
    /// Back to the photo's own settings.
    void ended();

private:
    void tick();

    QPointer<PhotoTile> m_tile;
    int m_index = -1;
    int m_delay = kWait;
    QElapsedTimer m_clock;
    QTimer* m_tick;
    QTimer* m_leave;
    bool m_previewing = false;
};

} // namespace pixelfx::ui
