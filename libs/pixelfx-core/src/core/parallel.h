// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>

namespace pfx::detail {

// Runs body(begin, end) over [0, count) split into contiguous chunks on up to `threads`
// threads (0: hardware concurrency). Blocks until all chunks are done.
void parallelFor(int count, int threads, const std::function<void(int, int)>& body);

} // namespace pfx::detail
