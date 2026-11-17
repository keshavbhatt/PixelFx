// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>

namespace pfx::detail {

// Runs body(begin, end) over [0, count) split into contiguous chunks on up to `threads`
// threads (0: hardware concurrency), the caller's among them, from a pool started once. Blocks
// until all chunks are done; an exception from a chunk is raised here after the rest finish.
void parallelFor(int count, int threads, const std::function<void(int, int)>& body);

} // namespace pfx::detail
