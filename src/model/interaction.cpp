/*
 * interaction.cpp
 *
 * This file contains important routines used to
 * evaluate interactions in the CA.
 */

#include <cassert>
#include <algorithm>
#include <utility>
#include "model/simulation.h"
#include "model/island_identity.h"
#include "model/chief_transition.h"
#include "globals.h"

namespace automaton
{
  extern unsigned EL;
  extern unsigned W_USED;

  int count = 0;

  bool ctrl = true; // debug

  // Diagnostic counters. Internal mechanical contacts count in enc_calls
  // but bypass the electroweak counters; historical rates are not comparable.
  long long enc_calls = 0;       // encounter() invocations that passed active checks
  long long enc_s2b   = 0;       // ... that also passed the s2B gate
  long long enc_pair  = 0;       // ... that formed a pair (canFormPair && samePos && sameT)
  long long enc_self  = 0;       // ... rejected by the same-W-island guard
  long long enc_collapse = 0;    // gate-passing contacts in the collapse (force) branch
  long long enc_adiah   = 0;     // gate-passing contacts in the adiabatic (attraction) branch
  long long enc_repel   = 0;     // gate-passing contacts that repel one light-step

  // Experimental P2 (macro ORPHAN_GUIDANCE_FSM): orphan-shell x free-photon
  // recruitment events.  Kept apart from the enc_* family so the campaign
  // logs stay comparable; always zero without the macro.
  long long recruit_events = 0;
  long long recruit_repel  = 0;   // ... whose impulse repelled the two islands
  long long recruit_attract = 0;  // ... whose impulse attracted the two islands
  long long annihilations  = 0;   // representative pairs annihilated (rule 5)

  // Candidate HOMB_PRODUCER_FSM: directional producers ported from the archived
  // CUDA kernel (see the block inside encounter()).  Stays zero unless the macro
  // is compiled, so the reference measurement is unaffected.
  long long homb_events   = 0;   // carrier/homer pairs formed by the ported producers
  // Downstream-chain probes (WP8, same macro): the producer is only useful if the
  // pre-existing SLOT II homing block actually SEES a homB neighbour, and if the
  // c[] channel it drives ever reaches a source centre -- where applyMomentum
  // consumes reloc[], NOT c[].  These three counters localise the break.
  long long homb_seen     = 0;   // SLOT II homing block found a homer next to an active cell
  long long c_at_center   = 0;   // scalar c[] nonzero AT a source centre (carrier arrived)
  long long cB_at_center  = 0;   // ... whose centre cell also carries the cB flag
  long long reloc_moves   = 0;   // source centres actually relocated (reloc[] consumed)
  long long reloc_cells   = 0;   // cells migrated by the c[]-driven topological relocate()
  long long consumer_transports = 0;  // candidate HOMB_CONSUMER_TRANSPORT: c[] decoded into reloc[]
  long long c_with_reloc  = 0;   // diagnostic: c[] nonzero AND reloc[] already pending
  // WP8 locality probe: do the L/3 copies of a family step ALIKE when each one
  // decodes its own field LOCALLY (its own c[] and its own x[] only)?  If they do,
  // the co-movement is emergent and the non-local co-location predicate of
  // FAMILY_RIGID_FSM is redundant.
  long long fam_all_agree = 0;   // all decodable copies of a family gave the same step
  long long fam_split     = 0;   // ... they gave different steps
  long long fam_now_ge2   = 0;   // ticks where >= 2 copies of a family had a field AT ONCE
  long long address_walks = 0;   // candidate ADDRESS_TARGET_FSM: centre steps toward its address site
  // Which COPY of a family receives the field?  (w % 3 = position inside the
  // family, so these three counters break the arrivals down by copy.)
  long long c_center_c0   = 0;   // arrivals at the first copy of a family
  long long c_center_c1   = 0;   // ... second
  long long c_center_c2   = 0;   // ... third
  long long loc_step_total = 0;  // copies whose LOCAL decode yields a non-zero step

  // Backward-compatible aliases (deprecated; new code should use enc_*).
  // References, so the old conv_* readers (alpha_probe / campaign logs)
  // observe the same values with no further changes.
  long long& conv_calls    = enc_calls;
  long long& conv_s2b      = enc_s2b;
  long long& conv_pair     = enc_pair;
  long long& conv_self     = enc_self;
  long long& conv_collapse = enc_collapse;
  long long& conv_adiah    = enc_adiah;
  long long& conv_repel    = enc_repel;

#ifdef SURFACE_ESCAPE_FSM
  // Candidate (item 4 of WORK_PLAN): members released by the surface-escape
  // rule.  Macro-guarded; the reference build does not see it.
  unsigned surface_escapes = 0;
#endif

#ifdef SPIN_GATED_FSM
  // J-programme S1/S3 (experiments/J1_SPIN.md): releases vetoed because the
  // leaving delegate would carry away the group's last unit of circulation.
  // SPIN_GATED_FSM is always built together with SURFACE_ESCAPE_FSM -- it gates
  // that rule's release test -- so it never introduces a counter on its own.
  unsigned spin_vetoes = 0;

  // J-programme S4 (J2b): the |J| magnitude cap.  spin_jmax2 == 0 disables it.
  // When a chief's |J|^2 exceeds the cap the rotation is over-driven, so the
  // OUTERMOST member is shed, overriding the S3 veto for that one member.  The
  // cap is a runtime input so the same binary serves both the protect test
  // (cap off) and the selection test (cap on), keeping the build matrix small.
  unsigned long long spin_jmax2 = 0;
  unsigned spin_cap_evictions = 0;   // members shed by the S4 cap
#endif

#ifdef WINDING_GATED_FSM
  // T1 spatial-winding programme: SURFACE_ESCAPE leave-veto gated by a winding
  // label on the chief.  Always built with SURFACE_ESCAPE_FSM.  The probe plants
  // chief_W; production builds leave it at zero (macro off).
  unsigned winding_vetoes = 0;
  int chief_W[3] = {0, 0, 0};
#endif

#ifdef PAIR_STACK_ABSORB_FSM
  // Candidate (multi-frequency mechanics): free S+S formations that absorbed
  // identical co-located stacks.  Always zero without the macro, so the
  // reference measurement is unaffected.
  long long enc_absorb = 0;
#endif

#ifdef MULTIFREQ_RAY_FSM
  // Candidate (multi-frequency mechanics, the required-mechanics pieces):
  // per-tick results of the ray detection for each W source.  mfHitState[w]
  // is the binary freq_hit of the manuscript; mfDetU/mfDetPos record the
  // interrogation inputs (probe visibility).  All reset every tick by
  // beginSourceTick(); always empty without the macro.
  std::vector<unsigned char> mfHitState;
  std::vector<unsigned>      mfDetU;
  std::vector<std::array<int, 3>> mfDetPos;
  long long mf_reads = 0;   // frequency-bearing sides whose ray was walked
  long long mf_hits  = 0;   // ... whose detection cell answered freq_hit = 1
  long long mf_gated = 0;   // pB/sB channel openings suppressed by freq_hit = 0
#endif

  namespace
  {
    std::vector<Cell> sourceBefore, sourceAfter;

    // ---- pending source impulses -----------------------------------------------------
    //
    // Where the booking lives matters.  `sourceAfter` is a per-tick working copy: it is
    // re-seeded from the lattice at every beginSourceTick() (`sourceAfter = sourceBefore`)
    // and it is discarded at the end of the tick, so a displacement written only there is
    // gone before the next tick's commitSourceTick() -- and, measured, before the commit of
    // the SAME tick when the booking happens after it (L=7, sieve open: 1456 non-zero
    // bookings in one tick, 0 pending at the commit, 0 applied by applyMomentum, 0 sources
    // moved, occupiedCenters = 1 for the whole era).
    //
    // The durable home for a pending displacement is the LATTICE itself: the source-centre
    // cell of layer w in lattice_curr.  bookImpulse()/reemitSourceAt() write `reloc` there,
    // and the value then survives every tick boundary by construction -- beginSourceTick()
    // captures it (sourceBefore = that cell), commitSourceTick() hands it to the draft, and
    // applyMomentum() consumes it, zeroing the draft; swap_lattices() then copies the zero
    // back into lattice_curr, so an impulse is applied exactly once.
    //
    // NB: the impulse paths that write a source-level `reloc` directly instead of going
    // through reemitSourceAt()/bookImpulse() are all inside candidate macros that no build
    // defines (the absorption, exclusion/repulsion and collision rules); they would need
    // bookImpulse(w, axis, step) applied at the same place if switched on.
    /// Book a pending displacement of source w for this tick (drained by the commit).
    void bookImpulse(unsigned w, int dx, int dy, int dz)
    {
      if (w >= W_USED) return;
      if (dx == 0 && dy == 0 && dz == 0) return;
      g_pendingImpulses.push_back(ImpulseBooking{w, dx, dy, dz});
#ifdef S2B_TRACE
      ++s2bTraceImpulseBooked;
#endif
    }

    std::vector<std::pair<WIndex, WIndex>> internalContacts;
    std::vector<unsigned char> contactSeen;
#ifdef PARENT_SELECTIVE_FSM
    // Candidate (2026-09-19): equal-charge delegates of DIFFERENT dynamical islands,
    // recorded once per light frame for the one-step separation push applied at the frame
    // edge.  Same-island delegates are never recorded because their D x D contact does
    // nothing at all -- identity preserved, no step, no record; it is NOT a cohesion step
    // (corrected 19 Sep 2026: the equal-charge D x D early return inside `encounter()` precedes
    // the `internal` recording, so such a pair never reaches the cohesion loop).
    std::vector<std::pair<WIndex, WIndex>> parentRepel;
#ifdef PARENT_FUSION_ABSORB
    // Candidate (completed rule set, 2026-09-19): on a K x K fusion the demoted chief's
    // delegation must be ABSORBED (re-pointed to the surviving chief) instead of orphaned --
    // that is what makes "the two agglutinate" grow an island rather than dissolve it.
    std::vector<std::pair<WIndex, WIndex>> fusionAbsorb;   // {demotedW, survivorW}
    long long pselKClashes = 0;   // K x K encounters seen this frame, any ordering (diagnostic)
#endif
#endif
#ifdef SURFACE_ESCAPE_FSM
    // Candidate (item 4): shell-overlap signal for the escape rule.  A separate
    // bitmap from contactSeen, because the reference `internal` predicate covers
    // only delegate-delegate / pair cases and adding entries there would also add
    // a cohesion step.  Here any contact between two same-charge sources counts.
    std::vector<unsigned char> surfaceContactSeen;
#endif
    std::vector<std::array<long long,3>> transportDebt;
    unsigned long long transportFrame = 0;
#ifdef SURFACE_ESCAPE_FSM
    // Per source: consecutive light frames without an internal contact with its
    // own group (the escape timer).  Sized lazily, like contactSeen above.
    std::vector<unsigned> framesWithoutContact;
#endif
#ifdef ORPHAN_GUIDANCE_FSM
    // Experimental P2: island pairs whose mediator engaged a shell during this
    // frame, with the requested sign (+1 repel / -1 attract).  Collected by
    // encounter() and applied once per pair per frame by resolveRecruitPush()
    // at the frame edge (the internalContacts / resolveExclusionPush pattern).
    std::vector<std::array<int, 3>> recruitPushes;
#endif
#ifdef ORPHAN_GUIDANCE_FSM
    // Island pairs already given their mediated light-step in this tick (the
    // shell coincidence is seen by many lattice cells; the impulse must land
    // once per pair per tick).  Cleared by beginSourceTick().
    std::vector<std::array<unsigned, 2>> recruitApplied;
#ifdef ORPHAN_KICK_PER_WINDOW
    // Flux-proportional scheme: one entry per ENGAGEMENT EVENT (cell x tick)
    // {island A, island B, sign}.  Aggregated once per tick by
    // resolveRecruitHits(), which pays out one kick per pair per tick with a
    // magnitude of min(events, ORPHAN_KICK_CAP) - so the momentum transfer
    // tracks the number of mediating interactions (the flux) instead of being
    // a fixed single step.
    std::vector<std::array<int, 3>> recruitHits;
#endif
#endif

    inline const std::array<unsigned, 3>& sourceCenter(const Cell& c)
    {
      return lcenters[c.x[3]];
    }

    inline Cell& sourceCenterDraft(const Cell& c)
    {
      return sourceAfter[c.x[3]];
    }

    inline Cell& sourceCenterCurr(const Cell& c)
    {
      return sourceBefore[c.x[3]];
    }

    inline int shortestDelta(int a, int b, int mod)
    {
      int d = b - a;
      if (mod > 0)
      {
        int half = mod / 2;
        if (d > half) d -= mod;
        else if (d < -half) d += mod;
      }
      return d;
    }

    inline int sign(int v)
    {
      return (v > 0) - (v < 0);
    }

    // Pair-formation test from the six superposing-bubble rules.
    bool canFormPair(const Cell& a, const Cell& b)
    {
      unsigned char ca = a.ch;
      unsigned char cb = b.ch;
      // Rule 3: both neutral (000000)
      if (ca == 0x00 && cb == 0x00) return true;
      // Rule 4: both anti-neutral (111111)
      if (ca == 0x3F && cb == 0x3F) return true;
      // Rule 1: every charge bit complementary
      if ((ca ^ cb) == 0x3F) return true;

      bool qa = (ca & 0x08) != 0;
      bool qb = (cb & 0x08) != 0;
      bool w1a = (ca & 0x20) != 0;
      bool w1b = (cb & 0x20) != 0;
      bool w0a = (ca & 0x10) != 0;
      bool w0b = (cb & 0x10) != 0;
      unsigned char cola = ca & 0x07;
      unsigned char colb = cb & 0x07;

      // Rule 2: q, w0 and color complementary; w1 identical
      if ((qa ^ qb) && (w1a == w1b) && (w0a ^ w0b) && ((cola ^ colb) == 0x07)) return true;
      // Rule 5: q=0, w1=0, w0=1, same non-neutral color
      if (!qa && !qb && !w1a && !w1b && w0a && w0b && cola == colb && cola != 0x00 && cola != 0x07) return true;
      // Rule 6: q=1, w1=1, w0=0, same non-neutral color
      if (qa && qb && w1a && w1b && !w0a && !w0b && cola == colb && cola != 0x00 && cola != 0x07) return true;
      return false;
    }

    // Blob-formation test (manuscript "Blob" and "Superposing bubbles").
    // A blob is a group of superposed equal-status pairs.  Only the R2
    // charge geometry qualifies (same sector w1 with q/w0/color
    // complementary -- gluon/photon).  The specialized pairs never blob:
    // neutrino/antineutrino (R3/R4), up quark (R5/R6), graviton (R1,
    // fully complementary charges) and propeller (a pair currently acting
    // as a momentum carrier -- one with a pending relocation impulse).
    bool canFormBlob(const Cell& a, const Cell& b)
    {
      const unsigned char ca = a.ch;
      const unsigned char cb = b.ch;

      // R3 / R4: fully neutral words are neutrinos / antineutrinos.
      if (ca == 0x00 && cb == 0x00) return false;
      if (ca == 0x3F && cb == 0x3F) return false;

      // R1: fully complementary charges are gravitons.
      if ((ca ^ cb) == 0x3F) return false;

      const bool qa  = (ca & 0x08) != 0, qb  = (cb & 0x08) != 0;
      const bool w1a = (ca & 0x20) != 0, w1b = (cb & 0x20) != 0;
      const bool w0a = (ca & 0x10) != 0, w0b = (cb & 0x10) != 0;
      const unsigned char cola = ca & 0x07u, colb = cb & 0x07u;

      // R5 / R6: same weak bits and the same non-trivial color -> up quark.
      if (qa == qb && w1a == w1b && w0a == w0b &&
          cola == colb && cola != 0x00u && cola != 0x07u)
        return false;

      // Affiliation and m identify a propeller, never a received impulse.
      if (isBoundPropeller(a) || isBoundPropeller(b))
        return false;

      // Remaining allowed geometry: same sector w1, complementary q/w0/color.
      return (w1a == w1b) && (qa ^ qb) && (w0a ^ w0b) &&
             ((cola ^ colb) == 0x07u);
    }

#ifdef PAIR_STACK_ABSORB_FSM
    // Candidate absorption for the multi-frequency mechanics: count the free
    // stacks IDENTICAL to the forming pair (a, b) that are superposed on the
    // forming pair's site, register their population on the formed pair, and
    // release the absorbed halves as singletons.  A stack is a mutual P pair
    // with affinity W_USED (free), centred where the forming sources are
    // (same voxel, same wavefront radius f) and carrying the same unordered
    // charge words; only the leader half (lower W address) is processed, and
    // the release reuses the deterministic split of the consumption path in
    // simulation.cpp (axis = leader % 3, sign by parity).  Returns the formed
    // pair's count (1 when nothing was absorbed), clamped to 255 so a growing
    // stack can never wrap the uint8_t bookkeeping.
    uint8_t absorbCoLocatedStacks(const Cell& a, const Cell& b)
    {
      unsigned total = 1;
      for (unsigned w = 0; w < W_USED; ++w)
      {
        const Cell& s = sourceBefore[w];
        if (s.kind != SourceKind::P || s.a != W_USED)
          continue;                       // free stacks only
        if (s.pair_idx >= W_USED || w >= s.pair_idx)
          continue;                       // malformed pair / non-leader half
        const Cell& sp = sourceBefore[s.pair_idx];
        if (sp.kind != SourceKind::P || sp.pair_idx != w || sp.a != W_USED)
          continue;                       // mutual free pair
        // Idempotence inside one sweep: if this stack was already absorbed
        // by an earlier encounter window (encounter is evaluated from both W
        // directions), its sourceAfter kind is already S -- skip, so the
        // release split is applied exactly once.
        if (sourceAfter[w].kind != SourceKind::P ||
            sourceAfter[s.pair_idx].kind != SourceKind::P)
          continue;
        // Superposed on the forming pair's site with the same wavefront
        // radius (the manuscript stack condition: same site, same radius).
        if (s.x[0] != a.x[0] || s.x[1] != a.x[1] ||
            s.x[2] != a.x[2] || s.f != a.f)
          continue;
        // Identical unordered charge words -> the same kind of pair.
        if (!((sp.ch == a.ch && s.ch == b.ch) ||
              (sp.ch == b.ch && s.ch == a.ch)))
          continue;

        // Absorb: release both halves as singletons (consumption split).
        Cell& dl = sourceAfter[w];
        Cell& dp = sourceAfter[s.pair_idx];
        dl.kind       = SourceKind::S;
        dl.pair_idx   = NO_PAIR;
        dl.pair_count = 0;
        dl.leader_w   = NO_LEADER_W;
        dl.a          = W_USED;
        dp.kind       = SourceKind::S;
        dp.pair_idx   = NO_PAIR;
        dp.pair_count = 0;
        dp.leader_w   = NO_LEADER_W;
        dp.a          = W_USED;
        const int axis = (int)(w % 3u);
        const int sign = (w & 1u) ? +1 : -1;
        dl.reloc[axis] += sign;
        dp.reloc[axis] -= sign;
        total += s.pair_count;
      }
      return (total > 255u) ? 255u : (uint8_t)total;
    }
#endif

#ifdef MULTIFREQ_RAY_FSM
    // Multi-frequency ray detection (the manuscript's required mechanics,
    // Sect. "Multi frequency mechanics").  mfFreqBearing: any source that
    // carries a non-trivial pair_count.  mfComputeRayDetect derives the
    // effective tick from the population, walks the pure-climb DDA ray
    // along m (the climb component of the polarization walker with the
    // orbital budget set to zero -- adds and compares only), and
    // interrogates the wave cloud on the detection cell: freq_hit.
    bool mfFreqBearing(const Cell& src)
    {
      return src.kind == SourceKind::P && src.pair_count > 0;
    }

