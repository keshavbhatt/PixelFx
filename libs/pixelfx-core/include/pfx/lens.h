// SPDX-License-Identifier: Apache-2.0
// Lens profiles (ADR-013): a lens's distortion, lateral chromatic aberration and vignetting,
// measured and kept in the lensfun database, undone for the lens a photo was taken with. The
// lens is found from the photo's Exif; a preset only turns the correction on.
#pragma once

#include "pfx/io.h"

#include <filesystem>
#include <string>

namespace pfx {

// What a lens profile is found by: the camera and the lens a photo was taken with, and the
// focal length and aperture its measurements depend on.
struct LensFacts
{
    std::string cameraMake;
    std::string cameraModel;
    std::string lens;         // the lens model as Exif names it; empty for most fixed lenses
    double focalLength = 0.0; // millimetres
    double aperture = 0.0;    // f-number

    bool operator==(const LensFacts&) const = default;
};
[[nodiscard]] LensFacts lensFacts(const ExifSummary& exif);

// Whether lens profiles can be used: this build has lensfun and its database could be read.
[[nodiscard]] bool lensProfilesAvailable();
// Reads the lens database from `folder` (the one a package ships) on the next lookup instead
// of from lensfun's own places; an empty path goes back to them.
void setLensDatabaseFolder(const std::filesystem::path& folder);
// The profiled lens `facts` lead to ("Sony FE 24-70mm f/2.8 GM"), empty when there is none.
[[nodiscard]] std::string lensProfileName(const LensFacts& facts);

} // namespace pfx
