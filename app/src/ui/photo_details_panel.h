#pragma once

#include "services/document.h"

#include <QFrame>
#include <QPointer>

#include <functional>

class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace pixelfx::ui {

class ElidedLabel;
class PathLabel;

/// Photo details: the open photo's file (size, format, file size, colour profile) and what its
/// camera wrote (camera, lens, exposure, focal length, date taken), in a panel over the photo
/// under the header's file name. Where the photo was taken shows only after Show, and is hidden
/// again for every photo the panel shows next.
class PhotoDetailsPanel : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PhotoDetailsPanel)

public:
    /// What Export does with a photo's location, for the line under it.
    enum class LocationOnExport
    {
        Kept,
        LeftOut,             ///< Keep location is off
        LeftOutWithMetadata, ///< camera details are not kept at all
    };

    explicit PhotoDetailsPanel(QWidget* parent);
    ~PhotoDetailsPanel() override;

    /// Shows `info`. The location is hidden again unless it is the same photo.
    void setDetails(const services::DocumentInfo& info, LocationOnExport onExport);
    /// Keeps the panel's left edge on `anchor`'s, 6 px under `below`'s bottom; both are in the
    /// panel's parent.
    void setAnchor(QWidget* anchor, QWidget* below);
    /// Opens the panel with the focus on its first control, or closes it, giving the focus back.
    void setOpen(bool open);

    /// Shows or hides the coordinates of a photo that has them.
    void showLocation(bool shown);
    [[nodiscard]] bool locationShown() const { return m_locationShown; }
    /// Every line as text: what Copy details puts on the clipboard.
    [[nodiscard]] QString detailsText() const;

Q_SIGNALS:
    /// The panel was opened or closed (the header's name shows it open).
    void openChanged(bool open);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// The file name, its folder with Copy and Show in folder, and the close button.
    QWidget* buildHead();
    void rebuild();
    void place();
    /// The folder as wide as it is drawn in the room Copy and Show in folder leave.
    void fitPath();
    void addSection(const QString& title);
    void addRow(const QString& label, const QString& value);
    void addNote(const QString& text);
    void addLocation();
    /// The line under the location: what Export does with it.
    [[nodiscard]] QString locationOnExportText() const;
    /// A flat icon button that copies `text()` and shows a check for a moment.
    QToolButton* copyButton(const QString& tip, std::function<QString()> text);

    services::DocumentInfo m_info;
    LocationOnExport m_onExport = LocationOnExport::LeftOut;
    bool m_locationShown = false;
    QPointer<QWidget> m_anchor;
    QPointer<QWidget> m_below;
    QPointer<QWidget> m_focusBefore;
    ElidedLabel* m_name = nullptr;
    PathLabel* m_folder = nullptr;
    QToolButton* m_copyPath = nullptr;
    QWidget* m_pathRow = nullptr; ///< the folder, Copy and Show in folder
    QPushButton* m_showFolder = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_body = nullptr;
    QVBoxLayout* m_column = nullptr;
    QGridLayout* m_rows = nullptr; ///< the rows of the section being added
    QStringList m_lines;           ///< detailsText, built with the rows
};

} // namespace pixelfx::ui