    void mfComputeRayDetect(const Cell& src)
    {
      const unsigned w = src.x[3];
      if (w >= W_USED) return;
      ++mf_reads;

      // Effective tick from the population: t_eff = 2 * pair_count (the even
      // frequency 2n; a shift and an add).  The stamp covers 0..RMAX, so the
      // free scale choice clamps into the walked range.
      int tEff = (int)src.pair_count << 1;
      if (tEff > (int)RMAX) tEff = (int)RMAX;

      // Pure-climb DDA along m: absa[i] = |m[i]|, climb_total =
      // |m0|+|m1|+|m2|; every step is a climb step chosen by the DDA argmax
      // (no multiplication or division of dynamic quantities).
      const int absa[3] = { src.m[0] < 0 ? -src.m[0] : src.m[0],
                            src.m[1] < 0 ? -src.m[1] : src.m[1],
                            src.m[2] < 0 ? -src.m[2] : src.m[2] };
      const int sgn[3]  = { src.m[0] < 0 ? -1 : 1,
                            src.m[1] < 0 ? -1 : 1,
                            src.m[2] < 0 ? -1 : 1 };
      const int climbTotal = absa[0] + absa[1] + absa[2];
      const int edges[3]   = { (int)ELX, (int)ELY, (int)ELZ };
      int dda[3] = { 0, 0, 0 };
      int pos[3] = { (int)src.x[0], (int)src.x[1], (int)src.x[2] };

      int  detU      = 0;
      bool detActive = false, detS2B = false;
      for (int k = 0; k <= (int)RMAX; ++k)
      {
        if (k > 0 && climbTotal > 0)
        {
          dda[0] += absa[0]; dda[1] += absa[1]; dda[2] += absa[2];
          int i = 0;
          if (dda[1] > dda[i]) i = 1;
          if (dda[2] > dda[i]) i = 2;
          dda[i] -= climbTotal;
          pos[i] += sgn[i];
          if (pos[i] < 0)         pos[i] += edges[i];
          if (pos[i] >= edges[i]) pos[i] -= edges[i];
        }
        if (k == tEff)
        {
          // The detection point: the ray cell whose stamp equals t_eff.
          const Cell& c = getCell(lattice_curr, pos[0], pos[1], pos[2], (int)w);
          detU        = c.u;
          detActive   = c.active != 0;
          detS2B      = c.s2B;
          mfDetPos[w] = { pos[0], pos[1], pos[2] };
        }
      }
      const bool hit = (detActive && detS2B) || detU > 0;
      mfHitState[w] = hit ? 1u : 0u;
      mfDetU[w]     = (unsigned)detU;
      if (hit) ++mf_hits;
    }

    // The gated channel flags: a frequency-bearing side whose detection bit
    // did not fire contributes no pB/sB ("only after freq_hit is true is the
    // ordinary pB/sB channel allowed to fire for that frequency mode").
    bool mfGatedFlag(const Cell& src, bool flag)
    {
      if (mfFreqBearing(src) && !mfHitState[src.x[3]])
      {
        if (flag) ++mf_gated;
        return false;
      }
      return flag;
    }
#endif

#ifdef S2B_TRACE
    // Tag which site contributed a displacement to this layer in the current light frame (the mask
    // is read by the harness and by the S2B_DUMP_PENDING probe; see simulation.h for the bit list).
    inline void tagWriter(unsigned w, unsigned bit)
    {
      if (w < s2bTraceWriterMask.size()) s2bTraceWriterMask[w] |= bit;
    }
#endif

    // Add an impulse (dx, dy, dz) to the source-center cell and reemit phase 0.
    // The long-term momentum-direction vector m is preserved; the consumable
    // relocation vector reloc records the pending displacement.  The actual move
    // is performed after the FSM by applyMomentum().
    void reemitSourceAt(Cell& srcDraft, int dx, int dy, int dz)
    {
      srcDraft.reloc[0] += dx;
      srcDraft.reloc[1] += dy;
      srcDraft.reloc[2] += dz;
#ifdef S2B_TRACE
      if (dx || dy || dz) tagWriter((unsigned)srcDraft.x[3], 1u);   // walk funnel
#endif
      // Book the displacement in the model's impulse queue (see the NOTE in simulation.h):
      // that queue is the only carrier that survives the per-tick re-seed of the source
      // working array.  Written here rather than through bookImpulse() on purpose -- the
      // caller's draft is normally the source cell itself, and the queue must hold the
      // booking exactly once.
      if (dx || dy || dz)
      {
        const unsigned wi = (unsigned)srcDraft.x[3];
        if (wi < W_USED)
        {
          g_pendingImpulses.push_back(ImpulseBooking{wi, dx, dy, dz});
#ifdef S2B_TRACE
          ++s2bTraceImpulseBooked;
#endif
        }
      }
      srcDraft.t = 0;
      srcDraft.f = 0;
#ifdef S2B_TRACE
      ++s2bTraceReemitResets;
      if (dx || dy || dz) ++s2bTraceReemitImpulse;   // non-zero displacement requested
      if (srcDraft.r2 == 0) ++s2bTraceReemitAtCentre; else ++s2bTraceReemitOffCentre;
#endif
      // Fatia 1: ledger the reemission — island w just had its clock reset.
      // Every reemission path funnels through here (reemitAtContact,
      // moveOneStep, moveOneStepAway), so this is the single hook.
      chargesMarkInteraction((unsigned)srcDraft.x[3]);
    }

    // Move one light-step along the direction from 'from' to 'to'.
    // --- Re-anchor to the layer's own charge octant (candidate, /D PAIR_OCTANT_REANCHOR) -------
    // OFF in the reference build.
    //
    // Every walk direction here is `sign(shortestDelta(self, other))`, and shortestDelta takes the
    // short way round the torus.  Measured (README, "Multi-era run with (a)+(b)"): once a layer's
    // centre has crossed the lattice centre in an axis, that offset inverts and the walk runs
    // against the layer's charge octant -- off-side layers were octant-aligned in only 1 of 23 and
    // 8 of 21 steps where on-side layers were 23 of 25 and 9 of 9.
    //
    // So when the geometric step disagrees with the layer's own octant (c2,c1,c0 -> x,y,z signs),
    // take the octant: the interaction still fires, but the displacement always runs along the
    // direction the layer owns.  A zero component (same cell / same coordinate) is NOT a
    // disagreement -- a no-op stays a no-op.  No address, hash, scan order or RNG enters.
    void octantReanchor(const Cell& src, int& dx, int& dy, int& dz)
    {
      const unsigned ch6 = src.ch & 0x3Fu;
      const int sx = sign(dx), sy = sign(dy), sz = sign(dz);
      const int ox = (ch6 & 4u) ? +1 : -1;
      const int oy = (ch6 & 2u) ? +1 : -1;
      const int oz = (ch6 & 1u) ? +1 : -1;
      if (sx * ox < 0 || sy * oy < 0 || sz * oz < 0) { dx = ox; dy = oy; dz = oz; }
    }

    void moveOneStep(Cell& srcDraft, const std::array<unsigned, 3>& from,
                     const std::array<unsigned, 3>& to)
    {
      int dx = shortestDelta((int)from[0], (int)to[0], (int)ELX);
      int dy = shortestDelta((int)from[1], (int)to[1], (int)ELY);
      int dz = shortestDelta((int)from[2], (int)to[2], (int)ELZ);
#ifdef PAIR_OCTANT_REANCHOR
      octantReanchor(srcDraft, dx, dy, dz);
#endif
      reemitSourceAt(srcDraft, sign(dx), sign(dy), sign(dz));
    }

    // Move one light-step away from the other source center.
    void moveOneStepAway(Cell& srcDraft, const std::array<unsigned, 3>& selfCenter,
                         const std::array<unsigned, 3>& otherCenter)
    {
      int dx = shortestDelta((int)otherCenter[0], (int)selfCenter[0], (int)ELX);
      int dy = shortestDelta((int)otherCenter[1], (int)selfCenter[1], (int)ELY);
      int dz = shortestDelta((int)otherCenter[2], (int)selfCenter[2], (int)ELZ);
#ifdef PAIR_OCTANT_REANCHOR
      octantReanchor(srcDraft, dx, dy, dz);
#endif
      reemitSourceAt(srcDraft, sign(dx), sign(dy), sign(dz));
    }

    // One light-step toward another centre WITHOUT resetting the clock (the
    // clock-preserving twin of moveOneStep, used by the relay shuttle).
    void reseatStepToward(Cell& srcDraft, const std::array<unsigned, 3>& self,
                          const std::array<unsigned, 3>& other)
    {
      const int dx = shortestDelta((int)self[0], (int)other[0], (int)ELX);
      const int dy = shortestDelta((int)self[1], (int)other[1], (int)ELY);
      const int dz = shortestDelta((int)self[2], (int)other[2], (int)ELZ);
#ifdef S2B_TRACE
      // This was the only reloc writer with no counter.  It steps toward ANOTHER centre, so
      // its direction is the partner's offset, not the layer's own axis: this is what the
      // residual non-octant-aligned displacements of the (a)+(b) runs are suspected to be.
      if (sign(dx) || sign(dy) || sign(dz))
      {
        ++s2bTraceReseatSteps;
        const unsigned ch6 = srcDraft.ch & 0x3Fu;
        unsigned a = 0;
        if (sign(dx) * ((ch6 & 4u) ? +1 : -1) > 0) ++a;
        if (sign(dy) * ((ch6 & 2u) ? +1 : -1) > 0) ++a;
        if (sign(dz) * ((ch6 & 1u) ? +1 : -1) > 0) ++a;
        if (a == 3u) ++s2bTraceReseatAligned; else ++s2bTraceReseatOther;
        tagWriter((unsigned)srcDraft.x[3], 2u);   // relay shuttle
      }
#endif
      srcDraft.reloc[0] += sign(dx);
      srcDraft.reloc[1] += sign(dy);
      srcDraft.reloc[2] += sign(dz);
      chargesMarkInteraction((unsigned)srcDraft.x[3]);
    }

    // Reemit at the contact voxel (curr position) without changing kind.
    void reemitAtContact(Cell& srcDraft, const Cell& contact)
    {
      int dx = shortestDelta((int)srcDraft.x[0], (int)contact.x[0], (int)ELX);
      int dy = shortestDelta((int)srcDraft.x[1], (int)contact.x[1], (int)ELY);
      int dz = shortestDelta((int)srcDraft.x[2], (int)contact.x[2], (int)ELZ);
#ifdef PAIR_OCTANT_REANCHOR
      // Same re-anchor as moveOneStep/moveOneStepAway: this path writes reloc directly and was the
      // mover the first re-anchor attempt missed (measured: frames 12 and 14 kept the off-side
      // misalignments).  Note it books the FULL offset to the contact, so re-anchoring also brings
      // the step down to one cell along the octant.
      octantReanchor(srcDraft, dx, dy, dz);
#endif
      reemitSourceAt(srcDraft, dx, dy, dz);
    }

    // Displace a source centre to the contact point WITHOUT resetting its clock.
    // Used by the recruit relay: resetting t there re-pins the free mediator to
    // phase 0 on every engagement, which stops the photon from propagating (see
    // the parked-probe analysis in the design note).
    void reseatAtContact(Cell& srcDraft, const Cell& contact)
    {
      const int dx = shortestDelta((int)srcDraft.x[0], (int)contact.x[0], (int)ELX);
      const int dy = shortestDelta((int)srcDraft.x[1], (int)contact.x[1], (int)ELY);
      const int dz = shortestDelta((int)srcDraft.x[2], (int)contact.x[2], (int)ELZ);
#ifdef S2B_TRACE
      // Sibling of reseatStepToward, used by the recruit relay: it books the FULL offset to the
      // contact point (not one unit step), so it can displace a centre by several cells at once.
      if (dx || dy || dz) ++s2bTraceReseatAtContact;
      if (dx || dy || dz) tagWriter((unsigned)srcDraft.x[3], 4u);   // relay contact
#endif
      srcDraft.reloc[0] += dx;
      srcDraft.reloc[1] += dy;
      srcDraft.reloc[2] += dz;
      chargesMarkInteraction((unsigned)srcDraft.x[3]);
    }

    // Convenience: make a cell adopt a leader identity.
    inline void adoptLeader(Cell& dst, WIndex leader)
    {
      dst.leader_w = leader;
      dst.a = (unsigned)leader;
    }

    // Choose the dominant leader between two sources; if neither has one,
    // fall back to the smaller intrinsic W address.
    inline WIndex dominantLeader(const Cell& a, const Cell& b)
    {
      WIndex leader = std::min(a.leader_w, b.leader_w);
      if (leader == NO_LEADER_W)
        leader = std::min(a.w, b.w);
      return leader;
    }
  }

  bool isBoundPropeller(const Cell& s)
  {
    return s.kind == SourceKind::P && s.a < W_USED &&
           s.pair_idx < W_USED && s.pair_count > 0 &&
           (s.m[0] != 0 || s.m[1] != 0 || s.m[2] != 0);
  }

  bool hadInternalContact(WIndex a, WIndex b)
  {
    if (a >= W_USED || b >= W_USED || contactSeen.size() != (size_t)W_USED * W_USED)
      return false;
    if (a > b) std::swap(a, b);
    return contactSeen[(size_t)a * W_USED + b] != 0;
  }

  void resetSourceTransactions()
  {
    sourceBefore.clear();
    sourceAfter.clear();
    g_pendingImpulses.clear();   // a new run never inherits a pending booking
    internalContacts.clear();
#ifdef PARENT_SELECTIVE_FSM
    parentRepel.clear();
#ifdef PARENT_FUSION_ABSORB
    fusionAbsorb.clear();
#endif
#endif
    contactSeen.clear();
    transportDebt.clear();
    transportFrame = 0;
  }

