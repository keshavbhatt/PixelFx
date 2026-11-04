#pragma once

#include "services/document.h"
#include "services/exporter.h"

#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QToolButton;
class QPushButton;

namespace pixelfx::core {
class Settings;
} // namespace pixelfx::core

namespace pixelfx::ui {

class AdjustPanel;
class Canvas;
class WelcomePage;

/// The main window (DESIGN section 2). M2: the header bar, the welcome page, and the canvas
/// showing one opened photo. It owns the open Document; it holds no image logic itself.
class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    explicit MainWindow(core::Settings& settings, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Opens the first supported file of `files` (the filmstrip for many arrives with M3).
    void openFiles(const QStringList& files);
    void chooseFiles();
    /// Opens the Export sheet for the open photo and writes it.
    void exportPhoto();
    [[nodiscard]] services::Exporter& exporter() { return m_exporter; }

    [[nodiscard]] services::Document& document() { return m_document; }
    [[nodiscard]] Canvas* canvas() const { return m_canvas; }
    [[nodiscard]] AdjustPanel* adjustPanel() const { return m_adjust; }
    /// Shows the photo without its settings while `on` (the eye button, or Space held).
    void showOriginal(bool on);

    /// Whether `path` has an extension PixelFx opens.
    [[nodiscard]] static bool isSupported(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void setupUi();
    void buildHeader();
    void setupActions();
    void connectDocument();
    void updateHistoryButtons();
    void showToast(const QString& text);
    void showOpened(const services::DocumentInfo& info);
    void showFailed(const QString& path, const QString& message);
    [[nodiscard]] int previewLongEdge() const;

    core::Settings& m_settings;
    services::Document m_document;
    services::Exporter m_exporter;
    QLabel* m_toast = nullptr;
    QPushButton* m_export = nullptr;
    QStackedWidget* m_stack = nullptr;
    WelcomePage* m_welcome = nullptr;
    Canvas* m_canvas = nullptr;
    AdjustPanel* m_adjust = nullptr;
    QToolButton* m_undo = nullptr;
    QToolButton* m_redo = nullptr;
    QToolButton* m_original = nullptr;
    bool m_showingOriginal = false;
    QLabel* m_docName = nullptr;
    QLabel* m_docMeta = nullptr;
    QWidget* m_header = nullptr;
};

} // namespace pixelfx::ui
