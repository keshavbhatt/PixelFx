#include "ui/save_folder_list.h"

#include "ui/style.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QStyleOptionComboBox>
#include <QStylePainter>

#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

enum Kind
{
    NextToPhoto,
    Folder,
    Choose,
};

constexpr int kKindRole = Qt::UserRole;
constexpr int kPathRole = Qt::UserRole + 1;

} // namespace

SaveFolderList::SaveFolderList(QWidget* parent)
    : QComboBox(parent)
{
    setAccessibleName(tr("Save to"));
    fitToPanel(this); // a long folder name is cut with "...", not the window widened
    connect(this, &QComboBox::activated, this, &SaveFolderList::choose);
}

SaveFolderList::~SaveFolderList() = default;

void SaveFolderList::setChoices(const QString& defaultFolder, const QString& folder, const QString& recent)
{
    const auto clean = [](const QString& path) { return path.isEmpty() ? QString() : QDir::cleanPath(path); };
    m_default = clean(defaultFolder);
    m_folder = clean(folder);
    m_recent = clean(recent);
    if (!m_folder.isEmpty() && m_folder != m_default) {
        m_recent = m_folder; // the folder in use is the one to keep in the list
    }
    if (m_recent == m_default) {
        m_recent.clear();
    }
    rebuild();
}

QString SaveFolderList::shortName(const QString& folder)
{
    const QFileInfo info(folder);
    const QString parent = info.dir().dirName();
    const QString name = info.fileName();
    if (name.isEmpty()) {
        return QDir::toNativeSeparators(folder);
    }
    return parent.isEmpty() ? name : u"%1 › %2"_s.arg(parent, name);
}

void SaveFolderList::rebuild()
{
    const QSignalBlocker block(this);
    clear();
    const auto add = [this](const QString& text, Kind kind, const QString& path = {}) {
        addItem(text);
        const int i = count() - 1;
        setItemData(i, kind, kKindRole);
        setItemData(i, path, kPathRole);
        if (!path.isEmpty()) {
            setItemData(i, QDir::toNativeSeparators(path), Qt::ToolTipRole);
        }
    };
    add(tr("Next to the photo"), NextToPhoto);
    add(shortName(m_default), Folder, m_default);
    if (!m_recent.isEmpty()) {
        add(shortName(m_recent), Folder, m_recent);
    }
    insertSeparator(count());
    add(tr("Choose a folder..."), Choose);
    int current = 0;
    for (int i = 0; i < count(); ++i) {
        if (itemData(i, kKindRole).toInt() == Folder && itemData(i, kPathRole).toString() == m_folder) {
            current = i;
        }
    }
    setCurrentIndex(m_folder.isEmpty() ? 0 : current);
    setToolTip(m_folder.isEmpty() ? tr("Beside the photo it was made from")
                                  : QDir::toNativeSeparators(m_folder));
}

void SaveFolderList::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter painter(this);
    QStyleOptionComboBox option;
    initStyleOption(&option);
    const QRect text =
        style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
    option.currentText = fontMetrics().elidedText(option.currentText, Qt::ElideMiddle, text.width());
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

void SaveFolderList::choose(int index)
{
    QString folder;
    switch (static_cast<Kind>(itemData(index, kKindRole).toInt())) {
    case NextToPhoto:
        break;
    case Folder:
        folder = itemData(index, kPathRole).toString();
        break;
    case Choose: {
        const QString start = m_folder.isEmpty() ? m_default : m_folder;
        folder = QFileDialog::getExistingDirectory(this, tr("Save to"), start);
        if (folder.isEmpty()) {
            rebuild(); // cancelled: the choice before stays
            return;
        }
        folder = QDir::cleanPath(folder);
        if (folder != m_default) {
            m_recent = folder;
        }
        break;
    }
    }
    m_folder = std::move(folder);
    rebuild();
    Q_EMIT folderChosen(m_folder);
}

} // namespace pixelfx::ui
