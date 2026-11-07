#pragma once

#include "core/crash_handler.h"
#include "ui/sheet.h"

namespace pixelfx::ui {

/// The start after a crash, once: PixelFx closed unexpectedly, what was kept, and Report the
/// problem (accept) or Not now (reject, and Escape).
class CrashNotice : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CrashNotice)

public:
    /// `editsKept`: edits are remembered (Remember edits on), so only the last moments can be lost.
    CrashNotice(const core::crash::Report& crash, bool editsKept, QWidget* parent = nullptr);
    ~CrashNotice() override;
};

} // namespace pixelfx::ui
