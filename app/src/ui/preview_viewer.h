#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

#include <functional>

class QButtonGroup;
class QLabel;

namespace pixelfx::ui {

class Canvas;

/// The large preview (preview-viewer.html): one or more photos, each before and after, as big as
/// the window (F11 for the whole screen), with the editor's split and zoom. Batch steps through
/// its photos with Left and Right. It edits nothing.
class PreviewViewer : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PreviewViewer)

public:
    /// A photo as shown: the photo before and after, and its size in pixels.
    struct Photo
    {
        QImage before;
        QImage after;
        QSize fullSize;
    };
    /// Renders photo `index` no larger than `longEdge` pixels; called on a worker thread.
    using Render = std::function<Photo(int index, int longEdge)>;

    /// `detail` is the line under each photo's name ("Batch preview · Golden hour").
    PreviewViewer(int count, std::function<QString(int)> names, QString detail, Render render,
                  QWidget* parent);
    ~PreviewViewer() override;

    void showPhoto(int index);
    [[nodiscard]] int current() const { return m_current; }
    [[nodiscard]] Canvas* canvas() const { return m_canvas; }

    enum class View
    {
        Before,
        Split,
        After
    };
    void setView(View view);

Q_SIGNALS:
    /// A photo has been rendered and is on show.
    void photoShown(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QWidget* buildBar();
    void refreshTitle();
    void present();

    int m_count;
    int m_current = -1;
    int m_generation = 0;
    std::function<QString(int)> m_names;
    QString m_detail;
    Render m_render;
    Photo m_photo;
    View m_view = View::After;
    bool m_holdingBefore = false;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QButtonGroup* m_views = nullptr;
    QWidget* m_stage = nullptr;
    Canvas* m_canvas = nullptr;
    QWidget* m_previous = nullptr;
    QWidget* m_next = nullptr;
};

} // namespace pixelfx::ui
