#pragma once

#include <QDateTime>
#include <QImage>
#include <QStringList>
#include <QWidget>

class QFrame;
class QHBoxLayout;
class QLabel;

namespace pixelfx::services {
class Thumbnailer;
struct Thumbnail;
} // namespace pixelfx::services

namespace pixelfx::ui {

class RecentCard;

/// First run and nothing open: the drop zone, Open images, and the Recent
/// row with each photo as it was left. Open folder and Batch edit arrive with Batch (M5).
class WelcomePage : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WelcomePage)

public:
    explicit WelcomePage(QWidget* parent = nullptr);
    ~WelcomePage() override;
    /// Highlights the drop zone while files are dragged over the window.
    void setDragActive(bool active);
    /// Where remembered edits are, so the Recent pictures show them (empty: none).
    void setSidecarFolder(const QString& folder);
    /// Shows the first files of `files` that still exist (most recent first).
    void setRecentFiles(const QStringList& files);
    [[nodiscard]] QList<RecentCard*> recentCards() const;

    /// The formats line under the button, from what the image library reads.
    [[nodiscard]] static QString formatsText(const std::vector<std::string>& extensions);

    static constexpr int kRecentShown = 4;

Q_SIGNALS:
    void openRequested();
    void openFolderRequested();
    void batchRequested();
    /// The quick jobs: Batch ready to convert or resize, or in Compress only.
    void convertRequested();
    void compressRequested();
    void recentOpened(const QString& path);
    void clearRecentRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void buildDropZone();
    QHBoxLayout* buildQuickJobs();
    void buildRecent();
    void showThumbnail(const services::Thumbnail& thumbnail) const;

    QFrame* m_dropZone = nullptr;
    QWidget* m_recent = nullptr;
    QHBoxLayout* m_cards = nullptr;
    services::Thumbnailer* m_thumbnailer = nullptr;
    QString m_sidecarFolder;
};

/// One recent photo: its picture, name and when it was edited. Clicking opens it.
class RecentCard : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RecentCard)

public:
    explicit RecentCard(const QString& path, QWidget* parent = nullptr);
    ~RecentCard() override;
    [[nodiscard]] const QString& path() const { return m_path; }
    void setThumbnail(const QImage& image, const QDateTime& edited);
    [[nodiscard]] QString dateText() const;

    static constexpr int kWidth = 170;
    static constexpr int kPictureHeight = 113; // 3:2

Q_SIGNALS:
    void clicked(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QString m_path;
    QImage m_image;
    QDateTime m_edited;
    QLabel* m_name = nullptr;
    QLabel* m_date = nullptr;
};

} // namespace pixelfx::ui
