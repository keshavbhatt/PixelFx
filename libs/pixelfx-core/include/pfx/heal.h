// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/settings.h"

namespace pfx {

// Spot heal (FEATURES E20). render() heals the spots of the "heal.spots" setting first of all,
// on the upright photo; these are the same steps for a host that wants them on their own.

// Covers `spot` on `image` with its source, the source's tone fitted to the area around the
// spot (a plane per channel, so a spot on a sky gradient matches both sides), blended in by
// the feather and the opacity. `image` is the upright photo the spot's fractions refer to.
void healSpot(Image& image, const Spot& spot, int threads = 0);

// Picks a source for `spot` near it on `image` ("Patch picked for you"): of the candidate
// places around the spot that stay inside the photo and do not overlap it, the one whose
// surroundings look most like the spot's own and whose inside looks like those surroundings
// (so another blemish is not copied in), slightly preferring near ones. Returns the spot with
// `sourceX` and `sourceY` set.
[[nodiscard]] Spot findHealSource(const Image& image, Spot spot);

} // namespace pfx
