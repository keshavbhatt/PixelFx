#pragma once

#include <QString>

class QImage;
class QLabel;
class QWidget;

namespace pixelfx::ui {

class ListRow;
class StatusPill;

/// A file in Batch's lists (batch.html, batch-compress.html): its picture, its name and a line
/// under it, then anything the list adds, a status, and a × that leaves it out. Hover and
/// selection come from ListRow, so both lists look and behave alike.
struct FileRow
{
    ListRow* row = nullptr;
    QLabel* thumb = nullptr;
    StatusPill* pill = nullptr;
};

/// `extra` (sizes, for one) goes between the text and the status.
[[nodiscard]] FileRow makeFileRow(QWidget* parent, const QString& path, const QString& detail,
                                  QWidget* extra = nullptr);
/// Shows `image` in the row's picture, filling it, with rounded corners.
void setFileThumb(const FileRow& row, const QImage& image);

} // namespace pixelfx::ui
