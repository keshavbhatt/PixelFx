// SPDX-License-Identifier: Apache-2.0
#include "core/parallel.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace pfx::detail {

void parallelFor(int count, int threads, const std::function<void(int, int)>& body)
{
    if (count <= 0) {
        return;
    }
    int workers = threads > 0 ? threads : static_cast<int>(std::thread::hardware_concurrency());
    workers = std::clamp(workers, 1, count);
    if (workers == 1) {
        body(0, count);
        return;
    }
    std::vector<std::jthread> pool;
    pool.reserve(static_cast<std::size_t>(workers));
    const int chunk = (count + workers - 1) / workers;
    for (int begin = 0; begin < count; begin += chunk) {
        const int end = std::min(count, begin + chunk);
        pool.emplace_back([&body, begin, end] { body(begin, end); });
    }
}

} // namespace pfx::detail
