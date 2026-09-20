#pragma once

#include <vector>
#include <atomic>
#include <cstdint>

namespace sinc_overlay {

// Update the self-contained radial sinc(r) / r*sin(r) overlay CA.
void update(unsigned selectedW);

// Read-front accessors for the HUD overlay.
const std::vector<float>& profile();
const std::vector<float>& triggerRate();
const std::vector<float>& peakHistory();
const std::vector<float>& andMask();
unsigned currentRadius();
unsigned graphSize();
bool ready();

// ---------------------------------------------------------------------
// Sine-mask trail of the selected layer.
//
// visitedMask() is a flat, per-cell buffer indexed with the lattice layout
// ((x*ELY) + y)*ELZ + z (same as getCell) and holds the cells that were a
// sine-mask hit (cell.active && cell.s2B) during the LAST PASS OF THE
// WAVEFRONT THAT PRODUCED HITS: one breathing period, i.e. 2*RMAX light
// frames, with the current light frame excluded.
//
// The pass is the unit of memory, not a fixed sliding window: with the
// reference sieve modulus the gate closes in steady state, and the last
// productive pass is kept on screen instead of expiring (a strict window
// would leave the 3-D view empty).  Every tick records a hit, so the trail is
// the union of the gate draws over that pass: the 3-D counterpart of the red
// "gate hits" points of the 2-D radial profile, drawn at a much lower
// intensity than the current sine mask.
//
// The buffer is double buffered: the simulation thread fills the back copy
// while the render thread reads the front one.
// ---------------------------------------------------------------------
const std::vector<uint8_t>& visitedMask();
unsigned maskSize();

} // namespace sinc_overlay
