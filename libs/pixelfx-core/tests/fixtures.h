// SPDX-License-Identifier: Apache-2.0
// Test images generated in code (no third-party pictures in the repository). Values are in the
// working space (linear Rec.2020).
#pragma once

#include "pfx/image.h"

#include <filesystem>

namespace fixtures {

// A small "photo": sky gradient, sun, coloured patches, hard edges and fine deterministic noise.
[[nodiscard]] pfx::Image scene(int width = 256, int height = 192);
// A horizontal grey ramp from black to white (in the perceptual encoding).
[[nodiscard]] pfx::Image ramp(int width = 256, int height = 16);
[[nodiscard]] pfx::Image flat(float r, float g, float b, int width = 8, int height = 8);

// Mean and maximum absolute difference of the sRGB-encoded RGB channels.
struct Diff
{
    double mean = 0.0;
    double max = 0.0;
};
[[nodiscard]] Diff compare(const pfx::Image& a, const pfx::Image& b);

[[nodiscard]] double meanLuma(const pfx::Image& image);
[[nodiscard]] double lumaStdDev(const pfx::Image& image);

// A fresh temporary directory, removed by the destructor.
class TempDir
{
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

} // namespace fixtures
