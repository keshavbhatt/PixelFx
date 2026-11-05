// SPDX-License-Identifier: Apache-2.0
// Lens profiles through lensfun (ADR-013). Vignetting is undone first, on the pixels as the lens
// drew them, then distortion and lateral chromatic aberration in one resampling, scaled so the
// corrected picture fills the frame. Both have an amount (100 the profile's own, 0 none, 200
// twice). lensfun works on coordinates relative to the image and its sensor, so a preview and
// the full-size export are corrected alike.
#include "core/log_internal.h"
#include "core/parallel.h"
#include "ops/ops.h"
#include "pfx/lens.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>

#ifdef PFX_HAS_LENSFUN
#include <lensfun.h>
#endif

namespace pfx {

LensFacts lensFacts(const ExifSummary& exif)
{
    return {exif.make, exif.model, exif.lens, exif.focalLength, exif.fNumber};
}

} // namespace pfx

#ifdef PFX_HAS_LENSFUN

namespace pfx {
namespace {

// The lens database, read once on first use (and again after the folder changes).
struct Database
{
    std::mutex mutex;
    std::filesystem::path folder;
    std::unique_ptr<lfDatabase> db;
    bool tried = false;
};

Database& database()
{
    static Database d;
    return d;
}

const lfDatabase* loadedDatabase()
{
    Database& d = database();
    const std::scoped_lock lock(d.mutex);
    if (!d.tried) {
        d.tried = true;
        auto db = std::make_unique<lfDatabase>();
        const bool read =
            d.folder.empty() ? db->Load() == LF_NO_ERROR : db->LoadDirectory(d.folder.string().c_str());
        if (read) {
            d.db = std::move(db);
        } else {
            detail::log(LogLevel::Warning, "The lens database could not be read; lens profiles are off");
        }
    }
    return d.db.get();
}

struct Match
{
    const lfCamera* camera = nullptr;
    const lfLens* lens = nullptr;
};

// The camera and the best-matching lens for `facts`. A photo without a lens name matches only
// when the camera has a single lens (a fixed-lens camera).
std::optional<Match> findLens(const lfDatabase& db, const LensFacts& facts)
{
    if (facts.cameraModel.empty()) {
        return std::nullopt;
    }
    Match m;
    const lfCamera** cameras = db.FindCamerasExt(facts.cameraMake.c_str(), facts.cameraModel.c_str());
    if (cameras != nullptr) {
        m.camera = cameras[0];
        lf_free(static_cast<void*>(cameras));
    }
    if (m.camera == nullptr) {
        return std::nullopt;
    }
    const lfLens** lenses =
        db.FindLenses(m.camera, nullptr, facts.lens.empty() ? nullptr : facts.lens.c_str());
    if (lenses != nullptr) {
        if (!facts.lens.empty() || (lenses[0] != nullptr && lenses[1] == nullptr)) {
            m.lens = lenses[0];
        }
        lf_free(static_cast<void*>(lenses));
    }
    if (m.lens == nullptr) {
        return std::nullopt;
    }
    return m;
}

// Channel `c` of `image` at (x, y), bilinear, edges clamped.
float sampleChannel(const Image& image, int c, float x, float y)
{
    const float cx = std::clamp(x, 0.0F, static_cast<float>(image.width() - 1));
    const float cy = std::clamp(y, 0.0F, static_cast<float>(image.height() - 1));
    const int x0 = static_cast<int>(cx);
    const int y0 = static_cast<int>(cy);
    const int x1 = std::min(x0 + 1, image.width() - 1);
    const int y1 = std::min(y0 + 1, image.height() - 1);
    const float fx = cx - static_cast<float>(x0);
    const float fy = cy - static_cast<float>(y0);
    const float top = (image.pixel(x0, y0)[c] * (1.0F - fx)) + (image.pixel(x1, y0)[c] * fx);
    const float bottom = (image.pixel(x0, y1)[c] * (1.0F - fx)) + (image.pixel(x1, y1)[c] * fx);
    return (top * (1.0F - fy)) + (bottom * fy);
}

// Brightens each pixel by the profile's vignetting gain raised to `amount`.
void undoVignetting(Image& image, const lfModifier& modifier, double amount, int threads)
{
    detail::parallelFor(image.height(), threads, [&](int y0, int y1) {
        std::vector<float> gain(static_cast<std::size_t>(image.width()));
        for (int y = y0; y < y1; ++y) {
            std::ranges::fill(gain, 1.0F);
            modifier.ApplyColorModification(gain.data(), 0.0F, static_cast<float>(y), image.width(), 1,
                                            LF_CR_1(INTENSITY),
                                            static_cast<int>(gain.size() * sizeof(float)));
            float* p = image.row(y);
            for (const float g : gain) {
                const auto factor = static_cast<float>(std::pow(double{g}, amount));
                for (int c = 0; c < 3; ++c) {
                    p[c] *= factor;
                }
                p += 4;
            }
        }
    });
}

// Moves each channel of each pixel `amount` of the way to where the profile says it belongs.
void undoDistortion(Image& image, const lfModifier& modifier, double amount, int threads)
{
    const Image source = image;
    const auto a = static_cast<float>(amount);
    detail::parallelFor(image.height(), threads, [&](int y0, int y1) {
        std::vector<float> where(static_cast<std::size_t>(image.width()) * 6);
        for (int y = y0; y < y1; ++y) {
            if (!modifier.ApplySubpixelGeometryDistortion(0.0F, static_cast<float>(y), image.width(), 1,
                                                          where.data())) {
                continue;
            }
            float* p = image.row(y);
            const auto fy = static_cast<float>(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const float* w = where.data() + (static_cast<std::size_t>(x) * 6);
                const auto fx = static_cast<float>(x);
                for (int c = 0; c < 3; ++c) {
                    const std::size_t at = static_cast<std::size_t>(c) * 2;
                    p[c] = sampleChannel(source, c, fx + (a * (w[at] - fx)), fy + (a * (w[at + 1] - fy)));
                }
                p[3] = sampleChannel(source, 3, fx + (a * (w[2] - fx)), fy + (a * (w[3] - fy)));
            }
        }
    });
}

} // namespace

bool lensProfilesAvailable()
{
    return loadedDatabase() != nullptr;
}

void setLensDatabaseFolder(const std::filesystem::path& folder)
{
    Database& d = database();
    const std::scoped_lock lock(d.mutex);
    d.folder = folder;
    d.db.reset();
    d.tried = false;
}

std::string lensProfileName(const LensFacts& facts)
{
    const lfDatabase* db = loadedDatabase();
    const auto match = db != nullptr ? findLens(*db, facts) : std::nullopt;
    if (!match) {
        return {};
    }
    const char* model = lf_mlstr_get(match->lens->Model);
    return model != nullptr ? model : "";
}

namespace detail {

bool lensProfileActive(const Settings& s)
{
    return s.boolean("detail.lensProfile.enable", false) &&
           (s.number("detail.lensProfile.distortion", 100.0) > 0.0 ||
            s.number("detail.lensProfile.vignetting", 100.0) > 0.0);
}

void lensProfileApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const lfDatabase* db = ctx.lens != nullptr ? loadedDatabase() : nullptr;
    const auto match = db != nullptr ? findLens(*db, *ctx.lens) : std::nullopt;
    if (!match) {
        return;
    }
    const double distortion = s.number("detail.lensProfile.distortion", 100.0) / 100.0;
    const double vignetting = s.number("detail.lensProfile.vignetting", 100.0) / 100.0;
    const lfLens* lens = match->lens;
    // Without them in Exif: the shortest focal length and the widest aperture, where the
    // corrections are strongest; and focused far away.
    const auto focal =
        static_cast<float>(ctx.lens->focalLength > 0.0 ? ctx.lens->focalLength : double{lens->MinFocal});
    const auto aperture =
        static_cast<float>(ctx.lens->aperture > 0.0 ? ctx.lens->aperture : double{lens->MinAperture});
    constexpr float kFar = 1000.0F;
    constexpr auto kVignetting = static_cast<unsigned>(LF_MODIFY_VIGNETTING);
    constexpr unsigned kGeometry = static_cast<unsigned>(LF_MODIFY_DISTORTION) |
                                   static_cast<unsigned>(LF_MODIFY_TCA) |
                                   static_cast<unsigned>(LF_MODIFY_SCALE);
    unsigned flags = 0U;
    if (vignetting > 0.0) {
        flags |= kVignetting;
    }
    if (distortion > 0.0) {
        flags |= kGeometry;
    }
    lfModifier modifier(lens, match->camera->CropFactor, image.width(), image.height());
    // A scale of 0: lensfun's own, the smallest that leaves no empty corners.
    const auto done = static_cast<unsigned>(modifier.Initialize(
        lens, LF_PF_F32, focal, aperture, kFar, 0.0F, LF_RECTILINEAR, static_cast<int>(flags), false));
    if ((done & kVignetting) != 0U) {
        undoVignetting(image, modifier, vignetting, ctx.threads);
    }
    if ((done & kGeometry) != 0U) {
        undoDistortion(image, modifier, distortion, ctx.threads);
    }
}

} // namespace detail
} // namespace pfx

#else // without lensfun: no profiles, the step does nothing

namespace pfx {

bool lensProfilesAvailable()
{
    return false;
}

void setLensDatabaseFolder(const std::filesystem::path& /*folder*/) {}

std::string lensProfileName(const LensFacts& /*facts*/)
{
    return {};
}

namespace detail {

bool lensProfileActive(const Settings& /*s*/)
{
    return false;
}

void lensProfileApply(Image& /*image*/, const Settings& /*s*/, const OpContext& /*ctx*/) {}

} // namespace detail
} // namespace pfx

#endif
