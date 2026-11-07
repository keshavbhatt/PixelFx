// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/geometry.h"
#include "pfx/pipeline.h"

#include <array>
#include <cmath>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {

// A 40 x 20 image whose red channel is x and green channel is y, so positions can be read back.
pfx::Image coordinates()
{
    pfx::Image img(40, 20);
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 40; ++x) {
            float* p = img.pixel(x, y);
            p[0] = static_cast<float>(x) / 40.0F;
            p[1] = static_cast<float>(y) / 20.0F;
            p[2] = 0.5F;
            p[3] = 1.0F;
        }
    }
    return img;
}

pfx::Image run(const pfx::Image& img, std::initializer_list<std::pair<const char*, pfx::Value>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    CHECK(pfx::outputSize(s, {img.width(), img.height()}) ==
          pfx::Size{out.value().width(), out.value().height()});
    return std::move(out).value();
}

} // namespace

TEST_CASE("a quarter turn is clockwise and swaps the sides")
{
    const pfx::Image out = run(coordinates(), {{"geometry.rotate90", 1.0}});
    CHECK(out.width() == 20);
    CHECK(out.height() == 40);
    // The original's top-left corner ends at the top-right.
    CHECK(out.pixel(19, 0)[0] == doctest::Approx(0.0F));
    CHECK(out.pixel(19, 0)[1] == doctest::Approx(0.0F));
}

TEST_CASE("flips mirror the image")
{
    const pfx::Image h = run(coordinates(), {{"geometry.flipH", true}});
    CHECK(h.pixel(0, 0)[0] == doctest::Approx(39.0F / 40.0F));
    const pfx::Image v = run(coordinates(), {{"geometry.flipV", true}});
    CHECK(v.pixel(0, 0)[1] == doctest::Approx(19.0F / 20.0F));
}

TEST_CASE("the crop is a share of the image and keeps the right part")
{
    const pfx::Image out = run(coordinates(), {{"geometry.crop.x", 0.5},
                                               {"geometry.crop.y", 0.25},
                                               {"geometry.crop.w", 0.25},
                                               {"geometry.crop.h", 0.5}});
    CHECK(out.width() == 10);
    CHECK(out.height() == 10);
    CHECK(out.pixel(0, 0)[0] == doctest::Approx(20.0F / 40.0F));
    CHECK(out.pixel(0, 0)[1] == doctest::Approx(5.0F / 20.0F));
}

TEST_CASE("straightening trims to the largest rectangle without empty corners")
{
    const pfx::Image scene = fixtures::scene(300, 200);
    const pfx::Image out = run(scene, {{"geometry.angle", 7.0}});
    const double s = pfx::straightenScale(7.0, {300, 200});
    CHECK(s < 1.0);
    CHECK(out.width() == static_cast<int>(std::floor(300 * s)) - 2);
    for (const auto& [x, y] : {std::pair{0, 0},
                               {out.width() - 1, 0},
                               {0, out.height() - 1},
                               {out.width() - 1, out.height() - 1}}) {
        CHECK(out.pixel(x, y)[3] > 0.99F); // nothing transparent in the corners
    }
    CHECK(pfx::straightenScale(0.0, {300, 200}) == 1.0);
    CHECK(pfx::straightenScale(-7.0, {300, 200}) == doctest::Approx(s));
}

TEST_CASE("everything together, and the size is known before rendering")
{
    const pfx::Image scene = fixtures::scene(320, 240);
    for (const double turns : {0.0, 1.0, 2.0, 3.0}) {
        for (const double angle : {0.0, -12.5, 30.0}) {
            run(scene, {{"geometry.rotate90", turns},
                        {"geometry.flipH", true},
                        {"geometry.angle", angle},
                        {"geometry.crop.x", 0.1},
                        {"geometry.crop.w", 0.7},
                        {"geometry.crop.h", 0.9}});
        }
    }
}