  void beginSourceTick()
  {
    sourceBefore.resize(W_USED);
#ifdef ORPHAN_GUIDANCE_FSM
    recruitApplied.clear();
#ifdef ORPHAN_KICK_PER_WINDOW
    recruitHits.clear();
#endif
#endif
    if (transportDebt.size() != W_USED) transportDebt.assign(W_USED, {0,0,0});
    for (unsigned w = 0; w < W_USED; ++w) {
      const auto& p = lcenters[w];
      sourceBefore[w] = getCell(lattice_curr, p[0], p[1], p[2], w);
    }
    sourceAfter = sourceBefore;
    // The pending displacements are NOT re-applied here: they live on the lattice itself
    // (bookImpulse/reemitSourceAt write reloc on the source-centre cell) and sourceBefore
    // has just captured them, so the re-seed cannot lose them.  See the NOTE above.
    //
    // Candidate (/D IMPULSE_NO_INHERIT).  OFF in the reference build and in the promoted default.
    //
    // This block was added to test the reading that the off-octant flight steps of the later eras were
    // stale impulses the commit re-applied: the capture above *inherits* whatever reloc the lattice cell
    // still carries, and the commit copies it into the draft.  The test came out negative, and the
    // reading was wrong.  At L = 7 over 20 frames the build with this macro is byte-identical to the
    // build without it (build\inherit_L7.out against build\both_L7_repro.out) and `carried` is zero in
    // every frame; the passive sum of |reloc| over the centre cells at the same reseed
    // (s2bTraceReapplySum, printed as `captured-sum`) is zero in every frame too.  What the counters
    // that motivated the reading actually describe is the QUEUE: frame 14 carries pending-impulse=64 and
    // booked-on-lattice=8 against applied=72, i.e. 64 + 8 = 72, and g_pendingImpulses is the designed
    // carrier of a booking made in an earlier frame.  The macro is kept because it is the probe that
    // settled the question: with it on, a frame's displacements can only come from that frame's bookings
    // and from the queue.  (README, "The inherited-impulse probe".)
#ifdef IMPULSE_NO_INHERIT
    {
      unsigned carried = 0;
      for (unsigned w = 0; w < W_USED; ++w)
        if (sourceBefore[w].reloc[0] || sourceBefore[w].reloc[1] || sourceBefore[w].reloc[2])
        {
          ++carried;
          sourceBefore[w].reloc[0] = sourceBefore[w].reloc[1] = sourceBefore[w].reloc[2] = 0;
          sourceAfter[w].reloc[0] = sourceAfter[w].reloc[1] = sourceAfter[w].reloc[2] = 0;
        }
#ifdef S2B_TRACE
      s2bTraceReseedCarried += carried;
#endif
    }
#endif
#ifdef S2B_TRACE
    ++s2bTraceTicks;
    for (unsigned w = 0; w < W_USED; ++w)
      s2bTraceReapplySum += (unsigned long long)(std::abs(sourceAfter[w].reloc[0]) +
                                                 std::abs(sourceAfter[w].reloc[1]) +
                                                 std::abs(sourceAfter[w].reloc[2]));
#endif
#ifdef MULTIFREQ_RAY_FSM
    mfHitState.assign(W_USED, 0);
    mfDetU.assign(W_USED, 0);
    mfDetPos.assign(W_USED, {0, 0, 0});
#endif
    if ((!lattice_curr.empty() && lattice_curr.front().k == 0) ||
        contactSeen.size() != (size_t)W_USED * W_USED) {
      internalContacts.clear();
#ifdef PARENT_SELECTIVE_FSM
      parentRepel.clear();
#ifdef PARENT_FUSION_ABSORB
      fusionAbsorb.clear();
#endif
#endif
#ifdef ORPHAN_GUIDANCE_FSM
      recruitPushes.clear();
#endif
      contactSeen.assign((size_t)W_USED * W_USED, 0);
#ifdef SURFACE_ESCAPE_FSM
      surfaceContactSeen.assign((size_t)W_USED * W_USED, 0);
#endif
    }
  }

  namespace {
    bool body(const Cell& s) {
      return s.kind == SourceKind::K || s.kind == SourceKind::D;
    }

    int delta(unsigned a, unsigned b, int axis) {
      const unsigned edges[] = {ELX, ELY, ELZ};
      return shortestDelta((int)lcenters[a][axis], (int)lcenters[b][axis],
                           (int)edges[axis]);
    }

    // Select a face step, not a displacement of |m| cells. Rotating the
    // tie breaker avoids privileging x for equal Cartesian components.
    std::array<int, 3> faceStep(const int* v, unsigned turn) {
      int axis = (int)(turn % 3);
      for (int j = 1; j < 3; ++j) {
        int k = (int)((turn + j) % 3);
        if (std::abs(v[k]) > std::abs(v[axis])) axis = k;
      }
      std::array<int, 3> step{0, 0, 0};
      step[axis] = sign(v[axis]);
      return step;
    }

    bool sameAffinity(const Cell& a, const Cell& b) {
      return a.a < W_USED && a.a == b.a;
    }

    std::array<int,3> propellerStep(unsigned p, const int* m) {
      // Integer DDA: distribute unit face-steps in proportion to |m|.
      // Magnitude changes cannot turn one encounter into a long jump.
      long long total=0;
      for(int k=0;k<3;++k) {
        const long long magnitude=m[k]<0?-(long long)m[k]:(long long)m[k];
        transportDebt[p][k]+=magnitude; total+=magnitude;
      }
      int axis=(int)(transportFrame%3);
      for(int k=0;k<3;++k)
        if(transportDebt[p][k]>transportDebt[p][axis]) axis=k;
      transportDebt[p][axis]-=total;
      std::array<int,3> step{0,0,0}; step[axis]=sign(m[axis]);
      return step;
    }

    void resolveInternalContacts()
    {
      // Contacts are voxel-independent events: one unordered source pair
      // per light frame, regardless of shell area and mirrored W visits.
      std::sort(internalContacts.begin(), internalContacts.end());
      internalContacts.erase(std::unique(internalContacts.begin(), internalContacts.end()),
                             internalContacts.end());
#ifdef S2B_TRACE
      // Cohesion-vs-flight split.  Everything booked here is COHESION: a single-axis face step
      // (faceStep, line 701) that holds the members of a K-D island together, by design not a
      // displacement along the layer's diagonal charge octant.  The flag lets the harness report
      // the two channels' alignment separately instead of mixing them (in the (a)+(b) runs the
      // cohesion steps were the whole residual: 14 bookings = 14 moved layers, 7/7 in signs).
      // The vector is rebuilt per call, and this function runs once per light frame.
      if (s2bTraceCohesionFlag.size() != W_USED) s2bTraceCohesionFlag.assign(W_USED, 0u);
      // Size the writer mask as well: without this the tagWriter() calls are no-ops (the vector is
      // empty) and every diagnostic reads a zero mask -- which is how the first masked run reported
      // "no mixtures" while the pending impulses were in fact accumulated own-octant thrusts.
      if (s2bTraceWriterMask.size() != W_USED) s2bTraceWriterMask.assign(W_USED, 0u);
      for (unsigned w = 0; w < W_USED; ++w) s2bTraceCohesionFlag[w] = 0u;
#endif
      std::vector<std::array<int, 3>> moves(W_USED, {0, 0, 0});
      std::vector<bool> moved(W_USED, false);

      // Cohesion: reciprocal face steps close a separation greater than one cell without
      // changing the body's centre of mass.  The `body` and `shareChief` filters mean this loop
      // only ever acts on K-D pairs: the equal-charge D x D early return in `encounter()` runs
      // BEFORE the `internal` recording in the same function, so no D-D pair ever reaches
      // `internalContacts`.  The earlier comment here said "K-D and D-D" -- corrected 19 Sep
      // 2026 (see experiments/WORK_PLAN.md): a delegate is held by its contacts with its CHIEF,
      // not by cohesion between delegates.
      for (const auto& [a, b] : internalContacts) {
        const Cell& sa = sourceAfter[a]; const Cell& sb = sourceAfter[b];
        if (!body(sa) || !body(sb) || !shareChief(sa, sb) || moved[a] || moved[b]) continue;
        int d[3] = {delta(a,b,0), delta(a,b,1), delta(a,b,2)};
        auto step = faceStep(d, (unsigned)transportFrame);
        int axis = step[0] ? 0 : (step[1] ? 1 : 2);
        if (std::abs(d[axis]) <= 1) continue;
        moves[a] = step;
        for (int k=0;k<3;++k) moves[b][k] = -step[k];
        moved[a] = moved[b] = true;
      }

      // Each reciprocal P pair owns one kick opportunity. Prefer the rear
      // eligible contact along m, so repeated kicks do not peel a leading
      // constituent away from its K-D/D-D contacts. Ties rotate by frame.
      for (unsigned p = 0; p < W_USED; ++p) {
        const Cell& prop = sourceAfter[p];
        if (!isBoundPropeller(prop) || p >= prop.pair_idx) continue;
        const unsigned half = prop.pair_idx;
        const Cell& other = sourceAfter[half];
        if (!isBoundPropeller(other) || other.pair_idx != p || !sameAffinity(prop, other)) continue;
        if (effective_t(prop.t) == 0) continue;
        unsigned target = W_USED;
        unsigned followTarget = W_USED;
        long long best = 0;
        for (const auto& [a,b] : internalContacts) {
          unsigned q = W_USED;
          if (a == p || a == half) q = b;
          else if (b == p || b == half) q = a;
          if (q == W_USED || !body(sourceAfter[q]) ||
              !sameAffinity(prop, sourceAfter[q])) continue;
          if (followTarget == W_USED) followTarget = q;
          if (moved[q]) continue;
          long long projection = 0;
          for (int k=0;k<3;++k) projection += (long long)delta(p,q,k) * prop.m[k];
          const auto rank = (q + W_USED - transportFrame % W_USED) % W_USED;
          const auto oldRank = (target + W_USED - transportFrame % W_USED) % W_USED;
          if (target == W_USED || projection < best || (projection == best && rank < oldRank)) {
            target = q; best = projection;
          }
        }
        if (target != W_USED) {
          moves[target] = propellerStep(p, prop.m);
          moved[target] = true;
          followTarget = target;
        }
        if (followTarget == W_USED) continue;
        // Recycle the bound photon pair toward the contacted constituent.
        // Both halves take the same step; affiliation and m are preserved.
        int follow[3];
        for (int k=0;k<3;++k) follow[k] = delta(p,followTarget,k) + moves[followTarget][k];
        moves[p] = moves[half] = faceStep(follow, (unsigned)transportFrame);
        moved[p] = moved[half] = true;
      }
      for (unsigned w = 0; w < W_USED; ++w)
      {
        bookImpulse(w, moves[w][0], moves[w][1], moves[w][2]);
#ifdef S2B_TRACE
        if (moves[w][0] || moves[w][1] || moves[w][2]) { s2bTraceCohesionFlag[w] = 1u; tagWriter(w, 8u); }
#endif
      }
      ++transportFrame;
    }

#ifdef SURFACE_ESCAPE_FSM
    // Candidate (item 4): escape by surface.
    //
    // A delegate that shares no shell overlap with its own group for one full
    // partner rotation is released back to a singleton.  The rotation period is
    // W_USED light frames: rotatePartners() (utils.cpp:36) advances the partner
    // lattice by one slice per frame, so every ordered pair of layers is given
    // the chance to meet within W_USED frames -- "no contact for one rotation"
    // is therefore the local, parameter-free test of "no shell overlap".
    // Contacts are the frame's same-charge source contacts (the shell-overlap
    // signal recorded in encounter()), so the test needs no ISLAND_SIZE and no L.
    void applySurfaceEscape()
    {
      if (framesWithoutContact.size() != W_USED)
        framesWithoutContact.assign(W_USED, 0);
      std::vector<unsigned char> contacted(W_USED, 0);
      for (size_t i = 0; i < surfaceContactSeen.size(); ++i)
        if (surfaceContactSeen[i]) {
          contacted[(unsigned)(i / W_USED)] = 1;
          contacted[(unsigned)(i % W_USED)] = 1;
        }
#ifdef SPIN_GATED_FSM
      // S1 (chief frame): total circulation J = sum over the members of
      // r x m, with r the toroidal offset from the chief and m the immutable
      // member momentum.  Integers only, no L and no ISLAND_SIZE.  A chief
      // contributes r = 0, so only delegates can add to J.
      std::vector<std::array<long long, 3>> chiefJ(W_USED, {0, 0, 0});
      for (unsigned w = 0; w < W_USED; ++w)
      {
        const Cell& s = sourceAfter[w];
        if (s.kind != SourceKind::D) continue;
        const WIndex g = s.parent;
        if (g >= W_USED) continue;
        if (sourceAfter[g].kind != SourceKind::K) continue;   // orphan: no frame
        const long long rx = delta(g, w, 0);
        const long long ry = delta(g, w, 1);
        const long long rz = delta(g, w, 2);
        const long long mx = s.m[0], my = s.m[1], mz = s.m[2];
        chiefJ[g][0] += ry * mz - rz * my;
        chiefJ[g][1] += rz * mx - rx * mz;
        chiefJ[g][2] += rx * my - ry * mx;
      }
#endif
      for (unsigned w = 0; w < W_USED; ++w)
      {
        if (sourceAfter[w].kind != SourceKind::D) { framesWithoutContact[w] = 0; continue; }
        if (contacted[w])                         { framesWithoutContact[w] = 0; continue; }
        if (++framesWithoutContact[w] < W_USED)   continue;
#ifdef SPIN_GATED_FSM
        // S3: a delegate may not leave while its group's circulation is
        // non-zero, because the departure would carry the spin away.  The
        // timer keeps running (no reset), so a group that later sheds J -- or
        // whose contributions cancel -- is released on a later frame.
        {
          const WIndex g = sourceAfter[w].parent;
          if (g < W_USED && sourceAfter[g].kind == SourceKind::K &&
              (chiefJ[g][0] || chiefJ[g][1] || chiefJ[g][2])) {
            ++spin_vetoes;
            continue;
          }
        }
#endif
#ifdef WINDING_GATED_FSM
        // T1 S3: a delegate may not leave while the group's winding label is
        // non-zero.  Timer does not reset -- if W later drops to 0, release can
        // occur on a later frame.  No L / L/3 / ISLAND_SIZE.
        {
          const WIndex g = sourceAfter[w].parent;
          if (g < W_USED && sourceAfter[g].kind == SourceKind::K &&
              (chief_W[0] || chief_W[1] || chief_W[2])) {
            ++winding_vetoes;
            continue;
          }
        }
#endif
        framesWithoutContact[w] = 0;
        sourceAfter[w].kind     = SourceKind::S;
        sourceAfter[w].parent   = NO_PARENT;
        sourceAfter[w].leader_w = NO_LEADER_W;
        ++surface_escapes;
      }
    }


#endif
#ifdef PARENT_SHELL_RELEASE_FSM
    // BRAKE (iii), parameter-free form (2026-09-19; experiments/PARENT_SELECTIVE.md).
    //
    // The driven fixture measured a RUNAWAY balance (capture outruns release, b = +0.0754), and the
    // displacement route is closed: the harness asserts `length<=1` (inertia_fixture.h:95), i.e. a
    // constituent may move at most ONE cell per light frame, so no rule can simply push harder because
    // an island is bigger.  This brake acts where the size dependence is FREE: on a TRANSITION.
    //
    // Cohesion is the claim that a chief's shell holds its delegates.  The shell's radius is the model's
    // own contact range 2*RMAX -- the same length the encounter predicate uses (two wavefronts can meet
    // only while their centres are within 2*RMAX) and the one the fixtures state.  A delegate whose
    // torus distance to its chief exceeds that range is outside the shell cohesion can claim, so it is
    // released.  A small island keeps every member inside the shell and loses none; an island that
    // grows past the range begins shedding its outer members -- size-dependent loss with NO new
    // constant, reading only lcenters, parent and RMAX.  No seed partition, no ISLAND_SIZE, no L.
    //
    // Deliberately independent of the contact-free timer (SURFACE_ESCAPE_FSM), which needs W_USED = 243
    // light frames in the cubic census (~3.4 h) and therefore never fires in a feasible run; this one
    // fires on the first frame the geometry allows it.
    void applyShellRelease()
    {
      const int range = 2 * (int)RMAX;
      unsigned released = 0;
      int maxDist = 0;
      for (unsigned w = 0; w < W_USED; ++w)
      {
        const Cell& s = sourceAfter[w];
        if (s.kind != SourceKind::D) continue;
        const WIndex g = s.parent;
        if (g >= W_USED) continue;                       // orphan: no shell to fall out of
        if (sourceAfter[g].kind != SourceKind::K) continue;
        int d = 0;
        for (int k = 0; k < 3; ++k) { const int t = delta(w, g, k); d += (t < 0 ? -t : t); }
        if (d > maxDist) maxDist = d;
        if (d <= range) continue;                        // inside the shell cohesion holds
#ifdef SURFACE_ESCAPE_FSM
        framesWithoutContact[w] = 0;                     // keep the shared timer consistent when both are on
#endif
        sourceAfter[w].kind     = SourceKind::S;
        sourceAfter[w].parent   = NO_PARENT;
        sourceAfter[w].leader_w = NO_LEADER_W;
        ++released;
      }
      fprintf(stderr, "[shell] frame-edge: range=%d max_dist=%d releases=%u\n",
              range, maxDist, released);
    }
#endif

#ifdef PARENT_SELECTIVE_FSM
    // Frame-edge separation push for the parent-selective candidate: one antisymmetric unit
    // step per pair per light frame, along the axis of largest shortest-torus separation
    // (W-derived axis when coincident).  Pairs inside one island are never recorded by
    // encounter(), and the impulse is deferred through reloc[] exactly like the exclusion
    // push below, so applyMomentum consumes it once per light frame.  The push is skipped
    // again here if the pair has become one island during the frame (a merge wins).
    void resolveParentRepulsion()
    {
      for (const auto& [a, b] : parentRepel) {
        const Cell& sa = sourceAfter[a];
        const Cell& sb = sourceAfter[b];
        if (sa.ch != sb.ch) continue;                                       // equal charge only
        if (sa.kind != SourceKind::D || sb.kind != SourceKind::D) continue;  // delegates only
        const WIndex ca = islandChief(sa), cb = islandChief(sb);
        if (ca != NO_PARENT && ca == cb) continue;                          // same island: no push
        int sep[3];
        int best = 0, bestAbs = -1;
        for (int axis = 0; axis < 3; ++axis) {
          const int d = delta(a, b, axis);
          sep[axis] = d;
          const int ad = d < 0 ? -d : d;
          if (ad > bestAbs) { bestAbs = ad; best = axis; }
        }
        int axis, sgn;
        if (bestAbs > 0) { axis = best; sgn = sep[axis] > 0 ? -1 : 1; }
        else { axis = (int)(((uint64_t)a + b) % 3u); sgn = a < b ? -1 : 1; }
        sourceAfter[a].reloc[axis] += sgn;
        sourceAfter[b].reloc[axis] -= sgn;
      }
    }
#endif

#ifdef PARENT_FUSION_ABSORB
    // Frame-edge absorption for the completed rule set: every delegate whose parent is a
    // chief demoted by a K x K fusion this frame is re-pointed to the surviving chief, so the
    // fusion GROWS the survivor instead of orphaning a delegation.  Iterated to a fixed point
    // because a demoted chief may itself have been re-pointed in the same frame (chains).
    // Same standing as the other frame-edge bookkeeping: it reads and rewrites the membership
    // relation only -- no geometry, no L, no seed partition.
    void resolveFusionAbsorb()
    {
      if (fusionAbsorb.empty()) {
        fprintf(stderr, "[absorb] frame-edge: no K x K fusion recorded this frame\n");
        return;
      }
      unsigned repointed = 0;
      for (unsigned pass = 0; pass < W_USED; ++pass) {
        bool changed = false;
        for (const auto& [demoted, survivor] : fusionAbsorb) {
          for (WIndex w = 0; w < W_USED; ++w) {
            Cell& s = sourceAfter[w];
            if (s.kind != SourceKind::D) continue;
            if (s.parent != demoted) continue;
            s.parent = survivor;
            s.leader_w = survivor;
            changed = true;
            ++repointed;
          }
        }
        if (!changed) break;
      }
      fprintf(stderr, "[absorb] frame-edge: K x K encounters=%lld pairs=%zu delegates re-pointed=%u\n",
              pselKClashes, fusionAbsorb.size(), repointed);
      pselKClashes = 0;
    }
#endif

#ifdef PARENT_NOMINATION_REPAIR
    // Frame-edge repair for the completed rule set -- the piece that makes it self-consistent.
    // T1/T4 may name as a parent a source that is NOT a chief (the minimum-W address of two
    // S's, or the parent of a delegate).  In the REFERENCE path the T2 promotion cascade
    // repaired such pending nominations; with T2 disabled they stay pending forever --
    // measured at L=9: 1 chief, 242 delegates, 241 naming a non-K.  Here the named source is
    // PROMOTED instead, which is what T1/T4 intended ("two S meet -> one becomes K, the other
    // its D") and what T2 was doing globally.  One pass suffices: promoting a parent cannot
    // invalidate another nomination.  Reads the membership relation only -- no geometry, no L,
    // no seed partition.
    void resolveParentNominations()
    {
      std::vector<unsigned char> pending(W_USED, 0);
      for (WIndex w = 0; w < W_USED; ++w) {
        const Cell& s = sourceAfter[w];
        if (s.kind != SourceKind::D) continue;
        if (s.parent < W_USED) pending[s.parent] = 1;
      }
      unsigned promoted = 0;
      for (WIndex w = 0; w < W_USED; ++w) {
        if (!pending[w]) continue;
        Cell& s = sourceAfter[w];
        if (s.kind == SourceKind::K) continue;
        if (s.kind == SourceKind::P) continue;   // a pair half is not a chief
        s.kind = SourceKind::K;
        s.parent = NO_PARENT;
        s.leader_w = w;
        ++promoted;
      }
      fprintf(stderr, "[nomination] frame-edge: pending parents promoted=%u\n", promoted);
    }
#endif

#ifdef EXCLUSION_FSM
    // Frame-edge separation push for cross-family equal-charge pairs
    // recorded by the identity hard core.  One antisymmetric unit step per
    // pair per light frame, along the axis of largest shortest-torus
    // separation (W-derived axis when coincident).  Bound same-chief pairs
    // are never pushed.
    void resolveExclusionPush()
    {
      for (const auto& [a, b] : internalContacts) {
        if (a / 3u == b / 3u) continue;                 // same family
        const Cell& sa = sourceAfter[a];
        const Cell& sb = sourceAfter[b];
        if (sa.ch != sb.ch) continue;                   // equal charge only
        WIndex ca = islandChief(sa), cb = islandChief(sb);
        if (ca != NO_PARENT && ca == cb) continue;      // already one island
        int sep[3];
        int best = 0, bestAbs = -1;
        for (int axis = 0; axis < 3; ++axis) {
          const int d = delta(a, b, axis);
          sep[axis] = d;
          const int ad = d < 0 ? -d : d;
          if (ad > bestAbs) { bestAbs = ad; best = axis; }
        }
        int axis, sgn;
        if (bestAbs > 0) { axis = best; sgn = sep[axis] > 0 ? -1 : 1; }
        else { axis = (int)(((uint64_t)a + b) % 3u); sgn = a < b ? -1 : 1; }
        sourceAfter[a].reloc[axis] += sgn;
        sourceAfter[b].reloc[axis] -= sgn;
      }
    }
#endif

#ifdef ORPHAN_KICK_PER_WINDOW
#ifndef ORPHAN_KICK_CAP
#define ORPHAN_KICK_CAP 4
#endif
    // Per-tick payout of the accumulated engagement events: one kick per island
    // pair per tick, magnitude = min(events, ORPHAN_KICK_CAP), direction and
    // sign from the two islands' charges (or always attractive for the R1
    // graviton).  Centre of mass conserved (equal and opposite steps).
    void resolveRecruitHits()
    {
      if (recruitHits.empty()) return;
      std::sort(recruitHits.begin(), recruitHits.end());
      size_t i = 0;
      while (i < recruitHits.size())
      {
        size_t j = i;
        while (j < recruitHits.size() &&
               recruitHits[j][0] == recruitHits[i][0] &&
               recruitHits[j][1] == recruitHits[i][1]) ++j;
        const unsigned a = (unsigned)recruitHits[i][0];
        const unsigned b = (unsigned)recruitHits[i][1];
        const int sign = recruitHits[i][2];
        const int magnitude = (int)std::min<size_t>(j - i, ORPHAN_KICK_CAP);
        if (a < W_USED && b < W_USED && a != b &&
            sourceAfter[a].kind != SourceKind::P &&
            sourceAfter[b].kind != SourceKind::P)
        {
          int sep[3], best = 0, bestAbs = -1;
          for (int axis = 0; axis < 3; ++axis)
          {
            const int d = delta(a, b, axis);
            sep[axis] = d;
            const int ad = d < 0 ? -d : d;
            if (ad > bestAbs) { bestAbs = ad; best = axis; }
          }
          if (bestAbs > 1)
          {
            const int axis = best;
            const int sgn  = sep[axis] > 0 ? -1 : 1;
            sourceAfter[a].reloc[axis] += sign * sgn * magnitude;
            sourceAfter[b].reloc[axis] -= sign * sgn * magnitude;
            if (sign > 0) ++recruit_repel; else ++recruit_attract;
          }
        }
        i = j;
      }
      recruitHits.clear();
    }
#endif

