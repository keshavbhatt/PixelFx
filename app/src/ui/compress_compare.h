#pragma once

#include <QImage>
#include <QWidget>

class QLabel;

namespace pixelfx::ui {

class StatusPill;

/// Compress only's before and after (batch-compress.html): the same spot of the original and the
/// result side by side at 100%, so a lossy setting is judged by eye.
class CompressCompare : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CompressCompare)

public:
    explicit CompressCompare(QWidget* parent = nullptr);
    ~CompressCompare() override;

    /// The two pictures and what each says ("DSC_0413.jpg · 9.1 MB", "5.6 MB, saved 38%").
    void setPair(const QImage& before, const QString& beforeText, const QImage& after,
                 const QString& afterText);
    /// While the spot of a newly selected file is read.
    void setWaiting(const QString& beforeText);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void place();

    QLabel* m_beforeText;
    QLabel* m_afterText;
    QLabel* m_beforePicture;
    QLabel* m_afterPicture;
    StatusPill* m_zoom;
    QImage m_before;
    QImage m_after;
};

} // namespace pixelfx::ui
