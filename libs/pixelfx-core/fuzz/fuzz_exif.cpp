// SPDX-License-Identifier: Apache-2.0
// libFuzzer target: any bytes given to the Exif reader and the location remover come back
// without a crash or a read out of bounds, and removing the location keeps the size.
#include "pfx/io.h"

#include <cstdint>
#include <cstdlib>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::vector<std::uint8_t> bytes(data, data + size);
    [[maybe_unused]] const pfx::ExifSummary summary = pfx::readExif(bytes);
    if (pfx::exifWithoutLocation(bytes).size() != bytes.size()) {
        std::abort();
    }
    return 0;
}
