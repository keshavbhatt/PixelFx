#pragma once

#include <QString>

namespace pixelfx::core {

/// Where PixelFx points people on the web.
[[nodiscard]] inline QString sourceUrl()
{
    return QStringLiteral("https://github.com/keshavbhatt/PixelFx");
}
[[nodiscard]] inline QString issuesUrl()
{
    return QStringLiteral("https://github.com/keshavbhatt/PixelFx/issues");
}
/// The online guide is the repository's wiki (owner, 2026-11-06).
[[nodiscard]] inline QString guideUrl()
{
    return QStringLiteral("https://github.com/keshavbhatt/PixelFx/wiki");
}

} // namespace pixelfx::core
