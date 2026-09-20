#ifndef WAVEFRONT_H_
#define WAVEFRONT_H_

/*
 * wavefront.h — Wavefront-fidelity instrumentation for the pulsating shell.
 *
 * Measures, once per completed light frame (reading lattice_curr only):
 *   1. Shell completeness — for each integer radius r that was the active
 *      target of some layer in a frame, the observed number of active cells
 *      on that shell divided by the theoretical number of lattice points
 *      with r^2 <= dx^2+dy^2+dz^2 < (r+1)^2  (|dx|,|dy|,|dz| <= r).
 *      A faithful constant-speed front should reach completeness ~ 1.
 *   2. Radial amplitude profile <|u|>(r) correlated against sin(r)/r.
 *   3. Peak-radius error — |observed peak of <|u|> - target radius| per
 *      layer, a direct check that the wave peak advances one cell per tick.
 *
 * Theoretical shell size is computed exactly by counting integer lattice
 * points (no isqrt involved), valid for r <= RMAX = L/2 on the 3-torus.
 */

namespace automaton
{
  namespace wavefront
  {
    /// Reset accumulators. Call once after tryAllocate().
    void begin();

    /// Scan lattice_curr after a completed light frame (frame = 1-based id).
    void sampleFrame(unsigned frame);

    /// Console report (stdout).
    void report();
  }
}

#endif /* WAVEFRONT_H_ */