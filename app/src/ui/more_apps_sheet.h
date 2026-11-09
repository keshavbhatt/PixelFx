#pragma once

#include "ui/sheet.h"

namespace pixelfx::ui {

/// The developer's other apps, from the KtechpitApps module's list: search, Refresh, and Get or
/// Installed on each. Opened from the main menu and from See all in About.
class MoreAppsSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MoreAppsSheet)

public:
    /// `place` says where it was opened from ("menu", "about"), for the links' ?ref=.
    explicit MoreAppsSheet(const QString& place, QWidget* parent = nullptr);
    ~MoreAppsSheet() override;
};

} // namespace pixelfx::ui