TEST_CASE("perspective: the leaning edge widens, nothing empty shows, the size stays")
{
    const pfx::Image img = coordinates();
    const pfx::Image fixed = run(img, {{"geometry.perspective.vertical", 100.0}});
    CHECK(fixed.width() == img.width());
    CHECK(fixed.height() == img.height());
    // Top row: the left edge now shows a point further in (red, the x coordinate, is larger).
    CHECK(fixed.pixel(0, 0)[0] > img.pixel(0, 0)[0] + 0.1F);
    // Bottom row: untouched at full strength (only the top narrows).
    CHECK(fixed.pixel(0, img.height() - 1)[0] ==
          doctest::Approx(img.pixel(0, img.height() - 1)[0]).epsilon(0.01));
    // Every pixel comes from inside the photo: alpha stays 1.
    for (int y = 0; y < fixed.height(); ++y) {
        for (int x = 0; x < fixed.width(); ++x) {
            CHECK(fixed.pixel(x, y)[3] == doctest::Approx(1.0));
        }
    }
    const pfx::Image sideways = run(img, {{"geometry.perspective.horizontal", -100.0}});
    CHECK(sideways.pixel(img.width() - 1, 0)[1] >
          img.pixel(img.width() - 1, 0)[1] + 0.1F); // right edge narrows
}

namespace {

// A dark 240 x 160 picture with a white four-sided shape at `q` (pixels, top left first,
// clockwise) and a red mark just inside its top left corner.
pfx::Image shapeAt(const std::array<pfx::Corner, 4>& q)
{
    pfx::Image img(240, 160);
    const auto inside = [&q](double x, double y) {
        for (std::size_t i = 0; i < 4; ++i) {
            const pfx::Corner& a = q.at(i);
            const pfx::Corner& b = q.at((i + 1) % 4);
            if (((b.x - a.x) * (y - a.y)) - ((b.y - a.y) * (x - a.x)) < 0.0) {
                return false;
            }
        }
        return true;
    };
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            float* p = img.pixel(x, y);
            const bool white = inside(x + 0.5, y + 0.5);
            const bool mark = white && std::hypot(x - (q[0].x + 12), y - (q[0].y + 12)) < 6;
            p[0] = white ? 1.0F : 0.05F;
            p[1] = white && !mark ? 1.0F : 0.05F;
            p[2] = white && !mark ? 1.0F : 0.05F;
            p[3] = 1.0F;
        }
    }
    return img;
}

void setCorners(pfx::Settings& s, const std::array<pfx::Corner, 4>& q, pfx::Size size)
{
    const std::array<std::string, 4> names = {"topLeft", "topRight", "bottomRight", "bottomLeft"};
    for (std::size_t i = 0; i < 4; ++i) {
        s.set("geometry.perspective.corners." + names.at(i) + ".x", q.at(i).x / size.width);
        s.set("geometry.perspective.corners." + names.at(i) + ".y", q.at(i).y / size.height);
    }
}

pfx::Image renderWith(const pfx::Image& img, const pfx::Settings& s)
{
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    CHECK(pfx::outputSize(s, {img.width(), img.height()}) ==
          pfx::Size{out.value().width(), out.value().height()});
    return std::move(out).value();
}

bool whiteAt(const pfx::Image& img, int x, int y)
{
    return img.pixel(x, y)[1] > 0.5F;
}

} // namespace

TEST_CASE("four corners, inside: a shape seen at an angle comes back as an upright rectangle")
{
    const std::array<pfx::Corner, 4> q{{{70, 30}, {190, 40}, {215, 140}, {40, 125}}};
    const pfx::Image img = shapeAt(q);
    pfx::Settings s;
    setCorners(s, q, {240, 160});
    s.set("geometry.perspective.keep", std::string("inside"));
    const pfx::Image out = renderWith(img, s);
    // As big as the shape's average sides.
    CHECK(out.width() == doctest::Approx((std::hypot(120, 10) + std::hypot(175, 15)) / 2).epsilon(0.02));
    CHECK(out.height() == doctest::Approx((std::hypot(30, 95) + std::hypot(25, 100)) / 2).epsilon(0.02));
    // White to its edges, two pixels in (the edges blend with the dark around the shape).
    for (int y = 2; y < out.height() - 2; ++y) {
        for (int x = 2; x < out.width() - 2; ++x) {
            if (std::hypot(x - 14.0, y - 13.0) > 18.0) { // the red mark, a little larger
                CAPTURE(x);
                CAPTURE(y);
                REQUIRE(whiteAt(out, x, y));
            }
        }
    }
    // The red mark stays at the top left: nothing turned or mirrored.
    CHECK(out.pixel(14, 13)[0] > 0.5F);
    CHECK(out.pixel(14, 13)[1] < 0.5F);
}