    // Direct (per-tick, single-step) mediated kick - the default scheme.
    void applyRecruitKick(unsigned aw, unsigned bw, int pushSign)
    {
      int sep[3], best = 0, bestAbs = -1;
      for (int axis = 0; axis < 3; ++axis)
      {
        const int d = delta(aw, bw, axis);
        sep[axis] = d;
        const int ad = d < 0 ? -d : d;
        if (ad > bestAbs) { bestAbs = ad; best = axis; }
      }
      if (bestAbs <= 1) return;
      const int sgn = sep[best] > 0 ? -1 : 1;
      const auto& cA = lcenters[aw];
      const auto& cB = lcenters[bw];
      // Candidate (/D PAIR_SAME_OCTANT).  This is the default per-tick mediated kick, and it
      // is the mover that dominates the cascades: its step direction is the partner's offset
      // (moveOneStep / moveOneStepAway book the sign of shortestDelta), which cuts across both
      // layers when the two partners sit in different charge octants.  Restricting the kick to
      // partners that share the colour triplet keeps the step along the octant both layers
      // already own.  Measured with only the contact handler guarded, the cascade still showed
      // 0 of 46 displacements matching the octant (0/7/39/0), which is what located the mover
      // here.  Compiled off in the reference build.
#ifdef PAIR_SAME_OCTANT
      if (((sourceAfter[aw].ch ^ sourceAfter[bw].ch) & 7u) != 0u)
        return;
#endif
      if (pushSign > 0)
      {
        moveOneStepAway(sourceAfter[aw], cA, cB);
        moveOneStepAway(sourceAfter[bw], cB, cA);
      }
      else
      {
        moveOneStep(sourceAfter[aw], cA, cB);
        moveOneStep(sourceAfter[bw], cB, cA);
      }
      if (pushSign > 0) ++recruit_repel; else ++recruit_attract;
    }

  }

  // Unit step from the charge word: (+/-c2, +/-c1, +/-c0).  The three colour bits are the
  // three coordinate signs, so the sector bit w1 = c1 IS the y-sign (Orbis -y, Umbra +y)
  // and the chirality bit w0 = c0 IS the z-sign.  The k <-> 7-k colour-complement classes
  // therefore get exactly antipodal steps, which keeps the centre of mass of the eight
  // classes at the origin.  Shared by the charge-seeded dispersion and by the direction
  // tie-break of the homing producer, so that neither has to read the W address.
  inline void chargeStep(unsigned ch6, int& dx, int& dy, int& dz)
  {
    dx = (ch6 & 4u) ? +1 : -1;   // c2
    dy = (ch6 & 2u) ? +1 : -1;   // c1 == w1: Orbis -y, Umbra +y
    dz = (ch6 & 1u) ? +1 : -1;   // c0 == w0: chirality R -z, L +z
  }

#ifdef CHARGE_STEP_MAGNITUDE
  // ===========================================================================
  // Magnitude of the charge step (candidate, /D CHARGE_STEP_MAGNITUDE and
  // /D CHARGE_STEP_SUBBLOCK).  OFF in the reference build, and OFF by default
  // inside the dispersion too: with neither macro the step stays one cell.
  //
  // The six-bit word fixes the octant but carries no information about HOW FAR a bubble
  // steps, and -- because the seed's family map makes the word a function of island mod 8
  // (w1 = bit 1, w0 = bit 0, q = their xor, c2c1c0 = the low three bits) -- it carries no
  // information that tells the copies of one class apart either.  Two magnitudes are
  // therefore added, both read from the seed's own structure and neither from the W
  // address:
  //
  //   CHARGE_STEP_MAGNITUDE : mag = 2 for sig in {0,3}, 1 for sig in {1,2}, where
  //       sig = c2+c1+c0.  The colour complement k <-> 7-k sends sig -> 3-sig, so the
  //       antipodal partners keep the same magnitude and their steps stay exactly
  //       opposite: the centre of mass is unmoved by this rule.  The eight classes then
  //       separate at two different speeds instead of one.
  //
  //   CHARGE_STEP_SUBBLOCK : mag += bit 3 of the family index (island).  This is the only
  //       non-address way to split a class, and the bit is invariant under the exact
  //       pairing island <-> (island ^ 7) -- the map that flips the low three bits, i.e.
  //       turns a word into its colour complement -- so the antipodal partners still get
  //       equal magnitude.  (The pairing is exact only while island ^ 7 is inside the
  //       family count; beyond it one family of a pair is absent and the centre of mass
  //       picks up the population imbalance, which is measurable.)
  // ===========================================================================
  inline unsigned chargeStepMagnitude(unsigned ch6, unsigned family)
  {
    const unsigned sig = ((ch6 >> 2) & 1u) + ((ch6 >> 1) & 1u) + (ch6 & 1u);
    unsigned mag = (sig == 0u || sig == 3u) ? 2u : 1u;
#ifdef CHARGE_STEP_SUBBLOCK
    mag += (family >> 3) & 1u;
#endif
    return mag;
  }
#endif

#ifdef CHARGE_DISPERSION_FSM
  // ===========================================================================
  // Charge-seeded dispersion (candidate, /D CHARGE_DISPERSION_FSM).  OFF in the
  // reference build.
  //
  // Until a layer has an elected momentum direction (m == 0, i.e. the polarisation
  // election has not published one) the fabric resolves its initial superposition by
  // stepping each layer along the octant of its own colour triplet:
  //
  //     step = ( +1/-1 from c2 , +1/-1 from c1 , +1/-1 from c0 )
  //
  // The three colour bits are the three coordinate signs, so the sector bit w1 = c1 IS
  // the y-sign: Orbis (w1 = 0) steps -y and Umbra (w1 = 1) +y, while matter and
  // antimatter -- the k <-> 7-k colour-complement classes of the seed, four pairs -- get
  // exactly antipodal steps, so the centre of mass of the eight classes stays at the
  // origin.  The diversity therefore comes from the charge word alone: no W address, no
  // hash, no scan order (the stance elect() states for the polarisation tie-break) and no
  // random number.
  //
  // One unit step per layer per era, while m == 0 and the shell has left the centre
  // (effective_t(t) >= 1); the latch re-arms at the era edge, so a layer stops dispersing
  // as soon as a direction is elected for it -- m == 0 is the phase delimiter.
  // ===========================================================================
  static void chargeDispersionTick()
  {
    static std::vector<unsigned char> latched;
    if (latched.size() != W_USED) latched.assign(W_USED, 0);

    for (unsigned w = 0; w < W_USED; ++w)
    {
      const auto& p = lcenters[w];
      const Cell& src = getCell(lattice_curr, p[0], p[1], p[2], w);
      const unsigned r = effective_t(src.t);

      if (r == 0) { latched[w] = 0; continue; }        // era edge: re-arm the latch
      if (latched[w]) continue;                        // one step per layer per era
      if (src.m[0] || src.m[1] || src.m[2]) continue;  // inertia exists: phase closed
      if (r != 1) continue;                            // step as soon as the shell exists

      int dx, dy, dz;
      chargeStep(src.ch & 0x3Fu, dx, dy, dz);
#ifdef CHARGE_STEP_MAGNITUDE
      {
        const unsigned fam = (ISLAND_SIZE > 0u) ? (w / ISLAND_SIZE) : 0u;
        const unsigned mag = chargeStepMagnitude(src.ch & 0x3Fu, fam);
        dx *= (int)mag; dy *= (int)mag; dz *= (int)mag;
      }
#endif
      latched[w] = 1;
      bookImpulse(w, dx, dy, dz);
#ifdef S2B_TRACE
      ++s2bTraceChargeDispersion;
      tagWriter(w, 16u);   // charge dispersion
#endif
    }
  }
#endif

