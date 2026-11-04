// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <string_view>

namespace pfx {

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error,
};

using LogSink = std::function<void(LogLevel, std::string_view)>;

// The library writes nothing by itself; the host decides where messages go.
void setLogSink(LogSink sink);

[[nodiscard]] const char* versionString();

} // namespace pfx
