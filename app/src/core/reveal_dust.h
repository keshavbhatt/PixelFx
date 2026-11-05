#pragma once

#include <QImage>

namespace pixelfx::core {

/// Heal's Reveal dust view (mock heal.html, D): the photo's fine detail on a middle grey, so
/// sensor dust and specks on a sky stand out. Brightness is taken out (each pixel minus the
/// average around it, about 1% of the long edge) and what is left is turned up. Display only.
[[nodiscard]] QImage revealDust(const QImage& image);

} // namespace pixelfx::core
