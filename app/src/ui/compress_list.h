#pragma once

#include "services/batch_runner.h"
#include "ui/file_row.h"

#include <QWidget>

#include <vector>

class QComboBox;
class QLabel;
class QProgressBar;
class QVBoxLayout;

namespace pixelfx::ui {

class ListRow;
class StatusPill;

/// Compress only's files (batch-compress.html): each row says the format and what was done,
/// the size before and after with a bar for the new size, and the saving. A click or the arrow
/// keys select a row for the before and after.
class CompressList : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CompressList)

public:
    enum class Sort
    {
        BiggestSaving,
        Name,
        Size,
    };

    explicit CompressList(QWidget* parent = nullptr);
    ~CompressList() override;

    void setFiles(const QStringList& files);
    void setThumbnail(const QString& path, const QImage& image);
    /// Shows what `runner` knows of each file (its last run, if it ran this list).
    void refresh(const services::BatchRunner& runner);
    void setSort(Sort sort);
    [[nodiscard]] const QString& selected() const { return m_selected; }
    void select(const QString& path);
    /// The first saved file in the order shown, empty when none is.
    [[nodiscard]] QString topSaved(const services::BatchRunner& runner) const;
    /// Whether the selection was made by a click or the arrow keys (not chosen for the user).
    [[nodiscard]] bool pickedByUser() const { return m_pickedByUser; }
    /// What was done to a file, in plain words ("JPEG, lossless").
    [[nodiscard]] static QString describe(const services::CompressResult& result, const QString& path);
    /// Why a file was left as it is.
    [[nodiscard]] static QString unchangedReason(pfx::CompressNote note, const QString& path);

    /// Whether a row's × takes it off the list (not while a batch runs).
    void setRemovable(bool removable);

Q_SIGNALS:
    void selectionChanged(const QString& path);
    void removeRequested(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row
    {
        QString path;
        FileRow file;
        ListRow* row = nullptr;
        QWidget* sizes = nullptr;
        QLabel* sizeText = nullptr;
        QProgressBar* bar = nullptr;
        StatusPill* pill = nullptr;
        qint64 before = 0;
        qint64 saved = -1; ///< bytes saved, -1 until done
    };

    void build();
    void order();
    static void showResult(Row& r, const services::BatchRunner& runner, int index);

    QStringList m_files;
    std::vector<Row> m_rows;
    bool m_removable = true;
    bool m_pickedByUser = false;
    QVBoxLayout* m_column = nullptr;
    QString m_selected;
    Sort m_sort = Sort::BiggestSaving;
};

} // namespace pixelfx::ui