  void commitSourceTick()
  {
#ifdef S2B_TRACE
    ++s2bTraceCommitCalls;
#endif
    if (!lattice_draft.empty() && lattice_draft.front().k == 0) {
      resolveInternalContacts();
#ifdef EXCLUSION_FSM
      resolveExclusionPush();
#endif
#ifdef PARENT_SELECTIVE_FSM
      resolveParentRepulsion();
#ifdef PARENT_FUSION_ABSORB
      resolveFusionAbsorb();
#endif
#ifdef PARENT_NOMINATION_REPAIR
      resolveParentNominations();
#endif
#endif
#ifdef SURFACE_ESCAPE_FSM
      // Candidate (item 4): runs after this frame's contacts have been resolved
      // and before the sources are committed, so a release reaches the draft.
      applySurfaceEscape();
#endif
#ifdef PARENT_SHELL_RELEASE_FSM
      // Same standing as the escape rule above: the frame's transitions are settled and the release
      // must reach the draft.  Independent of the contact-free timer -- this is the geometric brake.
      applyShellRelease();
#endif

    }
#ifdef ORPHAN_KICK_PER_WINDOW
    resolveRecruitHits();      // per tick: pay out the engagement windows
#endif
    for (unsigned w = 0; w < W_USED; ++w) {
      const auto& p = lcenters[w];
      Cell& dst = getCell(lattice_draft, p[0], p[1], p[2], w);
      const Cell& s = sourceAfter[w];
#ifdef S2B_TRACE
      // A pending impulse in the draft is about to be REPLACED (not accumulated) by the
      // source-level value: count what arrives and what this assignment erases.
      if (s.reloc[0] || s.reloc[1] || s.reloc[2]) ++s2bTraceCommitPending;
      else if (dst.reloc[0] || dst.reloc[1] || dst.reloc[2]) ++s2bTraceCommitWiped;
#ifdef S2B_DUMP_PENDING
      // Identification probe (/D S2B_DUMP_PENDING): print each pending impulse with the layer index
      // and its charge word.  Eleven sites in the model write reloc and the counters leave seven of
      // them without a hook; the PATTERN of these lines (address-derived axis/sign, diagonal triple,
      // paired a/b, ...) names the writer without touching any of them.
      if (s.reloc[0] || s.reloc[1] || s.reloc[2])
        printf("[pending] w=%u reloc=(%d,%d,%d) ch=%03x kind=%d mask=0x%02x\n",
               w, s.reloc[0], s.reloc[1], s.reloc[2], s.ch & 0x3Fu, (int)s.kind,
               (w < s2bTraceWriterMask.size() ? s2bTraceWriterMask[w] : 0u));
#endif
#endif
      dst.w = w; dst.ch = s.ch; dst.a = s.a; dst.leader_w = s.leader_w;
      dst.kind = s.kind; dst.parent = s.parent; dst.spin_target = s.spin_target;
      dst.pair_idx = s.pair_idx; dst.pair_count = s.pair_count; dst.bB = s.bB;
      for (int k=0;k<3;++k) { dst.m[k] = s.m[k]; dst.reloc[k] = s.reloc[k]; }
#ifdef S2B_TRACE
      s2bTraceRelocSumAtCommit += (unsigned long long)(std::abs(dst.reloc[0]) +
                                                       std::abs(dst.reloc[1]) +
                                                       std::abs(dst.reloc[2]));
#endif
      if (s.t != sourceBefore[w].t) { dst.t = s.t; dst.f = s.f; }
      else if (body(s) || isBoundPropeller(s)
#ifdef ORPHAN_MEDIATOR_PROPAGATES
               // Opt-in (distance-law study): let a FREE pair advance its clock
               // so the photon propagates.  WARNING: applyMomentum then CONSUMES
               // the pair at t == RMAX (pair_count--, released as singletons),
               // so the mediator disappears after one cycle and the mediated
               // repulsion is lost - use a large pair_count or re-emission
               // before enabling this.  See design note section 9.
               || (s.kind == SourceKind::P && s.a == W_USED)
#endif
              ) {
        dst.t = (s.t + (dst.k == 0 ? 1u : 0u)) % (2 * RMAX);
        dst.f = effective_t(dst.t);
      }
    }

    // Drain this tick's impulse queue into the draft, where applyMomentum() (called right
    // after this) reads the source-centre reloc.  Done AFTER the loop above on purpose: that
    // loop ASSIGNS dst.reloc from the source-level copy, so an earlier drain would be
    // overwritten.  See the NOTE on g_pendingImpulses in simulation.h.
#ifdef CHARGE_DISPERSION_FSM
    chargeDispersionTick();   // candidate: books the charge-seeded step, then drained below
#endif
    if (!g_pendingImpulses.empty())
    {
#ifdef S2B_TRACE
      {
        // Net (signed) impulse per layer, and how many layers end up with a non-zero net:
        // the encounter books opposite steps for the two bubbles of a pair (each walks
        // toward the other), so a book-keeping counter can be large while the net is zero.
        std::vector<std::array<long long,3>> net(W_USED, {0,0,0});
        for (const ImpulseBooking& b : g_pendingImpulses)
          if (b.w < W_USED)
          { net[b.w][0] += b.dx; net[b.w][1] += b.dy; net[b.w][2] += b.dz; }
        unsigned nonzero = 0; long long maxAbs = 0;
        for (unsigned w = 0; w < W_USED; ++w)
        {
          const long long a = std::llabs(net[w][0]) + std::llabs(net[w][1]) + std::llabs(net[w][2]);
          if (a) ++nonzero;
          if (a > maxAbs) maxAbs = a;
        }
        s2bTraceNetLayers += nonzero;
        if ((unsigned long long)maxAbs > s2bTraceNetMax) s2bTraceNetMax = (unsigned long long)maxAbs;
      }
      s2bTraceImpulseDrained += (unsigned long long)g_pendingImpulses.size();
#endif
      for (const ImpulseBooking& b : g_pendingImpulses)
      {
        if (b.w >= W_USED) continue;
        const auto& p = lcenters[b.w];
        Cell& dst = getCell(lattice_draft, p[0], p[1], p[2], b.w);
        dst.reloc[0] += b.dx;
        dst.reloc[1] += b.dy;
        dst.reloc[2] += b.dz;
#ifdef S2B_TRACE
        tagWriter(b.w, 64u);   // impulse-queue drain
#endif
#ifdef S2B_TRACE
        s2bTraceDrainWriteback += (unsigned long long)(std::abs(dst.reloc[0]) +
                                                       std::abs(dst.reloc[1]) +
                                                       std::abs(dst.reloc[2]));
#endif
      }
      g_pendingImpulses.clear();
    }
  }

  /**
   * Handles active wavefronts from distinct W addresses at one voxel,
   * including equal-affinity internal mechanical contacts. Named encounter():
   * nothing is averaged here; the old name survives only in the conv_*
   * diagnostics counters so the campaign logs/scripts stay valid.
   *
   * @curr the current lattice
   * @draft the draft lattice
   * @partner the mirrored lattice
   */
  bool encounter(Cell& curr, Cell &draft, Cell &partner)
  {
    if (!curr.active || !partner.active || curr.r<=0 || partner.r<=0)
      return false;

    // A source does not interact with its own W address.
    if (curr.x[3] == partner.x[3])
    {
      ++enc_self;
      return false;
    }
#ifdef MULTIFREQ_RAY_FSM
    // Multi-frequency mechanics: the first action of Encounter for any
    // source that carries a non-trivial pair_count is to derive the
    // effective tick from the population and interrogate the detection cell
    // of its pure-climb ray along m (freq_hit), before the sieve gate.
    {
      const WIndex mfWa = curr.x[3], mfWb = partner.x[3];
      if (mfWa < W_USED && mfFreqBearing(sourceBefore[mfWa]))
        mfComputeRayDetect(sourceBefore[mfWa]);
      if (mfWb < W_USED && mfWb != mfWa && mfFreqBearing(sourceBefore[mfWb]))
        mfComputeRayDetect(sourceBefore[mfWb]);
    }
#endif
#ifdef SURFACE_ESCAPE_FSM
    // Candidate (item 4): any contact between two sources of the same charge
    // word is a shell overlap for the escape rule -- chief-delegate contacts
    // included, which the reference `internal` predicate does not cover.  The
    // bitmap is separate, so no cohesion step is added to these pairs.
    {
      const WIndex wa = curr.x[3], wb = partner.x[3];
      if (wa < W_USED && wb < W_USED &&
          surfaceContactSeen.size() == (size_t)W_USED * W_USED &&
          sourceBefore[wa].ch == sourceBefore[wb].ch) {
        const WIndex a = std::min(wa, wb), b = std::max(wa, wb);
        surfaceContactSeen[(size_t)a * W_USED + b] = 1;
      }
    }
#endif

    ++enc_calls;

    // Source state is stored in the source-center cell of each W-layer.
    Cell& currSrc  = sourceCenterCurr(curr);
    Cell& partnerSrc = sourceCenterCurr(partner);
    Cell& currDraft  = sourceCenterDraft(curr);
    Cell& partnerDraft = sourceCenterDraft(partner);

    const auto& currCenter  = sourceCenter(curr);
    const auto& partnerCenter = sourceCenter(partner);

#ifdef ORPHAN_GUIDANCE_FSM
    // ==================================================================
    // Experimental P2 (recruit / relay): a FREE photon half (P source with no
    // leader) that coincides with an island wavefront illuminates that
    // island's orphan shell when one of the contact site's neighbours in the
    // island layer sits on the thin shell (isOrphanShell: r == f + 1).  The
    // shell is one cell thick, so the event stays rare.  On recruitment the
    // photon half is reissued at the contact point (the relay step); the
    // island itself is NOT touched here.
    // ==================================================================
    {
      // A mediator half is any P source that is NOT part of the engaged
      // island: a free photon/graviton (no leader) or the OTHER island's
      // dress (leader = that island).  Using the dress leader as island B is
      // exact, unlike the nearest-source fallback used for free mediators.
      auto mediator = [](const Cell& c)
      {
        return c.kind == SourceKind::P;
      };
      const Cell* isl = nullptr;
      const Cell* pho = nullptr;
      WIndex mLead = NO_LEADER_W;      // the mediator's leader (bound dress)
      if (currSrc.kind != SourceKind::P && mediator(partnerSrc))
      { isl = &curr; pho = &partner; }
      else if (partnerSrc.kind != SourceKind::P && mediator(currSrc))
      { isl = &partner; pho = &curr; }

      // Defensive: every sourceAfter[] index below must be a real layer.
      if (isl && pho && (isl->x[3] >= W_USED || pho->x[3] >= W_USED))
      { isl = nullptr; pho = nullptr; }

      if (isl && pho && isl->x[3] != pho->x[3])
      {
        const WIndex eChief = islandChief(sourceAfter[isl->x[3]]);
        mLead = sourceAfter[pho->x[3]].leader_w;
        const bool otherIsland = (mLead == NO_LEADER_W) ||
                                 (eChief == NO_PARENT) || (mLead != eChief);
        if (!otherIsland) { isl = nullptr; pho = nullptr; }
      }

      if (isl && pho && isl->x[3] != pho->x[3])
      {
        static const int off[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0},
                                       {0,-1,0}, {0,0,1}, {0,0,-1} };
        const int cx = (int)isl->x[0], cy = (int)isl->x[1], cz = (int)isl->x[2];
        // getCell() neither wraps nor bounds-checks, so the periodic wrap must
        // be applied here: a contact site on a lattice face would otherwise
        // read out of the lattice and segfault (root cause of the `dressed`
        // crash - a read-only gate build reproduced it).
        auto wrapC = [](int v, int m) { v %= m; return v < 0 ? v + m : v; };
        bool onShell = false;
        for (const auto& o : off)
        {
          const Cell& nb = getCell(lattice_curr,
                                   wrapC(cx + o[0], (int)ELX),
                                   wrapC(cy + o[1], (int)ELY),
                                   wrapC(cz + o[2], (int)ELZ),
                                   (int)isl->x[3]);
          if (isOrphanShell(nb)) { onShell = true; break; }
        }
        if (onShell)
        {
          ++recruit_events;
          // Mediator state (shared by the relay and the shuttle below).
          const Cell& medSrc = sourceAfter[pho->x[3]];
          const unsigned mw0 = pho->x[3];
          const unsigned mw1 = (medSrc.pair_idx < W_USED &&
                                medSrc.pair_idx != mw0)
                                   ? (unsigned)medSrc.pair_idx : W_USED;
          auto freeAtRest = [](const Cell& c)
          {
            return c.leader_w == NO_LEADER_W &&
                   c.m[0] == 0 && c.m[1] == 0 && c.m[2] == 0;
          };
          const bool medFree = freeAtRest(medSrc) &&
                               (mw1 >= W_USED || freeAtRest(sourceAfter[mw1]));
          // Relay only for a WHOLLY free mediator pair at rest (a photon in the
          // vacuum): the pair is seated at the contact point WITHOUT resetting
          // its clock, so it keeps propagating (resetting t would re-pin the
          // photon to phase 0 on every engagement).
          {
#ifndef ORPHAN_NO_RELAY
#if defined(ORPHAN_RELAY_KEEPT)
            // Variant B (opt-in): seat the mediator at the contact point but
            // KEEP its clock, so it keeps propagating.
            if (medFree) reseatAtContact(sourceCenterDraft(*pho), *pho);
#elif defined(ORPHAN_RELAY_NOOP)
            // Variant C (opt-in): the relay does not touch the mediator at all.
            if (medFree) chargesMarkInteraction((unsigned)pho->x[3]);
#else
            // Default (validated): reissue the pair half at the contact point,
            // which also resets its clock - this pins the photon's phase and is
            // what keeps the mediated repulsion alive (the pair is never
            // consumed).
            if (medFree) reemitAtContact(sourceCenterDraft(*pho), *pho);
#endif
#else
            (void)medFree;
#endif
          }

          // ---------------------------------------------------------------
          // Sign and the dual impulse (design section 4, rules 5-7).  The
          // sign is decided by the TWO ISLANDS' charge words alone; the
          // mediator is only the vehicle, so the orientation of its two
          // halves cannot matter:
          //   R1 graviton pair (fully complementary words, charge-neutral)
          //       -> ALWAYS attractive;
          //   R2 photon pair (same sector w1, complementary q/w0/color)
          //       -> equal island charge repels, opposite charge attracts.
          // The impulse is ONE antisymmetric face step per island pair per
          // tick (centre of mass conserved), the same one-light-step
          // granularity as the existing electroweak branches, applied
          // through sourceAfter[] (the sanctioned remote-impulse channel).
          // ---------------------------------------------------------------
#ifndef ORPHAN_GATE_ONLY
          const unsigned aw = isl->x[3];
          const unsigned pw0 = pho->x[3];
          // The pair link is authoritative in the source centre, not on the
          // wavefront cell.
          const unsigned pw1 = (sourceAfter[pw0].pair_idx < W_USED &&
                                sourceAfter[pw0].pair_idx != pw0)
                                 ? (unsigned)sourceAfter[pw0].pair_idx : W_USED;
          const unsigned char aCh = sourceAfter[aw].ch;     // island A charge
          const unsigned char q0  = sourceAfter[pw0].ch;    // mediator words
          const unsigned char q1  = (pw1 < W_USED) ? sourceAfter[pw1].ch : 0x00;
          const bool graviton = (pw1 < W_USED) &&
                                (((q0 ^ q1) & 0x3Fu) == 0x3Fu);

          // The other island: the mediator's own leader when it is a bound
          // dress; otherwise the nearest unaffiliated source (free mediator).
          unsigned bw = W_USED;
          unsigned char bCh = 0;
          if (mLead != NO_LEADER_W && mLead < W_USED)
          {
            bw = mLead;
            bCh = sourceAfter[bw].ch;
          }
          else
          {
          int bestD = -1;
          for (unsigned w = 0; w < W_USED; ++w)
          {
            if (w == aw) continue;
            const Cell& sc = sourceAfter[w];
            if (sc.kind == SourceKind::P) continue;
            const WIndex chief = islandChief(sc);
            if (chief != NO_PARENT && chief == islandChief(sourceAfter[aw]))
              continue;                       // same island
            int d = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
              const int dd = delta(aw, w, axis);
              d += dd * dd;
            }
            if (bestD < 0 || d < bestD) { bestD = d; bw = w; bCh = sc.ch; }
          }
          }

          if (bw < W_USED)
          {
            // The mediated kick acts on ISLANDS (any non-mediator source).  Two
            // schemes: the default one light-step per pair per tick
            // (applyRecruitKick, immediate), or - under ORPHAN_KICK_PER_WINDOW -
            // record every engagement event and let resolveRecruitHits() pay out
            // a flux-proportional kick at the end of the tick.
            const bool islands = sourceAfter[aw].kind != SourceKind::P &&
                                 sourceAfter[bw].kind != SourceKind::P;
            const int pushSign = graviton ? -1 : (aCh == bCh ? +1 : -1);
#ifdef ORPHAN_KICK_PER_WINDOW
            if (islands)
              recruitHits.push_back({ (int)aw, (int)bw, pushSign });
#else
            const std::array<unsigned, 2> key{ aw, bw };
            if (islands &&
                std::find(recruitApplied.begin(), recruitApplied.end(), key) ==
                recruitApplied.end())
            {
              recruitApplied.push_back(key);
              applyRecruitKick(aw, bw, pushSign);
            }
#endif
          }
#ifdef ORPHAN_RELAY_SHUTTLE
          // Relay-shuttle (manuscript 794: "the relay advances along the flux
          // toward the other body"): send the mediator one light-step toward
          // the OTHER island, clock preserved, so it shuttles between the two
          // fields and keeps engaging.  The transit time scales with the
          // separation - the hoped-for distance law.
          if (medFree && bw < W_USED)
            reseatStepToward(sourceCenterDraft(*pho),
                             lcenters[pho->x[3]], lcenters[bw]);
#endif
#endif
        }
      }
    }
#endif

