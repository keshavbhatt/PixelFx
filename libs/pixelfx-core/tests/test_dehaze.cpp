// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/pipeline.h"

#include <algorithm>
#include <doctest/doctest.h>

namespace {

// The scene under a pale blue-grey veil, as distance haze looks.
pfx::Image hazy(const pfx::Image& clear)
{
    pfx::Image out = clear;
    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            float* p = out.pixel(x, y);
            const float veil[3] = {0.55F, 0.58F, 0.65F};
            for (int c = 0; c < 3; ++c) {
                p[c] = (0.55F * p[c]) + (0.45F * veil[c]);
            }
        }
    }
    return out;
}

double meanDark(const pfx::Image& img)
{
    double sum = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const float* p = img.pixel(x, y);
            sum += double{std::min({p[0], p[1], p[2]})};
        }
    }
    return sum / static_cast<double>(img.pixelCount());
}

pfx::Image dehaze(const pfx::Image& img, double amount)
{
    pfx::Settings s;
    s.set("detail.dehaze", amount);
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    return std::move(out).value();
}

} // namespace

TEST_CASE("dehaze: lifts the veil, adding haze lays one")
{
    const pfx::Image clear = fixtures::scene(384, 256);
    const pfx::Image veiled = hazy(clear);
    const pfx::Image cleared = dehaze(veiled, 80);
    CHECK(meanDark(cleared) < meanDark(veiled) * 0.85);                         // the haze floor comes down
    CHECK(fixtures::lumaStdDev(cleared) > fixtures::lumaStdDev(veiled) * 1.15); // contrast returns
    const pfx::Image fogged = dehaze(clear, -80);
    CHECK(meanDark(fogged) > meanDark(clear) * 1.1);
    CHECK(fixtures::lumaStdDev(fogged) < fixtures::lumaStdDev(clear));
}

TEST_CASE("dehaze: a large photo and its preview get the same map")
{
    const pfx::Image full = hazy(fixtures::scene(2400, 1600));
    const pfx::Image preview = pfx::downscale(full, 2);
    const pfx::Image fromFull = pfx::downscale(dehaze(full, 70), 2);
    const pfx::Image fromPreview = dehaze(preview, 70);
    const fixtures::Diff diff = fixtures::compare(fromFull, fromPreview);
    CAPTURE(diff.mean);
    CHECK(diff.mean < 0.01);
}