TEST_CASE("four corners, whole photo: the shape is upright inside the whole picture, nothing empty")
{
    const std::array<pfx::Corner, 4> q{{{70, 30}, {190, 40}, {215, 140}, {40, 125}}};
    const pfx::Image img = shapeAt(q);
    pfx::Settings s;
    setCorners(s, q, {240, 160});
    const pfx::Image out = renderWith(img, s);
    CHECK(static_cast<double>(out.width()) / out.height() == doctest::Approx(1.5).epsilon(0.02));
    // The white shape's left and right edges are the same on every row it spans.
    std::vector<int> lefts;
    std::vector<int> rights;
    for (int y = 0; y < out.height(); ++y) {
        int left = -1;
        int right = -1;
        for (int x = 0; x < out.width(); ++x) {
            if (whiteAt(out, x, y)) {
                left = left < 0 ? x : left;
                right = x;
            }
        }
        if (left >= 0 && right - left > out.width() / 4) {
            lefts.push_back(left);
            rights.push_back(right);
        }
    }
    REQUIRE(lefts.size() > 20);
    for (std::size_t i = 3; i + 3 < lefts.size(); ++i) { // away from the blended top and bottom rows
        CHECK(std::abs(lefts.at(i) - lefts.at(lefts.size() / 2)) <= 1);
        CHECK(std::abs(rights.at(i) - rights.at(rights.size() / 2)) <= 1);
    }
    // Every pixel comes from inside the photo: the outline is trimmed, alpha stays 1.
    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            REQUIRE(out.pixel(x, y)[3] == doctest::Approx(1.0));
        }
    }
}

TEST_CASE("four corners: only a convex, clockwise shape is used; the sliders give way to it")
{
    using C = std::array<pfx::Corner, 4>;
    CHECK(pfx::cornersUsable(C{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}}));
    CHECK(pfx::cornersUsable(C{{{0.2, 0.1}, {0.9, 0.2}, {0.8, 0.9}, {0.1, 0.8}}}));
    CHECK_FALSE(pfx::cornersUsable(C{{{0, 0}, {1, 1}, {1, 0}, {0, 1}}}));     // crossed
    CHECK_FALSE(pfx::cornersUsable(C{{{0, 0}, {0, 1}, {1, 1}, {1, 0}}}));     // mirrored
    CHECK_FALSE(pfx::cornersUsable(C{{{0, 0}, {0.005, 0}, {1, 1}, {0, 1}}})); // a side too short
    CHECK_FALSE(pfx::cornersUsable(C{{{0, 0}, {1, 0}, {1, 1}, {0.7, 0.3}}})); // a dent
    // A folded shape in the settings is not used: the photo comes back as it was.
    const pfx::Image img = coordinates();
    pfx::Settings folded;
    setCorners(folded, C{{{0, 0}, {40, 20}, {40, 0}, {0, 20}}}, {40, 20});
    const pfx::Image same = renderWith(img, folded);
    CHECK(same.width() == img.width());
    CHECK(same.pixel(5, 5)[0] == doctest::Approx(img.pixel(5, 5)[0]));
    // With corners set the keystone sliders do nothing.
    const C q{{{4, 2}, {36, 1}, {38, 19}, {2, 18}}};
    pfx::Settings corners;
    setCorners(corners, q, {40, 20});
    pfx::Settings both = corners;
    both.set("geometry.perspective.vertical", 80.0);
    const pfx::Image a = renderWith(img, corners);
    const pfx::Image b = renderWith(img, both);
    REQUIRE(a.width() == b.width());
    CHECK(a.pixel(10, 10)[0] == doctest::Approx(b.pixel(10, 10)[0]));
}
