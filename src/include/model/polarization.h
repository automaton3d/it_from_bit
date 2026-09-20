#ifndef POLARIZATION_H_
#define POLARIZATION_H_

/* Experimental polarization-only election: rank existing nonzero
 * (pol_u, pol_v) lexicographically. Tied maxima remain unelected.
 * No global seed or coordinate tie-break is used. This hypothesis cannot
 * bootstrap the present zero-polarization seed: broadcast requires an axis,
 * while reconstruction requires broadcast stamps. Broadcast/reconstruction
 * below remain the reference mechanism; this does not certify locality.
 */

#include "model/simulation.h"

namespace automaton
{
  namespace polarization
  {
    /// Release all per-layer walker state (call before re-allocation).
    void resetAll();

    /// One automaton tick of election / helical broadcast orchestration.
    /// Must be called once per update_lattice_cpu(), AFTER lattice_curr
    /// holds the promoted phase state (post std::swap).
    void tick();

    /// True while layer w's helical walk is still advancing.
    bool walkLive(unsigned w);

    /// Elected axis for layer w (|axis| = RMAX), or nullptr before the
    /// first election.  Host-side read-only view for GUI/debug.
    const int* electedAxis(unsigned w);

    /// Experimental bootstrap: install axis (ax,ay,az) on layer w and start
    /// its helical broadcast walker immediately, so phase_step() begins to
    /// reconstruct pol_u/pol_v (and thus pB/sB) without waiting for the
    /// self-election that cannot bootstrap from zero polarisation.  The axis
    /// is a topological initial datum for prepared runs (dressed-island
    /// experiment); ordinary seeds do not call this.
    bool seedAxis(unsigned w, int ax, int ay, int az);

    /// Reconstruction stage: convert an arrival stamp into the transverse
    /// polarisation pair approximating the circle pol_u^2 + pol_v^2 = R^4.
    /// Stamps store (arrival tick + 1), so 0 unambiguously means "never
    /// reached" and cannot alias with a tick congruent to 0 mod 2R^2.
    inline void reconstructPair(unsigned int bstamp_, int R, int& pu, int& pv)
    {
      if (bstamp_ == 0u || R <= 0)
      {
        pu = 0;
        pv = 0;
        return;
      }
      unsigned int tick = bstamp_ - 1u;
      unsigned int phase_full = 2u * (unsigned int)R * (unsigned int)R;
      unsigned int cell_phase = tick % phase_full;
      int j = (int)(cell_phase / (unsigned int)R);
      int s;
      if (j < R)
      {
        pu = R * (R - 2 * j);
        s = isqrt(j * (R - j));
        pv = 2 * R * s;
      }
      else
      {
        int j2 = j - R;
        pu = R * (2 * j - 3 * R);
        s = isqrt(j2 * (R - j2));
        pv = -2 * R * s;
      }
    }
  }
}

#endif /* POLARIZATION_H_ */
