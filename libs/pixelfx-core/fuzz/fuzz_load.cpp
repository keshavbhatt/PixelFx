// SPDX-License-Identifier: Apache-2.0
// libFuzzer target: any bytes saved as a file and opened with probe and load either fail with
// a reason or give an image that renders and converts for display.
#include "pfx/io.h"
#include "pfx/pipeline.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    static const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("pfx-fuzz-" + std::to_string(getpid()) + ".img");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data),
                  static_cast<std::streamsize>(size)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    (void)pfx::probe(path);
    auto loaded = pfx::load(path, {256});
    if (!loaded) {
        return 0;
    }
    pfx::Settings settings;
    settings.set("light.exposure", 0.5);
    settings.set("detail.clarity", 30.0);
    const auto rendered = pfx::render(loaded.value().image, settings);
    if (!rendered) {
        std::abort();
    }
    (void)pfx::toSrgb8(rendered.value());
    return 0;
}
