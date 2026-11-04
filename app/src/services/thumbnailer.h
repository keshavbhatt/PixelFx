#pragma once

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>

#include <memory>
#include <pfx/image.h>
#include <pfx/settings.h>

class QThreadPool;

namespace pixelfx::services {

/// A small picture of a photo with its remembered edits (the welcome page's Recent row).
struct Thumbnail
{
    QString path;
    QImage image;     ///< empty when the photo could not be read
    QDateTime edited; ///< when its edits were last saved; invalid when it has none
};

/// Makes thumbnails on worker threads (ADR-006), two at a time, and reports each when ready.
class Thumbnailer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Thumbnailer)

public:
    explicit Thumbnailer(QObject* parent = nullptr);
    ~Thumbnailer() override;

    /// Edits are looked up in `sidecarFolder` (empty: the photo as it is).
    void request(const QString& path, int longEdge, const QString& sidecarFolder);
    /// Renders `settings` on `source` (already small) on a worker; `key` comes back with it.
    void requestRender(int key, std::shared_ptr<const pfx::Image> source, const pfx::Settings& settings);
    /// Runs on a worker: decode small, apply the stored settings, convert for display.
    [[nodiscard]] static Thumbnail make(const QString& path, int longEdge, const QString& sidecarFolder);

Q_SIGNALS:
    void ready(const pixelfx::services::Thumbnail& thumbnail);
    void rendered(int key, const QImage& image);

private:
    QThreadPool* m_pool = nullptr;
};

} // namespace pixelfx::services
