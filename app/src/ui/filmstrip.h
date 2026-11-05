#pragma once

#include <QImage>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QScrollArea;

namespace pixelfx::ui {

class FilmTile;

/// Every open photo along the bottom: the
/// current one framed, edited ones marked, Ctrl or Shift click to select several (ticks show
/// then), a + tile to add photos, and "2 of 6" or "4 selected · Select all · Clear".
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
    void setThumbnail(const QString& path, const QImage& image, bool edited) const;
    /// Selected photos, the current one included when there is a selection; empty otherwise.
    [[nodiscard]] QList<int> selection() const;
    void selectAll();
    void clearSelection();
    /// Ctrl or Shift click on tile `index` (tests use it directly).
    void toggle(int index, bool range);
    [[nodiscard]] QList<FilmTile*> tiles() const;

    static constexpr int kHeight = 92;

Q_SIGNALS:
    void currentRequested(int index);
    void addRequested();
    void selectionChanged(int count);

private:
    void rebuild();
    void refresh();
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

    static constexpr int kWidth = 92;
    static constexpr int kHeight = 64;

Q_SIGNALS:
    void clicked(Qt::KeyboardModifiers modifiers);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int m_index;
    QString m_path;
    QImage m_image;
    bool m_edited = false;
    bool m_current = false;
    bool m_selected = false;
    bool m_ticks = false;
};

} // namespace pixelfx::ui
