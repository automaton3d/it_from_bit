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

#ifdef AXIS_ELECTION_TRACE
    // ---------------------------------------------------------------------------
    // Measurement probe (/D AXIS_ELECTION_TRACE): it changes no rule.  Every
    // election that installs an axis is attributed to the path that produced it and
    // scored against the octant of the layer's own charge word -- the quantity the
    // harness's axis-align line measures over the whole lattice.  The paths are
    //
    //   1 classic     a unique (pol_u, pol_v) candidate won the tournament;
    //   2 placement   POLAR_SEED_FROM_PLACEMENT: no candidate at all, so the source
    //                 centre itself was elected, and the axis is the offset from the
    //                 lattice centre to it -- the octant the dispersal walked along,
    //                 unless the encounter has since carried the centre elsewhere;
    //   3 bootstrap   POLAR_BOOTSTRAP_ADDRESS installed the address direction;
    //   4 charge      POLAR_AXIS_FROM_CHARGE installed the charge octant itself.
    //
    // The counters are cumulative (the harness prints the totals each frame) and the
    // accessors report the LAST installation for a layer, so the standing m of a
    // layer can be attributed to the election that owns it -- which is what turns a
    // decay of the alignment into a statement about a path.
    // ---------------------------------------------------------------------------
    int      axisTracePath(unsigned w);       // 0 none/untraced, else a path above
    int      axisTraceSigns(unsigned w);      // 0..3 signs of the installed axis vs the octant
    unsigned axisTraceElections(unsigned w);  // installations recorded for this layer

    extern long long axisElectTotal;          // installations, all paths
    extern long long axisElectFirst;          // ... with m == 0 before
    extern long long axisElectRe;             // ... with m already set (a re-election)
    extern long long axisElectPath[5];        // ... by path
    extern long long axisElectAligned3[5];    // ... by path, all three signs matching
#endif

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
