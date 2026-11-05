#include "ui/part_checklist.h"

#include "core/looks.h"
#include "core/presets.h"
#include "ui/adjust_panel.h"
#include "ui/elided_label.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

// One or two sliders by name, more as a count; parts that are not sliders (curves, the colour
// mixer) need no note.
QString changedText(const pfx::Settings& settings, const QString& part)
{
    QStringList names;
    for (const std::string& key : core::changedKeys(settings, part)) {
        names << settingLabel(QString::fromStdString(key));
    }
    if (names.isEmpty() || names.contains(QString())) {
        return {};
    }
    return names.size() <= 2 ? names.join(u", "_s)
                             : PartChecklist::tr("%n changed", nullptr, static_cast<int>(names.size()));
}

} // namespace

PartChecklist::PartChecklist(const pfx::Settings& settings, const QStringList& parts, int columns,
                             QWidget* parent)
    : QFrame(parent)
    , m_parts(parts)
{
    setObjectName(u"card"_s);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(14, 10, 14, 10);
    row->setSpacing(20);
    std::vector<QVBoxLayout*> halves;
    for (int i = 0; i < std::max(1, columns); ++i) {
        auto* half = new QVBoxLayout;
        half->setSpacing(6);
        row->addLayout(half, 1);
        halves.push_back(half);
    }
    const auto perColumn =
        (parts.size() + static_cast<qsizetype>(halves.size()) - 1) / static_cast<qsizetype>(halves.size());
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const QString& part = parts.at(i);
        QString label = part;
        for (const core::PresetUnit& unit : core::presetUnits()) {
            if (unit.id == part) {
                label = unit.label;
            }
        }
        auto* box = new QCheckBox(label, this);
        auto* line = new QHBoxLayout;
        line->setSpacing(6);
        line->addWidget(box);
        if (QLabel* text = note(settings, part)) {
            line->addWidget(text, 1);
        } else {
            line->addStretch(1);
        }
        halves.at(static_cast<std::size_t>(i / perColumn))->addLayout(line);
        m_boxes[part] = box;
        connect(box, &QCheckBox::toggled, this, &PartChecklist::toggled);
        if (core::changedCount(settings, part) > 0 && part != u"wb"_s && part != u"crop"_s) {
            m_changed << part;
        }
    }
    for (QVBoxLayout* half : halves) {
        half->addStretch();
    }
    tick(m_changed);
}

PartChecklist::~PartChecklist() = default;

QLabel* PartChecklist::note(const pfx::Settings& settings, const QString& part)
{
    QString text;
    if (part == u"wb"_s) {
        text = tr("relative to the photo");
    } else if (part == u"look"_s) {
        if (const std::string id = settings.text("look.id", ""); !id.empty()) {
            const QString name = core::lookName(QString::fromStdString(id));
            text = u"%1 %2"_s.arg(name.isEmpty() ? tr("A look") : name)
                       .arg(std::lround(settings.number("look.strength", 100.0)));
        }
    } else {
        text = changedText(settings, part);
    }
    if (text.isEmpty()) {
        return nullptr;
    }
    auto* label = new ElidedLabel(this);
    label->setText(text);
    label->setToolTip(text);
    label->setObjectName(u"faint"_s);
    return label;
}

QStringList PartChecklist::ticked() const
{
    QStringList out;
    for (const QString& part : m_parts) {
        if (m_boxes.at(part)->isChecked()) {
            out << part;
        }
    }
    return out;
}

void PartChecklist::tick(const QStringList& parts)
{
    for (const auto& [part, box] : m_boxes) {
        const QSignalBlocker block(box);
        box->setChecked(parts.contains(part));
    }
    Q_EMIT toggled();
}

} // namespace pixelfx::ui