#ifdef ORPHAN_GUIDANCE_FSM
#ifndef ORPHAN_NO_ANNIH
    // ==================================================================
    // Experimental P2 (annihilation; design section 4 rule 5, decision D):
    // two BODY representatives (K or D) of DIFFERENT islands that carry
    // OPPOSITE charges and overlap at the SAME site annihilate: both are
    // demoted to S and reissued at the contact point with the orphan affinity
    // (a = W).  Distinct parents only, so a member never annihilates its own
    // island, and there is no distant annihilation - the overlap must be here.
    // ==================================================================
    {
      auto antimatter = [](unsigned char ch)
      {
        const unsigned color = ch & 0x07u;
        const unsigned ones = (color & 1u) + ((color >> 1) & 1u) +
                              ((color >> 2) & 1u);
        return ones >= 2u;      // same convention as the charges census
      };
      const bool sameSite = (curr.x[0] == partner.x[0] &&
                             curr.x[1] == partner.x[1] &&
                             curr.x[2] == partner.x[2]);
      const WIndex ca = islandChief(currSrc);
      const WIndex cb = islandChief(partnerSrc);
      // Distinct islands: different chiefs once both exist, otherwise different
      // charge FAMILIES (w/3), which is the island grouping before the chief
      // election.  This is the reachable reading of "different parents".
      const bool diffParents =
          (ca != NO_PARENT && cb != NO_PARENT) ? (ca != cb)
                                               : (islandOf(curr.x[3]) != islandOf(partner.x[3]));
      // DELEGATES only.  Root cause found (see the design note section 8):
      // demoting a CHIEF (K) to S leaves its island's members pointing at a
      // non-chief anchor, and the identity machinery then crashes a few frames
      // later (measured: ann = 18 then segfault in the anchor x anchor probe;
      // the D x D probe runs clean with ann = 354).  Restricting the branch to
      // D x D is also what the manuscript states ("only D x D overlap");
      // K-member annihilation needs island-dissolution semantics and stays open.
      const bool delegates = currSrc.kind == SourceKind::D &&
                             partnerSrc.kind == SourceKind::D;
      if (sameSite && delegates && diffParents &&
          antimatter(currSrc.ch) != antimatter(partnerSrc.ch))
      {
        ++annihilations;
        chargesMarkAnnihilation();
        for (Cell* d : { &currDraft, &partnerDraft })
        {
          d->kind        = SourceKind::S;
          d->parent      = NO_PARENT;
          d->leader_w    = NO_LEADER_W;
          d->a           = W_USED;
          d->pair_idx    = NO_PAIR;
          d->pair_count  = 0;
          d->spin_target = 0;
        }
        reemitAtContact(currDraft, curr);
        reemitAtContact(partnerDraft, partner);
        return false;
      }
    }
#endif
#endif

#ifdef EM_FORCE_PREREQ
    // WP4.2 spike hook: force the EM_FIRST_FSM prerequisite at this contact so
    // the reorder's guard (`curr.s2B && (pB||sB...)`) is evaluated as if the
    // polarization broadcast had lit pB and the sieve gate were open.  This
    // isolates whether the reorder is mechanically correct from whether the
    // (dormant) broadcast can supply the flags.  Experimental; compiled only
    // under this macro and only meaningful together with EM_FIRST_FSM.
    curr.pB = true; curr.s2B = true; partner.pB = true;
#endif

#ifdef HOMB_PRODUCER_FSM
    // ==================================================================
    // Candidate /D HOMB_PRODUCER_FSM (WP8).  The CPU path has the CONSUMER of
    // the homing flag -- the SLOT II "Homing using homB" block further down
    // this file -- but it never had a PRODUCER: outside this block homB is
    // written only as false, so c[] stays zero and the RELOC stage has nothing
    // to move.  The producers exist in the archived CUDA kernel
    // (src/cuda/cuda_automaton.cu, dev_encounter4/6/7) and are ported here.
    //
    // They turn a symmetric two-bubble superposition into a DIRECTED
    // (carrier, homer) pair: the carrier records its position in c[] and sets
    // cB, the homer sets homB and cB, so the SLOT II machinery can home on it.
    // The direction is set by the internal in-phase/quadrature bits pB/sB and,
    // for equal phases, by the immutable W address -- both deterministic; no
    // RNG enters.  This is the symmetry breaker the island census reported
    // missing ("never separates spatially"; SEED_ASYMMETRY.md and
    // ISLAND_CENSUS.md), and it needs live pB/sB: with pB = sB = false every
    // condition below is false and the block is inert.  A build that also
    // breaks the election fixed point (POLAR_BOOTSTRAP_ADDRESS) is therefore a
    // prerequisite.  OFF in the reference build.
    //
    // The CUDA kernel mutates the current cell's draft; here the pair's
    // source-centre drafts are used, which is how this function already
    // mutates the partner's state (cf. adoptLeader(partnerDraft, ...)).
    // ==================================================================
    {
      // D3: the grouping is the model's own one -- islandOf() over the runtime
      // ISLAND_SIZE -- and the manuscript's dispersion condition is the SECTOR (w1), the
      // middle colour bit.  The hardcoded divide by 3 assumed ISLAND_SIZE == 3, i.e. L == 9;
      // it belonged to the abandoned L/3 grouping, and it grouped by triples of addresses
      // at every other lattice size.
      const bool diffFamily = islandOf(curr.x[3]) != islandOf(partner.x[3]);
      const bool diffSector = ((curr.ch >> 5) & 1u) != ((partner.ch >> 5) & 1u);
      // The manuscript places the inter-sector interaction at the mid-expansion
      // instant t = RMAX/2, which the single-winner producer below keeps.
      const unsigned mid = (unsigned)(RMAX / 2);
      const bool free       = !currDraft.cB && !partnerDraft.cB;

      auto makeDirected = [&](const bool currCarries)
      {
        Cell& carrierD = currCarries ? currDraft    : partnerDraft;
        Cell& homerD   = currCarries ? partnerDraft : currDraft;
        const Cell& carrierS = currCarries ? curr : partner;
        carrierD.c[0] = carrierS.x[0];
        carrierD.c[1] = carrierS.x[1];
        carrierD.c[2] = carrierS.x[2];
        carrierD.cB   = 1;
        homerD.homB   = 1;
        homerD.cB     = 1;
        ++homb_events;
      };

#ifdef FAMILY_SELECTIVE_FSM
      // ==============================================================
      // FAMILY_SELECTIVE_FSM (WP8 option 2): make the directionality
      // FAMILY-SELECTIVE instead of global.
      //
      // The census expects islands of the form 1K + nD with n = L/3 - 1, i.e.
      // exactly the L/3 W copies of ONE family (family = w / 3), so a family has
      // to act as a single unit.  The direction is therefore decided by an
      // INTRA-family pair and written in the only directional encoding (the
      // relative displacement L + (own - anchor) % L), while the cross-family
      // producers are excluded: producer (2)'s W-address tie-break and the
      // absolute-coordinate encodings of (1)/(3) are what dragged constituents
      // of DIFFERENT families together -- measured as the 20-journey census
      // overshooting to 86-94 centres with max_families 3-4 instead of settling
      // at 81.
      //
      // The consumer half is in simulation.cpp applyMomentum(): the copies of a
      // family share ONE decision per light frame and step together, so a family
      // keeps a single centre.  Defining FAMILY_RIGID_FSM additionally sends
      // every copy of a NON-co-located family to its family chief, so an island
      // re-coheres instead of splitting (no freeze: the rigid rule is
      // self-correcting).  OFF in the reference build.
      // ==============================================================
      if (free && !diffFamily && (curr.pB != partner.pB) && (curr.sB || partner.sB) &&
          currSrc.a != W_USED && partnerSrc.a != W_USED)
      {
        // The carrier/homer split of the CUDA branch is kept -- the CARRIER holds
        // the target (c[] + cB) and the HOMER carries homB + cB, which is what the
        // SLOT II homing stage needs to relay the field at all -- but it is
        // restricted to the family, and the homer additionally receives the
        // DIRECTIONAL encoding (the signed relative displacement) instead of the
        // carrier's raw coordinate, so the step it walks is toward its anchor
        // rather than an undirected drift.
        makeDirected(curr.pB);
        const Cell& walkerS = curr.pB ? partner : curr;   // out of phase: walks home
        Cell&       walkerD = curr.pB ? partnerDraft : currDraft;
        walkerD.c[0] = ELX + (unsigned)(walkerS.x[0] - (curr.pB ? curr.x[0] : partner.x[0])) % ELX;
        walkerD.c[1] = ELY + (unsigned)(walkerS.x[1] - (curr.pB ? curr.x[1] : partner.x[1])) % ELY;
        walkerD.c[2] = ELZ + (unsigned)(walkerS.x[2] - (curr.pB ? curr.x[2] : partner.x[2])) % ELZ;
        walkerD.cB   = 1;
      }
      // (3-family) CUDA dev_encounter4, restricted to the family: ONE winner per
      // FAMILY per turnaround, on the family's anchor layer (the first of its L/3
      // copies).  This is the only producer whose write the SLOT II homing stage
      // can actually SEE, because it writes `draft.homB` on a CELL: a write to a
      // source-centre draft is not visible to that cell's neighbours until the
      // frame ends, and homB is cleared at the end of every frame -- measured:
      // with producers (1)/(2) alone the homing block reported homb_seen = 0
      // while homb_events was non-zero.
      // NB: the guard accepts ANY live directional bit (pB || sB), not sB alone:
      // the reconstruction can return a component that is EXACTLY zero, and with
      // the magnitude convention (pB = pol_u != 0, sB = pol_v != 0) a single
      // sB-gated rule then dies on the trajectories where pol_v == 0 -- measured:
      // the rigid variant's probe trajectory landed on pol = (4,0) and the whole
      // channel went dark (homb_seen = 0, c_at_center = 0), the same class of
      // failure as the phase-quadrant lottery that POLAR_MAGNITUDE_FSM removed.
      if (curr.active && (curr.pB || curr.sB) && isIslandChief(curr.x[3]))
      {
        static std::vector<unsigned char> latchedHombFam;
        const unsigned nFam = (ISLAND_COUNT > 0u) ? ISLAND_COUNT : 1u;
        if (latchedHombFam.size() != nFam) latchedHombFam.assign(nFam, 0);
        const unsigned fam = islandOf(curr.x[3]);
        if (effective_t(curr.t) == (unsigned)(RMAX / 2))
        {
          if (!latchedHombFam[fam])
          {
            latchedHombFam[fam] = 1;
            draft.homB = 1;
            draft.cB   = 1;
            ++homb_events;
          }
        }
        else
        {
          latchedHombFam[fam] = 0;
        }
      }
#else
      // (4) CUDA dev_encounter7 "same affinity" branch (lines 686-700) -- the
      //     RELATIVE-DISPLACEMENT encoding, and the only DIRECTIONAL one.  Within
      //     one island the in-phase cell (pB true) is the anchor whose raw position
      //     is relayed, while the out-of-phase cell records the signed distance to
      //     the anchor as ELX + (own - anchor) % ELX.  The consumer
      //     (interaction.cpp relocate()) tests `c[i] > 0` and decrements toward
      //     zero, so this encoding walks a cell HOME; an ABSOLUTE coordinate (the
      //     encodings above) makes every cell drift instead, which in a symmetric
      //     island balances out -- measured: 30752 migrations, zero net
      //     displacement.  The CUDA sets only c[] here; cB is set too because the
      //     CPU relays the field through SLOT IV on the cB gate, where the CUDA
      //     relayed per voxel.
      if (free && currSrc.a == partnerSrc.a && (curr.pB != partner.pB) &&
          currSrc.a != W_USED && partnerSrc.a != W_USED)
      {
        const Cell& walkerS = curr.pB ? partner : curr;   // out of phase: walks home
        Cell&       walkerD = curr.pB ? partnerDraft : currDraft;
        walkerD.c[0] = ELX + (unsigned)(walkerS.x[0] - (curr.pB ? curr.x[0] : partner.x[0])) % ELX;
        walkerD.c[1] = ELY + (unsigned)(walkerS.x[1] - (curr.pB ? curr.x[1] : partner.x[1])) % ELY;
        walkerD.c[2] = ELZ + (unsigned)(walkerS.x[2] - (curr.pB ? curr.x[2] : partner.x[2])) % ELZ;
        walkerD.cB   = 1;
        ++homb_events;
      }
      // (1) CUDA dev_encounter6/7: the in-phase sign decides who carries and
      //     who homes (the cell with pB false becomes the homer).  Requires a
      //     live quadrature bit on one side, and bound (non-orphan) sources.
      //     No phase guard: the CPU encounter is source-level, so the guard
      //     that matters is the contact condition already enforced above
      //     (both cells active, r > 0) -- the CUDA's f == t / t == RMAX/2
      //     guards belonged to its per-voxel rule cascade.
      else if (free && (curr.pB != partner.pB) && (curr.sB || partner.sB) &&
          currSrc.a != W_USED && partnerSrc.a != W_USED)
        makeDirected(curr.pB);
      // (2) CUDA dev_encounter7 affinity branch: equal phase, different
      //     families -> the W-address order breaks the symmetry.
      else if (free && diffSector && (curr.pB == partner.pB) &&
               (curr.pB || curr.sB) &&
               currSrc.a != W_USED && partnerSrc.a != W_USED)
      {
        // Direction from the CHARGE WORD, not from the W address: elect() refuses
        // address/hash/scan-order tie-breaks for the polarisation axis, and the W address is
        // a serial number rather than a species.  The bubble whose colour octant lies
        // "first" carries; complementary words have antipodal octants.
        int ax, ay, az, bx, by, bz;
        chargeStep(curr.ch & 0x3Fu, ax, ay, az);
        chargeStep(partner.ch & 0x3Fu, bx, by, bz);
        makeDirected((ax + ay + az) >= (bx + by + bz));
      }
      // (3) CUDA dev_encounter4: one winner per layer per turnaround, on the
      //     layer-0 source -- the "first directional datum" of SEED_ASYMMETRY.
      if (curr.active && curr.sB && isIslandChief(curr.x[3]))
      {
        static std::vector<unsigned char> latchedHomb;
        if (latchedHomb.size() != W_USED) latchedHomb.assign(W_USED, 0);
        if (effective_t(curr.t) == (unsigned)(RMAX / 2))
        {
          if (!latchedHomb[curr.x[3]])
          {
            latchedHomb[curr.x[3]] = 1;
            draft.homB = 1;
            draft.cB   = 1;
            ++homb_events;
          }
        }
        else
        {
          latchedHomb[curr.x[3]] = 0;
        }
      }
#endif
    }
#endif

#ifdef EM_FIRST_FSM
    // ==============================================================
    // Experimental /D EM_FIRST_FSM: decide the electroweak channel BEFORE
    // the identity merge for equal-charge contacts that carry live pB/sB
    // and pass the s2B gate.  In the reference order the identity path
    // (chiefContact / K-K / D-D, and the EXCLUSION gate) preempts these
    // branches, so two equal-charge islands always merge before any EM
    // response can act (PBSB_ISLANDS.md).  This block mirrors the tail EM
    // semantics for the equal-charge subset (collapse vs adiabatic; S/S
    // same-Q repel; K/K repel; D/D cross-tribe repel; S attaches to a
    // K/D chief) and is compiled only under this macro.
    // ==============================================================
    const bool emGate =
#ifdef EM_NOS2B_FSM
        // Candidate (Route B): drop the probabilistic sieve requirement from the
        // EM reorder, so the channel depends only on the live pB/sB flags.  The
        // empirical window (PBSB_ISLANDS.md "Why the window closes") shows the
        // sieve gate P ~= u/S is the only thing that closes the repulsion window;
        // this macro tests the reorder without it.  Macro-guarded; OFF by default.
        true;
#else
        curr.s2B;
#endif
#ifdef MULTIFREQ_RAY_FSM
    const bool eCurrPB = mfGatedFlag(currSrc, curr.pB);
    const bool eCurrSB = mfGatedFlag(currSrc, curr.sB);
    const bool ePartPB = mfGatedFlag(partnerSrc, partner.pB);
    const bool ePartSB = mfGatedFlag(partnerSrc, partner.sB);
#else
    const bool eCurrPB = curr.pB, eCurrSB = curr.sB;
    const bool ePartPB = partner.pB, ePartSB = partner.sB;
