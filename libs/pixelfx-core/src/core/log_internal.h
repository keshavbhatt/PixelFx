// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/log.h"

#include <string_view>

namespace pfx::detail {

void log(LogLevel level, std::string_view message);

} // namespace pfx::detail
