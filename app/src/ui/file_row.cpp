#include "ui/file_row.h"

#include "ui/list_row.h"
#include "ui/status_pill.h"
#include "ui/style.h"

#include <QFileInfo>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr QSize kThumb(72, 48);

} // namespace

FileRow makeFileRow(QWidget* parent, const QString& path, const QString& detail, QWidget* extra)
{
    FileRow f;
    const QString name = QFileInfo(path).fileName();
    f.row = new ListRow(parent);
    f.row->setTitle(name);
    f.row->setSubtitle(detail);
    f.thumb = new QLabel;
    f.thumb->setObjectName(u"batchThumb"_s);
    f.thumb->setProperty("path", path);
    f.thumb->setFixedSize(kThumb);
    f.row->addLeading(f.thumb);
    if (extra != nullptr) {
        f.row->addTrailing(extra);
    }
    f.pill = new StatusPill;
    f.row->addTrailing(f.pill);
    f.row->setAction(u"close"_s, QObject::tr("Leave %1 out").arg(name));
    return f;
}

void setFileThumb(const FileRow& row, const QImage& image)
{
    row.thumb->setPixmap(roundedPixmap(image, row.thumb->size(), row.thumb->devicePixelRatioF(), 4.0));
}

} // namespace pixelfx::ui