#endif
    if (currSrc.ch == partnerSrc.ch && emGate &&
        (eCurrPB || ePartPB || eCurrSB || ePartSB))
    {
      const bool electricCollapse = eCurrPB && ePartPB;
      const bool magneticCollapse = eCurrSB && ePartSB;
      const bool collapse          = electricCollapse || magneticCollapse;

      if (collapse)
      {
        ++enc_collapse;
        draft.kB = true;            // collapse flag: inward reissue
        draft.cB = true;
      }
      else
      {
        ++enc_adiah;
        WIndex minLeader = dominantLeader(currSrc, partnerSrc);
        adoptLeader(currDraft, minLeader);
        adoptLeader(partnerDraft, minLeader);
        std::swap(currDraft.t, partnerDraft.t);
        moveOneStep(currDraft, currCenter, partnerCenter);
        moveOneStep(partnerDraft, partnerCenter, currCenter);
        return false;
      }

      if (currSrc.kind == SourceKind::K && partnerSrc.kind == SourceKind::K)
      {
        ++enc_repel;
        moveOneStepAway(currDraft, currCenter, partnerCenter);
        return false;
      }

      if (currSrc.kind == SourceKind::S && partnerSrc.kind == SourceKind::S)
      {
        if (currSrc.Q() == partnerSrc.Q())
        {
          ++enc_repel;               // equal field sign -> repel one step
          moveOneStepAway(currDraft, currCenter, partnerCenter);
        }
        else
        {
          WIndex leader = dominantLeader(currSrc, partnerSrc);
          currDraft.kind  = SourceKind::D;
          partnerDraft.kind = SourceKind::D;
          currDraft.parent  = leader;
          partnerDraft.parent = leader;
          adoptLeader(currDraft, leader);
          adoptLeader(partnerDraft, leader);
        }
        return false;
      }

      if (currSrc.kind == SourceKind::D && partnerSrc.kind == SourceKind::D)
      {
        if (currSrc.parent != partnerSrc.parent)
        {
          ++enc_repel;
          moveOneStepAway(currDraft, currCenter, partnerCenter);
          currDraft.reloc[0] += partnerSrc.m[0];
          currDraft.reloc[1] += partnerSrc.m[1];
          currDraft.reloc[2] += partnerSrc.m[2];
        }
        return false;
      }

      // S x K / S x D and mirrors: the S attaches as a D of the chief.
      if ((currSrc.kind == SourceKind::S &&
           (partnerSrc.kind == SourceKind::K || partnerSrc.kind == SourceKind::D)) ||
          (partnerSrc.kind == SourceKind::S &&
           (currSrc.kind == SourceKind::K || currSrc.kind == SourceKind::D)))
      {
        Cell& sDraft  = (currSrc.kind == SourceKind::S) ? currDraft : partnerDraft;
        Cell& dSrc    = (currSrc.kind == SourceKind::S) ? partnerSrc : currSrc;
        Cell& dDraft  = (currSrc.kind == SourceKind::S) ? partnerDraft : currDraft;
        const auto& sCenter = (currSrc.kind == SourceKind::S) ? currCenter : partnerCenter;
        const auto& dCenter = (currSrc.kind == SourceKind::S) ? partnerCenter : currCenter;
        WIndex leader = (dSrc.leader_w == NO_LEADER_W ? dSrc.parent : dSrc.leader_w);
        if (leader == NO_LEADER_W) leader = dSrc.w;
        sDraft.kind = SourceKind::D;
        sDraft.parent = dSrc.parent == NO_PARENT ? leader : dSrc.parent;
        adoptLeader(sDraft, leader);
        moveOneStep(sDraft, sCenter, dCenter);
        moveOneStep(dDraft, dCenter, sCenter);
        return false;
      }

      return false;
    }
#endif

    // Role transitions precede internal-contact early returns. Only the
    // current draft is updated; reciprocal encounters update the other side.
#ifdef EXCLUSION_FSM
    // Pauli-like identity hard core (candidate, ported from the colour FSM):
    // equal-charge sources from DIFFERENT seed families never share identity
    // and are not allowed to merge through chiefContact/promotion/clash.
    // The pair is recorded (deduplicated per light frame) for the separation
    // push applied at the frame edge; everything else is skipped.
    if (currSrc.ch == partnerSrc.ch && currSrc.w / 3u != partnerSrc.w / 3u) {
      const WIndex a = std::min(currSrc.w, partnerSrc.w);
      const WIndex b = std::max(currSrc.w, partnerSrc.w);
      const size_t index = (size_t)a * W_USED + b;
      if (!contactSeen[index]) {
        contactSeen[index] = 1;
        internalContacts.emplace_back(a, b);
      }
      return false;
    }
#endif
    chiefContact(currSrc,partnerSrc,currDraft);
#ifdef CASCADE_LOG
    // A2 probe (read-only): did this encounter change the current source's role?  Recorded with the
    // tick, both addresses, both kinds, the charge word and the parent before/after.  OFF in the
    // reference, so the preprocessor removes it there.
    if (currDraft.kind != currSrc.kind || currDraft.parent != currSrc.parent)
      cascadeRecord(pulse_tick, currSrc.w, partnerSrc.w,
                    (unsigned char)currSrc.kind, (unsigned char)currDraft.kind,
                    (unsigned char)currSrc.ch, currSrc.parent, currDraft.parent);
#endif
#ifdef PARENT_FUSION_ABSORB
    // The K x K clash demoted the larger-address chief (chiefContact ran above); record
    // {demoted, survivor} so the frame edge can absorb the demoted chief's delegation.
    if (currSrc.kind == SourceKind::K && partnerSrc.kind == SourceKind::K &&
        currSrc.ch == partnerSrc.ch) {
      ++pselKClashes;
      if (currSrc.w > partnerSrc.w) fusionAbsorb.emplace_back(currSrc.w, partnerSrc.w);
    }
#endif
    if(currSrc.kind==SourceKind::K && partnerSrc.kind==SourceKind::K && currSrc.ch==partnerSrc.ch)
      return false; // Preserve the clash transition through the remaining branches.
#ifdef PARENT_SELECTIVE_FSM
    // Candidate (2026-09-19): two delegates of the same charge but of DIFFERENT dynamical
    // islands repel by one step per light frame.  The island identity is read from the
    // emergent parent linkage (islandChief), so the test contains no family index, no
    // ISLAND_SIZE and no L.  Same-island delegate pairs are deliberately NOT recorded: with T2
    // disabled in chief_transition.h nothing ejects them, and they then hit the equal-charge
    // D x D early return inside `encounter()` -- identity preserved, no step, no record.  They do
    // NOT reach the cohesion loop (corrected 19 Sep 2026; see experiments/WORK_PLAN.md).
    if (currSrc.kind == SourceKind::D && partnerSrc.kind == SourceKind::D &&
        currSrc.ch == partnerSrc.ch) {
      const WIndex ca = islandChief(currSrc), cb = islandChief(partnerSrc);
      if (ca != NO_PARENT && cb != NO_PARENT && ca != cb) {
        const WIndex a = std::min(currSrc.w, partnerSrc.w);
        const WIndex b = std::max(currSrc.w, partnerSrc.w);
        if (contactSeen.size() == (size_t)W_USED * W_USED) {
          const size_t index = (size_t)a * W_USED + b;
          if (!contactSeen[index]) {
            contactSeen[index] = 1;
            parentRepel.emplace_back(a, b);
          }
        }
        return false;
      }
    }
#endif
    if(currSrc.kind==SourceKind::D && partnerSrc.kind==SourceKind::D && currSrc.ch==partnerSrc.ch)
      return false; // The older generic branches must not overwrite this transition.

    const bool internal =
      (body(currDraft) && body(partnerDraft) && shareChief(currDraft, partnerDraft)) ||
      (sameAffinity(currSrc, partnerSrc) &&
      ((isBoundPropeller(currSrc) && body(partnerDraft)) ||
       (isBoundPropeller(partnerSrc) && body(currDraft)) ||
       (currSrc.kind == SourceKind::P && partnerSrc.kind == SourceKind::P)));
    if (internal) {
      const WIndex a = std::min(currSrc.w, partnerSrc.w);
      const WIndex b = std::max(currSrc.w, partnerSrc.w);
      const size_t index = (size_t)a * W_USED + b;
      if (!contactSeen[index]) {
        contactSeen[index] = 1;
        internalContacts.emplace_back(a, b);
      }
      return false;
    }

#ifdef ISLAND_SEED_EXPERIMENT
    // Reduced experiment: retain contact-driven election and internal
    // cohesion above; omit inter-family scattering and pair formation.
    return false;
#endif
    // The electroweak sieve does not gate same-affinity mechanical contacts.
    if (!curr.s2B) return false;
    ++enc_s2b;

    // A free pair may join a contacted island. Acquiring affinity is not
    // an immediate kick, and receiving a kick never turns a K/D into P.
    if ((currSrc.kind == SourceKind::P && body(partnerSrc)) ||
        (partnerSrc.kind == SourceKind::P && body(currSrc))) {
      const Cell& p = currSrc.kind == SourceKind::P ? currSrc : partnerSrc;
      const Cell& target = currSrc.kind == SourceKind::P ? partnerSrc : currSrc;
      if (p.a == W_USED && p.pair_idx < W_USED &&
          sourceBefore[p.pair_idx].pair_idx == p.w) {
        for (unsigned w : {p.w, p.pair_idx}) {
          sourceAfter[w].a = target.a;
          sourceAfter[w].leader_w = target.leader_w;
          sourceAfter[w].parent = target.leader_w;
        }
      }
      return false;
    }

    bool samePos = (curr.x[0] == partner.x[0] &&
                    curr.x[1] == partner.x[1] &&
                    curr.x[2] == partner.x[2]);
    bool sameT   = (currSrc.t == partnerSrc.t);

    // ---------------------------------------------------------------
    // Pair formation (photon-like P sources).
    // Two overlapping bubbles with the same wavefront time and complementary
    // charges can form a pair. The pair is "dressing" if both bubbles already
    // share the same leader; otherwise it is a free photon.
    // ---------------------------------------------------------------
    if (samePos && sameT && currSrc.kind == SourceKind::S &&
        partnerSrc.kind == SourceKind::S && canFormPair(currSrc, partnerSrc))
    {
      ++enc_pair;
#ifdef PAIR_WORD_LOG
      // Menu item 3, closed by measurement: which charge words actually reach
      // the pair branch.  Prints one line per formation so a long canonical run
      // can be checked against the reachability table of CHARGE_SPECTRUM.md
      // (canonically only R3 0x00/0x00 and R6 0x2A/0x2A, 0x2E/0x2E).
      printf("[pairword] w=%u ch=0x%02X | w=%u ch=0x%02X\n",
             (unsigned)currSrc.w, (unsigned)currSrc.ch,
             (unsigned)partnerSrc.w, (unsigned)partnerSrc.ch);
      fflush(stdout);
#endif
      bool dressing = (currSrc.leader_w != NO_LEADER_W &&
                       currSrc.leader_w == partnerSrc.leader_w);
      WIndex newLeader = dressing ? currSrc.leader_w : NO_LEADER_W;
      unsigned newA    = dressing ? (unsigned)newLeader : W_USED;
      WIndex parent    = dressing ? currSrc.leader_w : NO_PARENT;

      chargesMarkPair();          // idea B: count this registered formation

      // Frequency bookkeeping.  A fresh pair always starts at count 1: this
      // branch requires BOTH sources in state S (the guard above), so the
      // former "1 + pair_count + pair_count" accumulation and the P+P
      // alreadyPaired check that used to live here were unreachable dead
      // code (dead since 381c1b49 fixed the branch to S+S).  Dynamic stack
      // growth is the candidate rule PAIR_STACK_ABSORB_FSM below; without
      // it, a multifrequency population exists only where a harness seeds
      // it (alpha_probe "photonp").
      uint8_t newCount = 1;
#ifdef PAIR_STACK_ABSORB_FSM
      // Idempotence guard (candidate only): encounter is evaluated from both
      // W directions of a site.  If the first direction already formed this
      // pair (and possibly absorbed stacks into it), the reverse evaluation
      // must not run again, or it would overwrite the absorbed count.
      // Reference build: no guard, byte-identical to the pre-candidate tree.
      if (sourceAfter[currSrc.w].kind == SourceKind::P &&
          sourceAfter[currSrc.w].pair_idx == partnerSrc.w)
        return false;
      // Candidate absorption (multi-frequency mechanics): a free S+S
      // formation superposed on identical free stacks registers those
      // stacks' population on the new pair and releases the absorbed halves
      // as singletons, so a stack of n identical pairs grows by one unit
      // per encounter.  With newCount > 1 the blob branch below revives as
      // originally intended ("a single fresh pair is not yet a group").
      newCount = absorbCoLocatedStacks(currSrc, partnerSrc);
      if (newCount > 1)
        ++enc_absorb;
#endif

      currDraft.kind  = SourceKind::P;
      partnerDraft.kind = SourceKind::P;
      currDraft.pair_idx   = partnerSrc.w;
      partnerDraft.pair_idx = currSrc.w;
      currDraft.pair_count = newCount;
      partnerDraft.pair_count = newCount;
      currDraft.leader_w  = newLeader;
      partnerDraft.leader_w = newLeader;
      currDraft.a  = newA;
      partnerDraft.a = newA;
      currDraft.parent  = parent;
      partnerDraft.parent = parent;

      // Move both source centers to the contact point and reset their clocks.
      reemitAtContact(currDraft, curr);
      reemitAtContact(partnerDraft, partner);

      // -----------------------------------------------------------------
      // Blob formation (manuscript "Blob" and "Superposing bubbles"): a
      // group of superposed equal-status pairs aggregates into a blob when
      // the charge geometry is R2 (gluon/photon, see canFormBlob) and both
      // halves carry the same (active) polarization bits pB/sB and the same
      // bB flag.  The common affinity is already enforced by newA above.
      // Specialized pairs (neutrino/antineutrino, up quark, graviton,
      // propeller) never blob; a single fresh pair (newCount == 1) is not
      // yet a group.
      // -----------------------------------------------------------------
      if (newCount > 1 && canFormBlob(currSrc, partnerSrc) &&
          currSrc.pB == partnerSrc.pB &&
          currSrc.sB == partnerSrc.sB &&
          currSrc.bB == partnerSrc.bB)
      {
        currDraft.bB  = true;
        partnerDraft.bB = true;
        chargesMarkBlob();
      }

      return false;
    }

    // pB triggers the electric channel, sB the magnetic channel.
    // MULTIFREQ_RAY_FSM: a frequency-bearing side opens the channel only
    // after its freq_hit bit is set (gated read, fresh at this use site so
    // the reference semantics of curr/partner are untouched).
#ifdef MULTIFREQ_RAY_FSM
    const bool gCurrPB = mfGatedFlag(currSrc, curr.pB);
    const bool gCurrSB = mfGatedFlag(currSrc, curr.sB);
    const bool gPartPB = mfGatedFlag(partnerSrc, partner.pB);
    const bool gPartSB = mfGatedFlag(partnerSrc, partner.sB);
#else
    const bool gCurrPB = curr.pB, gCurrSB = curr.sB;
    const bool gPartPB = partner.pB, gPartSB = partner.sB;
#endif
    bool electricContact   = gCurrPB || gPartPB;
    bool magneticContact   = gCurrSB || gPartSB;
    bool electricCollapse  = gCurrPB && gPartPB;
    bool magneticCollapse  = gCurrSB && gPartSB;
    bool collapse          = electricCollapse || magneticCollapse;
#ifdef PAIR_SAME_OCTANT
    // Candidate (/D PAIR_SAME_OCTANT), shared by every mover of this handler.  Each of the
    // displacements below is directed by the partner's offset (moveOneStep books the sign of
    // shortestDelta toward the partner, moveOneStepAway the sign away from it).  When the two
    // partners sit in different charge octants that offset cuts across both layers, so the
    // step matches the layer's own octant in one or two signs and never all three -- measured
    // on the reference transport (README, "The axis was already charge-correlated"): the era-1
    // cascade of L=7 gave 0/15/79/0 over 94 displacements and the era-2 cascade 80 of 96 with
    // no matching sign, which is how the sector split of the dispersal is washed out.
    //
    // Restricting every mover to partners that share the colour triplet keeps each walk along
    // the octant both layers already own, so the ordering the dispersal established is
    // reinforced instead of scrambled.  The charge word is a property of the layer, so no W
    // address, hash, scan order or RNG enters.  Compiled off in the reference build.
    const bool sameOct = ((currSrc.ch ^ partnerSrc.ch) & 7u) == 0u;
