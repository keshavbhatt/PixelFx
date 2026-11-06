#pragma once

#include <QHash>
#include <QImage>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QMenu;
class QScrollArea;
class QToolButton;

namespace pixelfx::ui {

class FilmTile;

/// Every open photo along the bottom: the
/// current one framed, edited ones marked, Ctrl or Shift click to select several (ticks show
/// then), a + tile to add photos, and "2 of 6" or "4 selected · Select all · Clear". A tile's ×
/// and its right-click menu close photos (close-photo.html).
class Filmstrip : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Filmstrip)

public:
    explicit Filmstrip(QWidget* parent = nullptr);
    ~Filmstrip() override;

    void setFiles(const QStringList& files);
    [[nodiscard]] const QStringList& files() const { return m_files; }
    void setCurrent(int index);
    [[nodiscard]] int current() const { return m_current; }
    /// Kept for the photo while it is in the strip, also across setFiles.
    void setThumbnail(const QString& path, const QImage& image, bool edited);
    /// Selected photos, the current one included when there is a selection; empty otherwise.
    [[nodiscard]] QList<int> selection() const;
    void selectAll();
    void clearSelection();
    /// Ctrl or Shift click on tile `index` (tests use it directly).
    void toggle(int index, bool range);
    [[nodiscard]] QList<FilmTile*> tiles() const;
    /// The right-click menu of tile `index` (tests use it directly).
    [[nodiscard]] QMenu* tileMenu(int index);

    static constexpr int kHeight = 92;

Q_SIGNALS:
    void currentRequested(int index);
    void addRequested();
    void selectionChanged(int count);
    /// Close the photos at `indices`, in order.
    void closeRequested(const QList<int>& indices);

private:
    void rebuild();
    void refresh();
    struct Thumbnail
    {
        QImage image;
        bool edited = false;
    };
    QHash<QString, Thumbnail> m_thumbnails;
    QStringList m_files;
    int m_current = -1;
    int m_anchor = -1;
    QSet<int> m_selected;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_row = nullptr;
    QHBoxLayout* m_tiles = nullptr;
    QLabel* m_info = nullptr;
    QWidget* m_selectionBar = nullptr;
};

/// One photo of the strip.
class FilmTile : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FilmTile)

public:
    FilmTile(int index, const QString& path, QWidget* parent = nullptr);
    ~FilmTile() override;
    [[nodiscard]] int index() const { return m_index; }
    [[nodiscard]] const QString& path() const { return m_path; }
    void setThumbnail(const QImage& image, bool edited);
    [[nodiscard]] bool isEdited() const { return m_edited; }
    void setState(bool current, bool selected, bool ticks);
    /// The × in the top right corner, shown under the pointer.
    [[nodiscard]] QToolButton* closeButton() const { return m_close; }

    static constexpr int kWidth = 92;
    static constexpr int kHeight = 64;

Q_SIGNALS:
    void clicked(Qt::KeyboardModifiers modifiers);
    void closeClicked();
    void menuRequested(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    int m_index;
    QString m_path;
    QToolButton* m_close;
    QImage m_image;
    bool m_edited = false;
    bool m_current = false;
    bool m_selected = false;
    bool m_ticks = false;
};

} // namespace pixelfx::ui
