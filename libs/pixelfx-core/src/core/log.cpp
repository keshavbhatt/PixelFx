// SPDX-License-Identifier: Apache-2.0
#include "core/log_internal.h"

#include <mutex>
#include <utility>

namespace pfx {
namespace {

std::mutex& sinkMutex()
{
    static std::mutex mutex;
    return mutex;
}

LogSink& sinkStorage()
{
    static LogSink sink;
    return sink;
}

} // namespace

void setLogSink(LogSink sink)
{
    const std::scoped_lock lock(sinkMutex());
    sinkStorage() = std::move(sink);
}

const char* versionString()
{
    return PFX_VERSION_STRING;
}

namespace detail {

void log(LogLevel level, std::string_view message)
{
    const std::scoped_lock lock(sinkMutex());
    if (sinkStorage()) {
        sinkStorage()(level, message);
    }
}

} // namespace detail
} // namespace pfx