#endif

    if (!electricContact && !magneticContact)
      return false;

    if (collapse)
    {
      ++enc_collapse;
      // Collapse flag propagates inward and triggers reissue.
      draft.kB = true;
      draft.cB = true;
    }
    else
    {
      ++enc_adiah;
      // Adiabatic: no collapse, but the two sources exchange
      // leader identity, affinity and light clock, then drift one step
      // toward each other.
      WIndex minLeader = dominantLeader(currSrc, partnerSrc);
      adoptLeader(currDraft, minLeader);
      adoptLeader(partnerDraft, minLeader);
      std::swap(currDraft.t, partnerDraft.t);
      // Candidate (/D PAIR_SAME_OCTANT).  OFF in the reference build.
      //
      // The drift below is a displacement, and its direction is the partner's offset
      // (moveOneStep books the sign of shortestDelta toward the partner).  When the two
      // partners sit in different charge octants that offset cuts across both, so the step
      // matches the layer's own octant in one or two signs and never all three -- measured
      // on the reference transport (README, "The axis was already charge-correlated"):
      // frame 6 of L=7 gave 0/15/79/0 and the era-2 cascade 80 of 96 with no matching sign,
      // which is how the sector split of the dispersal is washed out.
      //
      // Restricting the drift to partners that share the colour triplet keeps the walk along
      // the octant both layers already own, so the ordering the dispersal established is
      // reinforced instead of scrambled.  The charge word is a property of the layer, so no
      // address, hash, scan order or RNG enters.  Other movers of this handler (the K x K
      // repulsion and the S x K absorption step) are left untouched by this first test.
#ifdef PAIR_SAME_OCTANT
      if (((currSrc.ch ^ partnerSrc.ch) & 7u) == 0u)
#endif
      {
        moveOneStep(currDraft, currCenter, partnerCenter);
        moveOneStep(partnerDraft, partnerCenter, currCenter);
      }
      return false;
    }

    // 1. K x K
    if (currSrc.kind == SourceKind::K && partnerSrc.kind == SourceKind::K)
    {
      ++enc_repel;
#ifdef PAIR_SAME_OCTANT
      if (sameOct)
#endif
      moveOneStepAway(currDraft, currCenter, partnerCenter);
      return false;
    }

    // 2. K x S (current = K, partner = S): K does not move; the S becomes a
    //    D delegate of K when it is its turn to be the current cell.
    //    The S-side is handled below.

    // 3. S x K (current = S, partner = K)
    if (currSrc.kind == SourceKind::S && partnerSrc.kind == SourceKind::K)
    {
      currDraft.kind = SourceKind::D;
      currDraft.parent = partnerSrc.w;
      adoptLeader(currDraft, partnerSrc.leader_w == NO_LEADER_W ? partnerSrc.w : partnerSrc.leader_w);
      // S vector direction relative to K is approximated as outward for now.
      currDraft.spin_target = 1;
#ifdef PAIR_SAME_OCTANT
      if (sameOct)
#endif
      moveOneStep(currDraft, currCenter, partnerCenter);
      return false;
    }

    // 4. S x S
    if (currSrc.kind == SourceKind::S && partnerSrc.kind == SourceKind::S)
    {
      if (currSrc.Q() == partnerSrc.Q())
      {
        // Same field sign: repel one light-step.
        ++enc_repel;
#ifdef PAIR_SAME_OCTANT
        if (sameOct)
#endif
        moveOneStepAway(currDraft, currCenter, partnerCenter);
      }
      else
      {
        // Opposite field sign: both become D delegates of the dominant leader.
        WIndex leader = dominantLeader(currSrc, partnerSrc);
        currDraft.kind  = SourceKind::D;
        partnerDraft.kind = SourceKind::D;
        currDraft.parent  = leader;
        partnerDraft.parent = leader;
        adoptLeader(currDraft, leader);
        adoptLeader(partnerDraft, leader);
      }
      return false;
    }

    // 5. S x D / D x S
    if ((currSrc.kind == SourceKind::S && partnerSrc.kind == SourceKind::D) ||
        (currSrc.kind == SourceKind::D && partnerSrc.kind == SourceKind::S))
    {
      Cell& sSrcDraft  = (currSrc.kind == SourceKind::S ? currDraft : partnerDraft);
      Cell& dSrc       = (currSrc.kind == SourceKind::S ? partnerSrc : currSrc);
      Cell& dSrcDraft  = (currSrc.kind == SourceKind::S ? partnerDraft : currDraft);
      const auto& sCenter = (currSrc.kind == SourceKind::S ? currCenter : partnerCenter);
      const auto& dCenter = (currSrc.kind == SourceKind::S ? partnerCenter : currCenter);

      // S becomes a D delegate of the K parent of the D.
      WIndex leader = (dSrc.leader_w == NO_LEADER_W ? dSrc.parent : dSrc.leader_w);
      if (leader == NO_LEADER_W) leader = dSrc.w;
      sSrcDraft.kind = SourceKind::D;
      sSrcDraft.parent = dSrc.parent == NO_PARENT ? leader : dSrc.parent;
      adoptLeader(sSrcDraft, leader);

      // Both reemit and move one light-step toward each other.
#ifdef PAIR_SAME_OCTANT
      if (sameOct)
      {
#endif
        moveOneStep(sSrcDraft, sCenter, dCenter);
        moveOneStep(dSrcDraft, dCenter, sCenter);
#ifdef PAIR_SAME_OCTANT
      }
#endif
      return false;
    }

    // 6. D x D
    if (currSrc.kind == SourceKind::D && partnerSrc.kind == SourceKind::D)
    {
      if (currSrc.parent != partnerSrc.parent)
      {
        // Different tribes: reemit, repel one light-step, exchange momentum.
        ++enc_repel;
#ifdef PAIR_OWN_AXIS_EXCHANGE
        // Candidate (/D PAIR_OWN_AXIS_EXCHANGE) -- design (b).  OFF in the reference build.
        //
        // Measured on the reference transport (README, "(a) Same-octant pairing"): the D x D
        // cross-tribe momentum exchange is the WHOLE transport (guarding it leaves moved = 0 in
        // every frame), and it transfers the PARTNER's m, which is parallel to the partner's
        // octant -- a ladder pair's two classes usually differ in one colour bit, which is
        // exactly the measured "two of three signs" signature of the cascade.
        //
        // Here the reaction keeps the per-axis magnitude the exchange would have delivered but
        // takes its DIRECTION from the receiver's own charge octant (c2,c1,c0 -> x,y,z signs,
        // the map the dispersal uses).  So the interaction still thrusts every layer it
        // contacts, and every thrust runs along that layer's own axis: the transport stays
        // alive AND charge-aligned.  No address, hash, scan order or RNG enters.
        //
        // The geometric repulsion is kept only for same-octant partners, where "away from the
        // partner" already lies on the shared axis; across octants the thrust replaces it.
        if (((currSrc.ch ^ partnerSrc.ch) & 7u) == 0u)
          moveOneStepAway(currDraft, currCenter, partnerCenter);
        {
          // The octant must come from the RECEIVER's own word.  `currSrc` is the source cell the
          // encounter passes in, and in this branch the pair is evaluated from both W directions
          // (the rotated partner lattice supplies one half), so its word is not always the
          // receiver's -- which is how a thrust meant to be own-axis could come out off-octant
          // (measured: the off-side flight steps, README "Multi-era run").  The draft cell is the
          // receiver: the commit assigns `dst.ch = s.ch` per layer.
          const unsigned ch6 = currDraft.ch & 0x3Fu;
          const int ox = (ch6 & 4u) ? +1 : -1;
          const int oy = (ch6 & 2u) ? +1 : -1;
          const int oz = (ch6 & 1u) ? +1 : -1;
          const int mx = partnerSrc.m[0] < 0 ? -partnerSrc.m[0] : partnerSrc.m[0];
          const int my = partnerSrc.m[1] < 0 ? -partnerSrc.m[1] : partnerSrc.m[1];
          const int mz = partnerSrc.m[2] < 0 ? -partnerSrc.m[2] : partnerSrc.m[2];
          currDraft.reloc[0] += ox * mx;
          currDraft.reloc[1] += oy * my;
          currDraft.reloc[2] += oz * mz;
#ifdef S2B_TRACE
          tagWriter((unsigned)currDraft.x[3], 32u);   // own-axis exchange thrust
#endif
        }
#else
#ifdef PAIR_SAME_OCTANT
        if (sameOct)
#endif
        moveOneStepAway(currDraft, currCenter, partnerCenter);
        // Momentum exchange via inertia path: add partner's m into reloc.
        // m itself is immutable here; applyMomentum only consumes reloc.
        //
        // Candidate (/D PAIR_SAME_OCTANT): the exchange transfers the PARTNER's m, which is
        // parallel to the partner's charge octant -- so when the two layers sit in different
        // octants the layer is displaced along an axis that is not its own.  That is exactly
        // the signature measured on the reference transport: the cascade's steps matched the
        // layer's own octant in one or two of the three signs and never all three (L=7 era-1
        // cascade: 0/15/79/0 over 94 displacements; the two classes of a ladder pair usually
        // differ in one colour bit, hence "two matches").  Restricting the transfer to
        // same-octant partners keeps every displacement on the layer's own axis.
#ifdef PAIR_SAME_OCTANT
        if (sameOct)
        {
#endif
          currDraft.reloc[0] += partnerSrc.m[0];
          currDraft.reloc[1] += partnerSrc.m[1];
          currDraft.reloc[2] += partnerSrc.m[2];
#ifdef PAIR_SAME_OCTANT
        }
#endif
#endif
      }
      else
      {
        // Same tribe: outer delegate imposes spin_target on inner one.
        // (Tangential/radial orbital motion is left as a refinement.)
        currDraft.spin_target = partnerSrc.spin_target;
        // Enforce a common leader identity.
        WIndex leader = dominantLeader(currSrc, partnerSrc);
        adoptLeader(currDraft, leader);
      }
      return false;
    }

    // P x K/D transport and free-pair affiliation were handled above.
    // A raw P/S contact does not confer the internal propeller role.
    return false;
  }

  void diffuse(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up)
  {
	  /****** SLOT I ******/
	  if (curr.k < SLOT1)
	  {
	    /*--- Orphan propagation ---*/
	    if ((north.a == W_USED && curr.r2 >= north.r2) ||
	        (west.a  == W_USED && curr.r2 >= west.r2)  ||
	        (down.a  == W_USED && curr.r2 >= down.r2)  ||
	        (south.a == W_USED && curr.r2 >= south.r2) ||
	        (east.a  == W_USED && curr.r2 >= east.r2)  ||
	        (up.a    == W_USED && curr.r2 >= up.r2))
	    {
	      draft.a = W_USED;
	      draft.leader_w = NO_LEADER_W;
	    }
	  }
	  /****** SLOT II ******/
    if (curr.k < SLOT2)
    {
      /*--- Orphan propagation ---*/
      if ((north.a == W_USED && curr.r2 >= north.r2) ||
          (west.a  == W_USED && curr.r2 >= west.r2)  ||
          (down.a  == W_USED && curr.r2 >= down.r2)  ||
          (south.a == W_USED && curr.r2 >= south.r2) ||
          (east.a  == W_USED && curr.r2 >= east.r2)  ||
          (up.a    == W_USED && curr.r2 >= up.r2))
      {
        draft.a = W_USED;
        draft.leader_w = NO_LEADER_W;
      }
      /*--- Homing using homB ---*/
      if (curr.active)
      {
#ifdef HOMB_PRODUCER_FSM
        // WP8 probe: did the pre-existing consumer actually find a homer?
        const bool sawHomer =
            north.homB || south.homB || east.homB || west.homB || up.homB || down.homB;
#endif
        if (north.homB) { draft.c[0] = (north.c[0] + 1) % ELX; curr.sB = !draft.homB; }
        else if (west.homB)  { draft.c[1] = (west.c[1] + 1) % ELY; curr.sB = !draft.homB; }
        else if (down.homB)  { draft.c[2] = (down.c[2] + 1) % ELZ; curr.sB = !draft.homB; }
        else if (south.homB) { draft.c[1] = (south.c[1] + 1) % ELY; curr.sB = !draft.homB; }
        else if (east.homB)  { draft.c[0] = (east.c[0] + 1) % ELX; curr.sB = !draft.homB; }
        else if (up.homB)    { draft.c[2] = (up.c[2] + 1) % ELZ; curr.sB = !draft.homB; }
#ifdef HOMB_PRODUCER_FSM
        if (sawHomer) ++homb_seen;
#endif
      }
    }
    /****** SLOT III ******/
    else if (curr.k < SLOT3)
    {
      if (!ZERO(north.c))
      {
        draft.c[0] = north.c[0];
        draft.c[1] = north.c[1];
        draft.c[2] = north.c[2];
        if (north.kB) draft.kB = north.kB;
      }
      if (!ZERO(west.c))
      {
        draft.c[0] = west.c[0];
        draft.c[1] = west.c[1];
        draft.c[2] = west.c[2];
        if (west.kB) draft.kB = west.kB;
      }
      if (!ZERO(down.c))
      {
        draft.c[0] = down.c[0];
        draft.c[1] = down.c[1];
        draft.c[2] = down.c[2];
        if (down.kB) draft.kB = down.kB;
      }
      // f is the local triangular breathing phase f = effective_t(t)
      // (manuscript Sect. "The light frame"); it is not diffused.
      // Diffuse CB toward center (r2=0)
      if (!curr.cB)
      {
        if (north.cB && north.r2 > curr.r2)
        {
          draft.cB = true;
          if (north.a != W_USED) {
            draft.a = north.a;
            draft.leader_w = (WIndex)north.a; }
        }
        else if (south.cB && south.r2 > curr.r2)
        {
          draft.cB = true;
          if (south.a != W_USED) {
            draft.a = south.a;
            draft.leader_w = (WIndex)south.a; }
        }
        else if (east.cB && east.r2 > curr.r2)
        {
          draft.cB = true;
          if (east.a != W_USED) {
            draft.a = east.a;
            draft.leader_w = (WIndex)east.a; }
        }
        else if (west.cB && west.r2 > curr.r2)
        {
          draft.cB = true;
          if (west.a != W_USED) {
            draft.a = west.a;
            draft.leader_w = (WIndex)west.a; }
        }
        else if (down.cB && down.r2 > curr.r2)
        {
          draft.cB = true;
          if (down.a != W_USED) {
            draft.a = down.a;
            draft.leader_w = (WIndex)down.a; }
        }
        else if (up.cB && up.r2 > curr.r2)
        {
          draft.cB = true;
          if (up.a != W_USED) {
            draft.a = up.a;
            draft.leader_w = (WIndex)up.a; }
        }
      }
    }
    /****** SLOT IV ******/
    else if (curr.k < SLOT4)
    {
      if (forward.kB && forward.a == curr.a)
      {
        int delta_x = (curr.x[0] - forward.x[0] + ELX) % ELX;
        int delta_y = (curr.x[1] - forward.x[1] + ELY) % ELY;
        int delta_z = (curr.x[2] - forward.x[2] + ELZ) % ELZ;

        draft.c[0] = (forward.c[0] + delta_x) % ELX;
        draft.c[1] = (forward.c[1] + delta_y) % ELY;
        draft.c[2] = (forward.c[2] + delta_z) % ELZ;
        draft.kB = forward.kB;
        draft.cB = forward.cB;
      }
      // f is the local triangular breathing phase; it is not diffused.
    }
    /****** SLOT V ******/
    else if (curr.k < SLOT5)
    {
      if (curr.a == W_USED)
      {
        if (curr.r2 < curr.t * curr.t)
        {
      	  draft.a = curr.x[3];
        }
      }
    }
  }

  /**
   * Grid relocation.
   */
  void relocate(Cell& curr, Cell &draft, Cell &north, Cell &west, Cell &down)
  {
    // Save the 3D address
    unsigned x, y, z;
    x = curr.x[0];
    y = curr.x[1];
    z = curr.x[2];
#ifdef HOMB_PRODUCER_FSM
    // WP8 probe: c[] drives a TOPOLOGICAL relocation -- `draft = north` slides the
    // state pattern through the lattice, keeping the address -- and THAT is the
    // archived CUDA kernel's transport.  MEAN_V measures the other machine
    // (source-centre transport via reloc[] in applyMomentum) and cannot see it.
    auto mig = [&]() { ++reloc_cells; };
#else
    auto mig = []() {};
#endif
    /****** SLOT VI ******/
    if (curr.k < SLOT6)
    {
      if (north.c[0] > 0)
      {
        draft = north;
        draft.c[0]--;
        mig();
      }
    }
    /****** SLOT VII ******/
    else if (curr.k < SLOT7)
    {
      if (west.c[1] > 0)
      {
        draft = west;
        draft.c[1]--;
        mig();
      }
    }
    /****** SLOT VIII ******/
    else if (curr.k < SLOT8)
    {
      if (down.c[2] > 0)
      {
        draft = down;
        draft.c[2]--;
        mig();
      }
    }
    // Recover 3D address
    draft.x[0] = x;
    draft.x[1] = y;
    draft.x[2] = z;
  }

  /**
   * Prepares new wavefront.
   *
   * @curr the current lattice
   * @draft the draft lattice
   * @partner the mirrored lattice
   */
  void reissue(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up)
  {
      // Reset propagation status
      draft.kB = false;
      draft.homB = false;
      draft.bB = false;
      // Propagate normal affinity outward, overwriting normal or orphan
      if (curr.active)
      {
          if (north.r2 > curr.r2)
          {
              // Copy a from inner to outer cell
              draft.a = north.a;
              draft.leader_w = (north.a == W_USED ? NO_LEADER_W : (WIndex)north.a);
          }
          if (south.r2 > curr.r2)
          {
              draft.a = south.a;
              draft.leader_w = (south.a == W_USED ? NO_LEADER_W : (WIndex)south.a);
          }
          if (east.r2 > curr.r2)
          {
              draft.a = east.a;
              draft.leader_w = (east.a == W_USED ? NO_LEADER_W : (WIndex)east.a);
          }
          if (west.r2 > curr.r2)
          {
              draft.a = west.a;
              draft.leader_w = (west.a == W_USED ? NO_LEADER_W : (WIndex)west.a);
          }
          if (up.r2 > curr.r2)
          {
              draft.a = up.a;
              draft.leader_w = (up.a == W_USED ? NO_LEADER_W : (WIndex)up.a);
          }
          if (down.r2 > curr.r2)
          {
              draft.a = down.a;
              draft.leader_w = (down.a == W_USED ? NO_LEADER_W : (WIndex)down.a);
          }
      }
      if (curr.cB)
      {
        // Consume cB
        draft.cB = false;
        if (curr.a != W_USED && curr.r2 < 4)
        {
          draft.t = 0;
#ifdef S2B_TRACE
          ++s2bTraceCBResets;
#endif
        }
      }
  }

  void flood(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up)
  {
	if (curr.a != W_USED)
    {
      draft.t = min({ north.t, south.t, east.t, west.t, down.t, up.t });
#ifdef S2B_TRACE
      // The flood operator pulls every non-virgin cell's clock down to the minimum of
      // its six neighbours: it is the synchronising force, and the carrier of any reset.
      if (draft.t != curr.t)                   ++s2bTraceFloodPulls;
      if (draft.t == 0 && curr.t != 0)         ++s2bTraceFloodResets;
#endif
    }
  }
}
