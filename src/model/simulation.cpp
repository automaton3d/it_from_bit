/*
 * simulation.cpp
 * Implements the main functionality of the FSM.
 * (comments in american english)
 */

#include <thread>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include "model/simulation.h"
#include "model/chief_transition.h"
#include "model/polarization.h"
#include "config.h"

#ifdef USE_CUDA
extern void cudaSimulationStepWrapper();
extern bool isCudaEnabled();
extern void updatePartnerOnGPU();
#endif

namespace automaton
{
  using namespace std;

  // Grid constants
  unsigned EL;
  // Per-axis edges.  In the default cubic mode ELX == ELY == ELZ == EL; the
  // rectangular-tube allocator (tryAllocateTube) will set them separately as
  // soon as the per-axis indexing/wrap/distance slices are in place.
  unsigned ELX = 0, ELY = 0, ELZ = 0;
  unsigned W_DIM;
  unsigned W_USED;
  std::vector<ImpulseBooking> g_pendingImpulses;   // see the NOTE in simulation.h
  unsigned L2;
  unsigned L3 = 0;
  unsigned long BLOCK = 0;
  unsigned DIAG = 0;
  unsigned RMAX = 0;
  unsigned CONTRACT = 0;
  unsigned UPDATE = 0;
  unsigned ENCOUNTER = 0;
  unsigned GSLOT_X = 0, GSLOT_Y = 0, GSLOT_Z = 0;
  unsigned SLOT1 = 0, SLOT2 = 0, SLOT3 = 0, SLOT4 = 0, SLOT5 = 0;
  unsigned SLOT6 = 0, SLOT7 = 0, SLOT8 = 0;
  unsigned DIFFUSION = 0;
  unsigned RELOC = 0;
  unsigned REISSUE = 0;
  unsigned FLOOD = 0;
  unsigned FRAME = 0;
  unsigned ORDER;
  unsigned CENTER;
  unsigned FCENTER;
  unsigned int pulse_tick = 0;
  unsigned ISLAND_SIZE = 0;
  unsigned ISLAND_COUNT = 0;

  // Electroweak sieve modulus.  Default reproduces the reference runs; the
  // headless runners may lower it to open the s2B gate (manuscript, M1
  // parameter sweep).
  int s2b_target = 16384;

#ifdef S2B_TRACE
  // Opt-in in-loop instrumentation of the s2B (sieve) channel.  Compiled ONLY with
  // /DS2B_TRACE; the default build -- GUI and headless -- is unaffected, and nothing
  // here reads or writes the lattice.  It answers one question: at the instant a cell's
  // s2B turns on during the FSM cell pass, what happened to that cell's clock (t) and to
  // its active flag?
  //   Fired        - cells whose s2B went false -> true in this cell pass
  //   FiredTOff    - ... whose t did NOT simply advance by one (a clock reset or a skip)
  //   FiredTChg    - ... whose t changed at all
  //   FiredActOff  - ... that were active and are no longer
  //   ActLost      - any cell that left the active set in this pass (firing or not)
  //   ClockReset   - any cell whose t landed on 0 without the natural wrap (2*RMAX-1 -> 0)
  unsigned long long s2bTraceFired = 0, s2bTraceFiredTOff = 0, s2bTraceFiredTChanged = 0,
                     s2bTraceFiredActOff = 0, s2bTraceActLost = 0, s2bTraceClockReset = 0,
                     s2bTraceReemitResets = 0, s2bTraceCBResets = 0,
                     s2bTraceFloodPulls = 0, s2bTraceFloodResets = 0, s2bTraceReemitImpulse = 0,
                     s2bTraceRelocSeen = 0, s2bTraceRelocApplied = 0,
                     s2bTraceReemitAtCentre = 0, s2bTraceReemitOffCentre = 0,
                     s2bTraceCommitPending = 0, s2bTraceCommitWiped = 0, s2bTraceImpulseCommitted = 0,
                     s2bTraceImpulseBooked = 0, s2bTraceImpulseSkipped = 0, s2bTraceRelocSumAtCommit = 0,
                     s2bTraceTicks = 0, s2bTraceReapplySum = 0, s2bTraceCommitAccSum = 0,
                     s2bTraceImpulseDrained = 0, s2bTraceCommitCalls = 0, s2bTraceDrainWriteback = 0,
                     s2bTraceNetLayers = 0, s2bTraceNetMax = 0, s2bTraceChargeDispersion = 0,
                     s2bTracePolarSeed = 0,
                     // Relay-shuttle bookers (reseatStepToward / reseatAtContact): the last
                     // reloc writers that had no counter.  `Aligned` counts the steps whose
                     // three signs match the layer's own charge octant, `Other` the rest.
                     s2bTraceReseatSteps = 0, s2bTraceReseatAligned = 0,
                     s2bTraceReseatOther = 0, s2bTraceReseatAtContact = 0,
                     s2bTraceReseedCarried = 0;
  // Per-layer cohesion flag of the current light frame (see simulation.h).
  std::vector<unsigned char> s2bTraceCohesionFlag;
  // Per-layer writer mask of the current light frame (see simulation.h).
  std::vector<unsigned int> s2bTraceWriterMask;
  // Thrust word probe counters and flag of the current light frame (see simulation.h).
  unsigned long long s2bTraceThrustCalls = 0, s2bTraceThrustWordDiff = 0;
  std::vector<unsigned char> s2bTraceThrustWordFlag;
  std::vector<int> s2bTraceThrustVec;
  unsigned long long s2bTraceThrustBookingAligned = 0, s2bTraceThrustBookingPartial = 0,
                     s2bTraceThrustBookingAgainst = 0;
#endif

  // Lattices
  std::vector<Cell> lattice_curr;
  std::vector<Cell> lattice_draft;
  std::vector<Cell> lattice_partner;

  string lastAllocationError;
  std::vector<std::array<unsigned, 3>> lcenters;

  // ============================================================
  // TOROIDAL (TRANSLATION) NEIGHBOUR ADDRESSING
  // Manuscript Sect. "Boundary behavior": the lattice is a 3-torus.
  // A step across a face of the cube continues from the opposite face as a
  // pure translation -- orientation preserved, nothing mirrored.
  // ============================================================

  inline void spherical_wrap(int& x, int& y, int& z, int& w)
  {
    x = ((x % (int)ELX) + (int)ELX) % (int)ELX;
    y = ((y % (int)ELY) + (int)ELY) % (int)ELY;
    z = ((z % (int)ELZ) + (int)ELZ) % (int)ELZ;

    // The w slot keeps its historical edge pairing (self-loop at the first/
    // last layer): cross-layer adjacency is owned by rotatePartners()'s
    // rotation schedule, not by spatial geometry.
    if (w < 0) w = 0;
    if (w >= (int)W_USED) w = (int)W_USED - 1;
  }

  void trackCenter(unsigned x, unsigned y, unsigned z, unsigned w)
  {
    lcenters[w][0] = x;
    lcenters[w][1] = y;
    lcenters[w][2] = z;
  }

  // ============================================================
  // DISTANCE-FIELD UPDATE — CaRaSh-style incremental r²/r update
  // ============================================================
  // Maintains the integer radius r and squared radius r² from each layer's
  // moving source center using only additions and square-boundary tests.
  // The active wavefront is scheduled in phase_step by comparing r with
  // effective_t(t), so no sqrt/isqrt is needed here.
  // ============================================================

  void update_pulsating_wavefront()
  {
    // Copy current r2 and integer radius r into draft
    for (size_t i = 0; i < BLOCK; ++i)
    {
        lattice_draft[i].r2 = lattice_curr[i].r2;
        lattice_draft[i].r  = lattice_curr[i].r;
    }

    for (unsigned w = 0; w < W_USED; ++w)
    {
        int cx = (int)lcenters[w][0];
        int cy = (int)lcenters[w][1];
        int cz = (int)lcenters[w][2];

    for (unsigned x = 0; x < ELX; ++x)
    for (unsigned y = 0; y < ELY; ++y)
    for (unsigned z = 0; z < ELZ; ++z)
    {
        Cell &curr = getCell(lattice_curr, x, y, z, w);

        if (curr.r2 == INF_R2)
            continue;

        // Toroidal axis offsets to the source centre (manuscript Sect.
        // "Boundary behavior"): shortest wrapped distance per axis, so the
        // squared-distance relaxation measures geodesics on the 3-torus.
        int dx_ = (int)x - cx;
        int dy_ = (int)y - cy;
        int dz_ = (int)z - cz;
        int ax = dx_ < 0 ? -dx_ : dx_;
        int ay = dy_ < 0 ? -dy_ : dy_;
        int az = dz_ < 0 ? -dz_ : dz_;
        if (ax > (int)ELX - ax) ax = (int)ELX - ax;
        if (ay > (int)ELY - ay) ay = (int)ELY - ay;
        if (az > (int)ELZ - az) az = (int)ELZ - az;

        // 6-connected spatial neighbors (no w propagation)
        static const int offsets[6][3] = {
            {+1,0,0}, {-1,0,0},
            {0,+1,0}, {0,-1,0},
            {0,0,+1}, {0,0,-1}
        };

        for (int dir = 0; dir < 6; ++dir)
        {
            // Periodic address on the 3-torus: a step across a face of the
            // cube re-enters through the opposite face (no flux is lost).
            int nx = ((int)x + offsets[dir][0] + (int)ELX) % (int)ELX;
            int ny = ((int)y + offsets[dir][1] + (int)ELY) % (int)ELY;
            int nz = ((int)z + offsets[dir][2] + (int)ELZ) % (int)ELZ;

            // Incremental r2 difference
            unsigned diff;
            if (dir < 2)
                diff = 2 * ax + 1;
            else if (dir < 4)
                diff = 2 * ay + 1;
            else
                diff = 2 * az + 1;

            unsigned int new_r2 = curr.r2 + diff;

            Cell &nxt = getCell(lattice_draft, nx, ny, nz, w);

            if (new_r2 < nxt.r2)
            {
                nxt.r2 = new_r2;

                // Propagate the integer radius without isqrt.
                // Moving one 6-neighbor step changes the true radius by 0 or 1,
                // so the new radius is either the parent's r or r+1.
                int child_r = (curr.r < 0) ? 0 : curr.r;
                unsigned int next_sq = (unsigned int)(child_r + 1) * (unsigned int)(child_r + 1);
                if (new_r2 >= next_sq)
                    child_r++;

                // Tiny correction for the parent's r being one unit stale
                // (source centers move at most one cell per light frame).
                while (child_r > 0 && (unsigned int)new_r2 < (unsigned int)child_r * (unsigned int)child_r)
                    child_r--;
                while ((unsigned int)(child_r + 1) * (unsigned int)(child_r + 1) <= (unsigned int)new_r2)
                    child_r++;

                nxt.r = child_r;
            }
        }
    }
    }

    // Ensure each source center stays at 0
    for (unsigned w = 0; w < W_USED; ++w)
    {
        int cx = (int)lcenters[w][0];
        int cy = (int)lcenters[w][1];
        int cz = (int)lcenters[w][2];
        Cell &src = getCell(lattice_draft, (unsigned)cx, (unsigned)cy, (unsigned)cz, w);
        src.r2 = 0;
        src.r  = 0;
    }

    // Copy r2 and r back to curr; no isqrt needed.
    for (size_t i = 0; i < BLOCK; ++i)
    {
        lattice_curr[i].r2 = lattice_draft[i].r2;
        lattice_curr[i].r  = lattice_draft[i].r;
    }
  }

  // ============================================================
  // RADIAL POLARISATION — (u,v) pair, evolved as a 3-D integer wave
  // ============================================================

  static void phase_step()
  {
    if (RMAX == 0 || BLOCK == 0)
      return;

    // Wave parameters (same scaling as the former SincWave test).
    int R = (RMAX > 0u) ? (int)RMAX : 1;
    int shellR = (int)((RMAX * 24u) / 100u);
    int shellW = (int)(RMAX / 5u);
    if (shellW < 1) shellW = 1;
    int absorbW = (R / 27 > 2) ? (R / 27) : 2;

    int diffDivShift = 2;
    if (R >= 384)       diffDivShift = 6;
    else if (R >= 192)  diffDivShift = 5;
    else if (R >= 96)   diffDivShift = 4;
    else if (R >= 40)   diffDivShift = 3;

    int velDampShift = diffDivShift + 3;
    constexpr int DIFF_SHIFT = 4;
    constexpr int SHELL_TARGET = 16384;

    int ELXi = (int)ELX, ELYi = (int)ELY, ELZi = (int)ELZ;
    int Wi  = (int)W_USED;

    // First pass: compute next (u,v) and active/pB/sB into lattice_draft.
    for (int x = 0; x < ELXi; ++x)
    for (int y = 0; y < ELYi; ++y)
    for (int z = 0; z < ELZi; ++z)
    for (int w = 0; w < Wi;  ++w)
    {
        const Cell& c = getCell(lattice_curr, x, y, z, w);
        Cell&       d = getCell(lattice_draft, x, y, z, w);

        // Start from the current CA state and overwrite only the (u,v) wave fields.
        d = c;

        // Active wavefront: shell of integer radius pulseR moving at one cell per
        // light frame.  c.r is propagated/corrected by square-boundary tests in
        // update_pulsating_wavefront, so no sqrt or isqrt is needed here.
        const auto& center = lcenters[w];
        const Cell& source = getCell(lattice_curr, center[0], center[1], center[2], w);
        int pulseR = (int)effective_t(source.t);
        bool active = (c.r2 != INF_R2 && c.r >= 0 && c.r == pulseR);
        // The cell wave phase is the triangular breathing phase f = effective_t(t):
        // it rises 0 -> L/2 on the ascending branch and falls back on the descending.
        d.f = (unsigned)pulseR;

        // Hard zero outside the processed sphere (radial dead zone).  On the
        // 3-torus faces are not special: the spherical cavity is enforced
        // radially, and the source-centre cell (r2 == 0) stays exempt.
        if (c.r2 != 0 && (c.r < 0 || c.r >= R))
        {
            d.u = 0;
            d.v = 0;
            d.active = active ? 1u : 0u;
            d.phiB = active;
            d.pol_u = 0;
            d.pol_v = 0;
            d.pB = false;
            d.sB = false;
            d.s2B = false;
            continue;
        }

        int u = c.u;
        int v = c.v;
        int r = c.r;

        // NOTE: neighbour reads are made in-bounds by periodic wrapping
        // (3-torus, manuscript Sect. "Boundary behavior").  A wrapped image
        // contributes flux exactly like the interior, so the wave exchanges
        // no spurious amplitude across seams; only the explicit radial
        // sponge, damping and shell-source terms below change the totals.
        auto uAt = [&](int xx, int yy, int zz) -> int
        {
            xx = ((xx % ELXi) + ELXi) % ELXi;
            yy = ((yy % ELYi) + ELYi) % ELYi;
            zz = ((zz % ELZi) + ELZi) % ELZi;
            return getCell(lattice_curr, xx, yy, zz, w).u;
        };

        int neighbors_u =
              uAt(x + 1, y, z)
            + uAt(x - 1, y, z)
            + uAt(x, y + 1, z)
            + uAt(x, y - 1, z)
            + uAt(x, y, z + 1)
            + uAt(x, y, z - 1);

        int lap = neighbors_u - 6 * u;

        int diffShift = DIFF_SHIFT + 1 - (r >> diffDivShift);
        if (diffShift < DIFF_SHIFT - 1)
            diffShift = DIFF_SHIFT - 1;

        int v_new = v + (lap >> diffShift);
        int u_new = u + v_new;
        v_new -= (v_new >> velDampShift);

        // Spherical-shell source forcing.
        int dr = r - shellR;
        if (dr < 0) dr = -dr;
        if (dr <= shellW)
        {
            if (u > SHELL_TARGET)
                v_new -= (u - SHELL_TARGET) >> 4;
            else if ((pulse_tick & 3u) == 0u)
                v_new += ((SHELL_TARGET - u) >> 10) + 1;
        }

        // Absorbing outer boundary.
        if (R > absorbW && r > R - absorbW)
        {
            int dist = r - (R - absorbW);
            if (dist >= absorbW)
                u_new = 0;
            else if (dist > 0)
                u_new /= (1 << dist);
        }

        // Global damping.
        u_new -= (u_new >> 12);
        v_new -= (v_new >> 12);

        // Emergent transverse polarisation (manuscript, Sect. "Emergent
        // polarization pair"): the pair (pol_u, pol_v) is NOT a geometric
        // function of the local radius — it is reconstructed from the
        // broadcasted arrival stamp b(x) of the elected momentum vector,
        // approximating pol_u^2 + pol_v^2 = R^4 via isqrt (R = L/2 - 2 emergent).
        // No trigonometric tables, no precomputed spiral and no per-cell
        // fixed constants: the direction comes from the payload tournament
        // over the W-ledger and the phase from the isqrt relation applied
        // to the arrival time.
        int pol_u = 0, pol_v = 0;
        polarization::reconstructPair(c.bstamp, (int)RMAX - 2, pol_u, pol_v);
        d.pol_u = pol_u;
        d.pol_v = pol_v;

        d.u      = u_new;
        d.v      = v_new;
        d.active = active ? 1u : 0u;
        d.phiB   = active;
#ifdef POLAR_MAGNITUDE_FSM
        // WP8 (iii): define a direction as LIVE BY MAGNITUDE instead of by sign.
        // The flags decide whether every directional rule may act at all, while
        // the sign of pol follows the phase quadrant the reconstruction stamp
        // falls in (simulation.cpp:377-378, reference): the same tube and the same
        // bootstrap gave pol=(0,4) at N=3 (sector alive) and pol=(-4,0) at N=6
        // (every directional producer dead).  With a magnitude test, liveness
        // stops being a phase lottery.  OFF in the reference build.
        d.pB     = (pol_u != 0);
        d.sB     = (pol_v != 0);
#else
        d.pB     = (pol_u > 0);
        d.sB     = (pol_v > 0);
#endif

        // Sieve trigger: probability proportional to positive wave amplitude.
        // The modulus is the runtime-tunable s2b_target (default 16384); the
        // headless runners lower it to sweep the electroweak channel (M1).
        bool s2B_trigger = false;
        if (u_new > 0 && s2b_target > 0)
        {
            int64_t prod = (int64_t)u_new * (int64_t)(pulse_tick + 1);
            int64_t mod = prod % (int64_t)s2b_target;
            if (mod < (int64_t)u_new) s2B_trigger = true;
        }
        d.s2B    = active && s2B_trigger;
    }

    // Copy the new wave state back to lattice_curr for the interaction FSM.
    for (size_t i = 0; i < BLOCK; ++i)
    {
        lattice_curr[i].u      = lattice_draft[i].u;
        lattice_curr[i].v      = lattice_draft[i].v;
        lattice_curr[i].active = lattice_draft[i].active;
        lattice_curr[i].phiB   = lattice_draft[i].phiB;
        lattice_curr[i].pol_u  = lattice_draft[i].pol_u;
        lattice_curr[i].pol_v  = lattice_draft[i].pol_v;
        lattice_curr[i].pB     = lattice_draft[i].pB;
        lattice_curr[i].sB     = lattice_draft[i].sB;
        lattice_curr[i].s2B    = lattice_draft[i].s2B;
    }
  }

  // ============================================================
  // CPU UPDATE — BFS + interaction FSM
  // ============================================================

  // Apply the consumable relocation/impulse stored in the source-center cell.
  // The long-term momentum-direction vector m is preserved; only reloc is
  // consumed when a non-zero displacement is pending.
  static int wrapCoordAxis(int v, int M)
  {
    int r = v % M;
    if (r < 0) r += M;
    return r;
  }

  static void applyMomentum()
  {
    for (unsigned w = 0; w < W_USED; ++w)
    {
      unsigned cx = lcenters[w][0];
      unsigned cy = lcenters[w][1];
      unsigned cz = lcenters[w][2];
      Cell& old = getCell(lattice_draft, cx, cy, cz, w);

#ifdef HOMB_PRODUCER_FSM
      // WP8 consumer-chain probe.  The directional channel is supposed to carry
      // the homer's position in c[]/cB; the transport below consumes reloc[]
      // instead.  Counting c[] AT the source centre (where reloc[] is read)
      // separates "the carrier never arrived" from "it arrived and was never
      // converted into motion".
      if (old.c[0] || old.c[1] || old.c[2]) ++c_at_center;
      if (old.cB) ++cB_at_center;
      // Discriminate the two possible reasons the consumer below stays silent:
      // the distance field never coincides with a PENDING impulse (reloc != 0),
      // or the field is not there at all.
      if ((old.c[0] || old.c[1] || old.c[2]) &&
          (old.reloc[0] || old.reloc[1] || old.reloc[2])) ++c_with_reloc;
#endif

#ifdef HOMB_PRODUCER_FSM
        // WP8 locality probe.  Each copy's step is decoded from its OWN c[] and its
        // OWN x[] -- nothing else, no lcenters -- and the copies of a family are
        // compared once the family's last copy has been visited.  This answers
        // whether the family co-movement is EMERGENT from the shared field (in
        // which case the non-local co-location predicate of FAMILY_RIGID_FSM is
        // redundant and can be dropped, making the island formation a local
        // result) or whether it requires reading the host's per-layer table.
        {
          static std::vector<int>  locAx, locStep;
          static std::vector<unsigned> locT;
          static std::vector<unsigned char> locHas, locNow;
          if (locAx.size() != W_USED)
          {
            locAx.assign(W_USED, -1); locStep.assign(W_USED, 0); locT.assign(W_USED, 0u);
            locHas.assign(W_USED, 0); locNow.assign(W_USED, 0);
          }
          const unsigned LENP[3] = { ELX, ELY, ELZ };
          int pAx = -1, pStep = 0, pMag = 0;
          for (int ax = 0; ax < 3; ++ax)
          {
            if (!old.c[ax]) continue;
            const int LEN = (int)LENP[ax];
            int target;
            if ((int)old.c[ax] >= LEN)
            {
              int tt = (int)old.x[ax] - ((int)old.c[ax] - LEN);
              while (tt < 0)    tt += LEN;
              while (tt >= LEN) tt -= LEN;
              target = tt;
            }
            else target = (int)old.c[ax];
            int delta = target - (int)old.x[ax];
            if (delta >  LEN / 2)       delta -= LEN;
            else if (delta < -(LEN / 2)) delta += LEN;
            const int mag = delta < 0 ? -delta : delta;
            if (mag > pMag) { pMag = mag; pAx = ax; pStep = (delta > 0) ? +1 : -1; }
          }
          if (pAx >= 0 && pStep != 0) ++loc_step_total;
          if (old.c[0] || old.c[1] || old.c[2])
          {
            switch (w % 3u)
            {
              case 0u: ++c_center_c0; break;
              case 1u: ++c_center_c1; break;
              default: ++c_center_c2; break;
            }
          }
          locNow[w] = (pAx >= 0 && pStep != 0) ? 1u : 0u;
          if (locNow[w]) { locAx[w] = pAx; locStep[w] = pStep; locT[w] = old.t; locHas[w] = 1u; }
          const unsigned fam0p   = (w / 3u) * 3u;
          const unsigned famLast = (fam0p + 2u < W_USED) ? (fam0p + 2u) : (W_USED - 1u);
          if (w == famLast)
          {
            const unsigned famSize = famLast - fam0p + 1u;
            unsigned now = 0, known = 0, sameT = 1u, tRef = 0u;
            for (unsigned f = fam0p; f <= famLast; ++f)
            {
              now += locNow[f];
              if (locHas[f])
              {
                ++known;
                if (known == 1u) tRef = locT[f];
                else if (locT[f] != tRef) sameT = 0u;
              }
            }
            if (now >= 2u) ++fam_now_ge2;
            // Last-known comparison, only when every copy is known and the three
            // stamps agree (i.e. the copies were told at the same phase).
            if (known == famSize && sameT)
            {
              unsigned same = 0;
              int gAx = -2, gStep = 0;
              for (unsigned f = fam0p; f <= famLast; ++f)
              {
                if (gAx == -2) { gAx = locAx[f]; gStep = locStep[f]; }
                else if (locAx[f] == gAx && locStep[f] == gStep) ++same;
              }
              if (same + 1u == known) ++fam_all_agree;
              else                    ++fam_split;
            }
          }
        }
#endif

#ifdef HOMB_CONSUMER_TRANSPORT
      // WP8 (i): CONSUME the directional channel at the source level.  The
      // archived CUDA kernel transported through the lattice (relocate(),
      // `draft = north; draft.c[0]--`), which the c[] field still drives here --
      // but that drift is balanced, so the source centre never moves and the two
      // bubbles never separate.  This block decodes the relative encoding
      // c[i] = L + (walk - anchor) % L into ONE step toward the anchor and adds it
      // to reloc[], which is what the relocation below actually consumes; the
      // component is cleared on consumption.  NOT a literal CUDA port (the CUDA
      // had no c[] -> reloc[] coupling): candidate coupling only, introduced to
      // give the directional field a source-level consequence.  OFF in the
      // reference build.
      // The distance field arrives on its own: cB is relayed by the SLOT III/IV
      // rules while c[] is propagated by the homing block, and at a source centre
      // the two never coincide (measured at 15x9x9/N=3: cB_at_center=116,
      // c_at_center=930, disjoint), so requiring cB here would make this block
      // dead code -- exactly the failure mode WP8 was chasing.
      if (!old.reloc[0] && !old.reloc[1] && !old.reloc[2] &&
          (old.c[0] || old.c[1] || old.c[2]))
      {
        const unsigned LEN3[3] = { ELX, ELY, ELZ };
        // The field is written in TWO encodings and the `+ L` offset is the tag
        // (which is why the CUDA writes the relative form as L + (own-partner)%L):
        //   c <  L : the ABSOLUTE coordinate of the target cell;
        //   c >= L : the RELATIVE displacement of the target,
        //            since L + (walk - target) % L  =>  target = walk - (c - L).
        // Both decode to a shortest toroidal step.  Take ONE step per light frame
        // along the dominant axis: the harness asserts <= 1 cell per frame
        // (inertia_fixture.h:95), and this block only acts when no other impulse
        // is pending, so the locality bound holds.
        int bestAx = -1, bestMag = 0, bestStep = 0;
        for (int ax = 0; ax < 3; ++ax)
        {
          if (!old.c[ax]) continue;
          const int LEN = (int)LEN3[ax];
          int target;
          if ((int)old.c[ax] >= LEN)
          {
            int t = (int)old.x[ax] - ((int)old.c[ax] - LEN);
            while (t < 0)      t += LEN;
            while (t >= LEN)   t -= LEN;
            target = t;
          }
          else
            target = (int)old.c[ax];
          int delta = target - (int)old.x[ax];
          if (delta >  LEN / 2)      delta -= LEN;
          else if (delta < -(LEN / 2)) delta += LEN;
          const int mag = delta < 0 ? -delta : delta;
          if (mag > bestMag) { bestMag = mag; bestAx = ax; bestStep = (delta > 0) ? +1 : -1; }
        }
        // The locality bound is one cell per LIGHT FRAME (inertia_fixture.h:95),
        // while applyMomentum runs every tick, so latch on `t` -- the per-light-
        // frame counter -- and move at most once per frame and layer.
        static std::vector<unsigned> lastMoveT;
        if (lastMoveT.size() != W_USED) lastMoveT.assign(W_USED, 0u);
        // ------------------------------------------------------------------
        // WITHDRAWN: the family co-movement and the FAMILY_RIGID_FSM re-cohesion
        // used to live here.  Both read `lcenters[]` -- the host's per-layer table --
        // for the family's OTHER layers, in the same tick, and used the result as a
        // predicate on the transport.  That is a non-local read with a READABLE
        // consequence (the centre position), so it sits outside the non-signaling
        // idealisation, exactly like the host scheduler, and it cannot support any
        // claim about the model's own local rule.  A purely local probe measured
        // that every field arrival lands in the family's FIRST copy only
        // (15x9x9: N=3 c0=393/c1=0/c2=0; N=6 c0=801/c1=0/c2=0), with no tick in
        // which two copies were fielded at once and no agreeing steps, so no shared
        // field exists for a co-movement to emerge from: the table predicate was
        // doing all the work, and the island count settling at 79 +- 2 is a
        // HOST-LEVEL result.  See experiments/FAMILY_SELECTIVE_DESIGN.md,
        // "Locality audit of the rigid rule".
        //
        // The consumer is now purely local -- the cell's own c[], its own x[] and its
        // own t, nothing else.  What survives of FAMILY_SELECTIVE_FSM is its
        // ENCOUNTER half (the producer), which pairs two cells already in contact and
        // reads only their own x[3].  FAMILY_RIGID_FSM is a no-op until a local
        // (encounter-level) reformulation lands.
        // ------------------------------------------------------------------
        if (bestAx >= 0 && bestStep != 0 && old.t != lastMoveT[w])
        {
          old.reloc[bestAx] += bestStep;
          lastMoveT[w] = old.t;
          ++consumer_transports;
        }
        for (int ax = 0; ax < 3; ++ax) old.c[ax] = 0;
        old.cB = 0;
      }
#endif

#ifdef ADDRESS_TARGET_FSM
      // ==============================================================
      // CANDIDATE RULE: the ADDRESS supplies the PLACE (formation test).
      //
      // In the canonical superposed seed all ISLAND_COUNT = 9*EL islands are born
      // at one point, and nothing in the core rules transports a source centre, so
      // the islands never separate (measured: distinct_centers = 1, max_span = 0
      // over 64 journeys).  The only datum that distinguishes one island from
      // another LOCALLY is its own address: island i = w / ISLAND_SIZE.  This rule
      // reads that address on the cell and walks the layer's centre to the site the
      // address labels -- (i % EL, (i / EL) % EL, CENTER) -- one lattice cell per
      // light frame, using only lcenters[w] (this layer) and its own x[3].  No
      // table, no communication, no partner: every one of the island's ISLAND_SIZE
      // layers computes the SAME target from the SAME address, so the copies walk
      // in lockstep and an island arrives whole and stays co-located.  The walk
      // stops when the site is reached, so the absorbing state characterised in
      // experiments/ISLAND_CENSUS.md ("quantised islands") is this rule's fixed
      // point.  One axis per light frame (the harness asserts <= 1 cell per frame
      // per constituent, inertia_fixture.h:95) and only when no other impulse is
      // pending.  OFF in the reference build.
      // ==============================================================
      if (ISLAND_SIZE > 0u && EL > 0u)
      {
        // CELL-LOCAL ONLY: the address and the position are read from the cell
        // itself (old.x[3] is this cell's W address, old.x[0..2] its coordinates).
        // lcenters[w] is the host's mirror of exactly these fields (trackCenter
        // copies them), so reading the cell instead removes any reliance on the
        // host table: nothing outside this cell is consulted -- not another
        // layer's centre, and not even this layer's table entry.
        const unsigned isl = (unsigned)old.x[3] / ISLAND_SIZE;
        const unsigned tX  = isl % EL;
        const unsigned tY  = (isl / EL) % EL;
        const unsigned tZ  = CENTER;
        const int LENc[3]  = { (int)ELX, (int)ELY, (int)ELZ };
        const int own[3]   = { (int)old.x[0], (int)old.x[1], (int)old.x[2] };
        const int tgt[3]   = { (int)tX, (int)tY, (int)tZ };
        int bestAx = -1, bestMag = 0, bestStep = 0;
        for (int ax = 0; ax < 3; ++ax)
        {
          const int LEN = LENc[ax];
          int d = tgt[ax] - own[ax];
          if (d >  LEN / 2)      d -= LEN;
          else if (d < -(LEN / 2)) d += LEN;
          const int mag = d < 0 ? -d : d;
          if (mag > bestMag) { bestMag = mag; bestAx = ax; bestStep = (d > 0) ? +1 : -1; }
        }
        if (bestAx >= 0 && bestStep != 0 &&
            !old.reloc[0] && !old.reloc[1] && !old.reloc[2])
        {
          static std::vector<unsigned> lastWalkT;
          if (lastWalkT.size() != W_USED) lastWalkT.assign(W_USED, 0u);
          if (old.t != lastWalkT[w])
          {
            old.reloc[bestAx] += bestStep;
            lastWalkT[w] = old.t;
            ++address_walks;
          }
        }
      }
#endif

      // Free photon pairs expand and are gradually consumed. At maximum
      // radius (t == RMAX) one pair is consumed; when the stack empties the
      // two partner source centers are released as singletons moving apart.
      if (old.kind == SourceKind::P &&
          old.a == W_USED &&
          old.pair_idx != NO_PAIR &&
          old.t == (unsigned)RMAX &&
          old.w < old.pair_idx)
      {
#ifdef ORPHAN_MEDIATOR_SUSTAIN
          // Self-sustaining mediator (experimental): a FREE pair that reaches
          // the turnaround is RE-EMITTED in place (phase reset to 0) instead of
          // being consumed, so the vacuum photon keeps propagating indefinitely
          // and the mediated channel neither dies nor needs a seeded stack.
          old.t = 0;
          old.f = 0;
          {
            const WIndex pw0 = old.pair_idx;
            Cell& partner0 = getCell(lattice_draft,
                                     (unsigned)lcenters[pw0][0],
                                     (unsigned)lcenters[pw0][1],
                                     (unsigned)lcenters[pw0][2],
                                     pw0);
            partner0.t = 0;
            partner0.f = 0;
          }
#else
          if (old.pair_count > 0)
              old.pair_count--;

          WIndex pw = old.pair_idx;
          Cell& partner = getCell(lattice_draft,
                                  (unsigned)lcenters[pw][0],
                                  (unsigned)lcenters[pw][1],
                                  (unsigned)lcenters[pw][2],
                                  pw);

          if (old.pair_count == 0)
          {
              // Last pair consumed: release two singletons.
              old.kind       = SourceKind::S;
              old.pair_idx   = NO_PAIR;
              old.pair_count = 0;
              old.leader_w   = NO_LEADER_W;
              old.a          = W_USED;

              partner.kind       = SourceKind::S;
              partner.pair_idx   = NO_PAIR;
              partner.pair_count = 0;
              partner.leader_w   = NO_LEADER_W;
              partner.a          = W_USED;

              int axis = (int)(old.w % 3u);
              int sign = ((old.w & 1u) ? +1 : -1);
              old.reloc[axis]     += sign;
              partner.reloc[axis] -= sign;
          }
          else
          {
              // The stack still holds pairs; keep the remaining count in sync.
              partner.pair_count = old.pair_count;
          }
#endif
      }

      int dx = old.reloc[0];
      int dy = old.reloc[1];
      int dz = old.reloc[2];

      if (dx == 0 && dy == 0 && dz == 0)
        continue;
#ifdef S2B_TRACE
      ++s2bTraceRelocSeen;   // a pending impulse reached the source centre
#endif
#ifdef HOMB_PRODUCER_FSM
      ++reloc_moves;   // WP8 probe: a source centre is about to be relocated
#endif

      int nx = wrapCoordAxis((int)cx + dx, (int)ELX);
      int ny = wrapCoordAxis((int)cy + dy, (int)ELY);
      int nz = wrapCoordAxis((int)cz + dz, (int)ELZ);

      // Translate this bubble's state bijectively on the torus. Moving only
      // its centre left stale r2 minima and lost charge/affinity at the new
      // centre. This is source transport, not a new emission or a clock reset.
      old.reloc[0] = old.reloc[1] = old.reloc[2] = 0;
      std::vector<Cell> shifted((size_t)ELX * ELY * ELZ);
      for (unsigned x=0;x<ELX;++x)
      for (unsigned y=0;y<ELY;++y)
      for (unsigned z=0;z<ELZ;++z) {
        const unsigned tx = wrapCoordAxis((int)x+dx, ELX);
        const unsigned ty = wrapCoordAxis((int)y+dy, ELY);
        const unsigned tz = wrapCoordAxis((int)z+dz, ELZ);
        Cell& dest = shifted[((size_t)tx*ELY+ty)*ELZ+tz];
        dest = getCell(lattice_draft,x,y,z,w);
        dest.x[0]=tx; dest.x[1]=ty; dest.x[2]=tz; dest.x[3]=w;
      }
      for (unsigned x=0;x<ELX;++x)
      for (unsigned y=0;y<ELY;++y)
      for (unsigned z=0;z<ELZ;++z)
        getCell(lattice_draft,x,y,z,w) = shifted[((size_t)x*ELY+y)*ELZ+z];

      lcenters[w][0] = (unsigned)nx;
      lcenters[w][1] = (unsigned)ny;
      lcenters[w][2] = (unsigned)nz;
#ifdef S2B_TRACE
      ++s2bTraceRelocApplied;   // the bubble was translated by (dx, dy, dz)
#endif
    }
  }

  // ============================================================
  // Fatia 2 — M/Mbar turnaround hook (flag-gated, default OFF).
  //
  // Implements the fsm.md §10 "future hook for charge inversion at
  // t == RMAX" as a controlled knob:
  //   simulation.mm_eps    C/CP-like bias of the conjugation rate
  //   simulation.mm_pbase  base probability per turnaround
  //   simulation.mm_seed   xorshift32 seed (deterministic runs)
  //
  // At each island turnaround (draft centre t == RMAX, edge-latched) the
  // bubble conjugates with probability
  //     p = mm_pbase * (1 + mm_eps * bias),   bias = +1 matter, -1 anti,
  // where matter/anti is read from the CURRENT charge word.  The
  // current-charge feedback is the only coupling that produced a
  // persistent excess in the virada.c ablation (birth-identity anchors
  // and sector mirrors wash out to noise).
  //
  // The conjugation is sector-preserving (toy "mode 0") and stays inside
  // the 32 valid charge words: ch ^= 0x1F flips color (c -> 7-c), q and
  // w0 while keeping w1 — so the q = w0 ^ w1 invariant still holds and
  // the word crosses the matter/anti threshold (popcount s -> 3-s).
  //
  // With mm_eps == 0 this routine is an exact no-op (bit-identical run).
  // ============================================================
  void applyChargeConjugation()
  {
    static bool inited = false;
    static double cfgEps = 0.0, cfgPbase = 1.0;
    static unsigned latchedSize = 0;
    static std::vector<unsigned char> latched;   // per-island edge latch
    static uint32_t rng = 1u;
    static long long netCellBias = 0;  // (+) A->M - M->A over the run; D moves +2/-2

    if (lattice_draft.empty() || BLOCK == 0 || W_USED == 0)
      return;

    if (!inited)
    {
      inited      = true;
      cfgEps      = gConfig.simulation.mm_eps;
      cfgPbase    = gConfig.simulation.mm_pbase;
      rng         = gConfig.simulation.mm_seed | 1u;
      latched.assign(W_USED, 0);
      latchedSize = W_USED;
      netCellBias = 0;
      fprintf(stderr, "[mm] init eps=%g pbase=%g seed=%u W_USED=%u RMAX=%u\n",
              cfgEps, cfgPbase, rng, W_USED, RMAX);
    }
    if (cfgEps == 0.0 || latchedSize != W_USED)
      return;                    // hook disabled (default) / lattice resized

    unsigned events = 0, flips = 0;

    for (unsigned w = 0; w < W_USED; ++w)
    {
      const unsigned cx = lcenters[w][0];
      const unsigned cy = lcenters[w][1];
      const unsigned cz = lcenters[w][2];
      Cell& dc = getCell(lattice_draft, (int)cx, (int)cy, (int)cz, (int)w);

      // DEBUG probe (stderr = unbuffered): what the hook actually reads.
      if (w == 0 && (pulse_tick % 256u) == 0u)
        fprintf(stderr, "[mm] probe tick=%u draft_t=%u RMAX=%u a=%u latched=%u\n",
                pulse_tick, dc.t, RMAX, dc.a, latched[w]);

      if (dc.t != RMAX) { latched[w] = 0; continue; }
      if (latched[w])   continue;  // same turnaround already handled
      latched[w] = 1;

      if (dc.a == W_USED) continue;  // released singleton: no island left

      ++events;

      const bool isMatter =
          ((dc.ch & 1u) + ((dc.ch >> 1) & 1u) + ((dc.ch >> 2) & 1u)) < 2u;
      const double bias = isMatter ? 1.0 : -1.0;

      double p = cfgPbase * (1.0 + cfgEps * bias);
      if (p < 0.0) p = 0.0;
      if (p > 1.0) p = 1.0;

      rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
      const double u = (double)(rng >> 8) / 16777216.0;    // uniform [0,1)
      if (u >= p) continue;

      const unsigned char chOld = dc.ch;
      const unsigned char chNew = (unsigned char)(chOld ^ 0x1Fu);

      // Conjugate every attached cell of the island in all three lattices,
      // so the flip survives the swap and the boundary partner stays coherent.
      // The matter/anti ledger is PER CELL: a merged island holds mixed
      // charges, and stamping C(center) flips each cell independently of the
      // center's own sign.  netCellBias accumulates the exact cell-level
      // matter-count change, so DeltaD = 2*netCellBias holds identically.
      long long eventBias = 0;
      for (unsigned x = 0; x < ELX; ++x)
      for (unsigned y = 0; y < ELY; ++y)
      for (unsigned z = 0; z < ELZ; ++z)
      {
        Cell& d = getCell(lattice_draft, (int)x, (int)y, (int)z, (int)w);
        const unsigned char chBefore = d.ch;   // capture BEFORE overwrite
        if (d.a != W_USED) d.ch = chNew;
        Cell& c2 = getCell(lattice_curr, (int)x, (int)y, (int)z, (int)w);
        if (c2.a != W_USED) c2.ch = chNew;
        Cell& m2 = getCell(lattice_partner, (int)x, (int)y, (int)z, (int)w);
        if (m2.a != W_USED) m2.ch = chNew;

        if (d.a != W_USED)
        {
          const bool mBefore = ((chBefore & 1u) + ((chBefore >> 1) & 1u) + ((chBefore >> 2) & 1u)) < 2u;
          const bool mAfter  = ((chNew & 1u) + ((chNew >> 1) & 1u) + ((chNew >> 2) & 1u)) < 2u;
          eventBias += (mAfter ? 1 : 0) - (mBefore ? 1 : 0);
        }
      }

      ++flips;
      netCellBias += eventBias;   // exact cell-level matter-count delta
      printf("[mm] tick=%u w=%u %s ch=0x%02X->0x%02X p=%.3f cellBias=%+lld\n",
             pulse_tick, w, isMatter ? "M->A" : "A->M", chOld, chNew, p, eventBias);
    }

    if (events > 0)
      printf("[mm] tick=%u turnarounds=%u flips=%u netCellBias=%+lld (DeltaD = 2*netCellBias)\n",
             pulse_tick, events, flips, netCellBias);
  }

#ifdef COLOR_ENCOUNTER_FSM
#include "color_fsm.inc"
#endif

  void update_lattice_cpu()
  {
#ifdef COLOR_ENCOUNTER_FSM
    colorFsmTick();
    return;
#endif
    // Phase 1: CaRaSh-style incremental distance field (r2/r) from each
    // moving source center using only additions and square comparisons.
    update_pulsating_wavefront();
    pulse_tick++;

    // Fatia 1 instrumentation (read-only): virgin-wrap ledger + throttled
    // matter/antimatter census.  Never writes to the lattice.
    chargesSampleTurnarounds();
    chargesReport(pulse_tick);

    // Phase 2: radial polarisation pair (u,v) and active wavefront flag.
    // The active shell is c.r == effective_t(c.t): one cell per light frame.
    phase_step();

    // The phase output is in lattice_draft.  Promote it to the live state so
    // the FSM can read it and write its own modifications back to lattice_draft.
    std::swap(lattice_curr, lattice_draft);

    // Phase 2b: emergent polarization broadcast.  At every expansion limit
    // of the breathing wavefront a momentum direction is elected out of
    // the automaton's own W-ledger; a helical walker then stamps arrival
    // ticks b(x) around the elected axis, so phase_step() can reconstruct
    // the transverse pair on the NEXT tick.  Runs on the promoted live
    // lattice; the FSM below copies the stamps into draft/partner.
    polarization::tick();
    beginSourceTick();

    // DEBUG: throttle a snapshot of the central cell so we can verify (u,v) are evolving.
    if (pulse_tick % 100 == 0) {
        const Cell& c = getCell(lattice_curr, CENTER, CENTER, CENTER, 0);
        printf("DEBUG phase tick %u: center u=%d v=%d active=%u pB=%d sB=%d pol=(%d,%d) bstamp=%u\n",
               pulse_tick, c.u, c.v, c.active, c.pB ? 1 : 0, c.sB ? 1 : 0,
               c.pol_u, c.pol_v, c.bstamp);
    }

    // Phase 3: FSM interaction loop (uses r2 instead of d)
    for (unsigned w = 0; w < W_USED; ++w)
    {
        if (w == 0)
        {
            const Cell& first = lattice_curr.front();
            if (gConfig.delays.convol && first.k < ENCOUNTER)
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
            else if (diffuse_delay && first.k >= ENCOUNTER && first.k < DIFFUSION)
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            else if (reloc_delay && first.k >= DIFFUSION && first.k < RELOC)
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }

        for (unsigned x = 0; x < ELX; ++x)
        for (unsigned y = 0; y < ELY; ++y)
        for (unsigned z = 0; z < ELZ; ++z)
        {
            Cell &curr   = getCell(lattice_curr, x, y, z, w);
            Cell &draft  = getCell(lattice_draft, x, y, z, w);
            Cell &partner = getCell(lattice_partner, x, y, z, w);

            draft = curr;

#ifdef S2B_TRACE
            const unsigned trTIn  = curr.t;
            const bool     trS2BIn = curr.s2B;
            const bool     trActIn  = curr.active != 0;
#endif

            // Ensure correct coordinates
            curr.x[0] = x;
            curr.x[1] = y;
            curr.x[2] = z;
            curr.x[3] = w;

            Cell &forward = curr.getNeighbor(FORWARD);
            Cell &north   = curr.getNeighbor(NORTH);
            Cell &west    = curr.getNeighbor(WEST);
            Cell &down    = curr.getNeighbor(DOWN);
            Cell &south   = curr.getNeighbor(SOUTH);
            Cell &east    = curr.getNeighbor(EAST);
            Cell &up      = curr.getNeighbor(UP);

            if (curr.k < ENCOUNTER) {
                // W rotation selects the partner identity. Its wave flags
                // must belong to this physical instant, not the preceding
                // frame's shell (which never overlaps a synchronous shell).
                Cell contactPartner = getCell(lattice_curr, x, y, z, partner.w);
                encounter(curr, draft, contactPartner);
            } else if (curr.k < GSLOT_Z) {
                // glider slots
            } else if (curr.k < DIFFUSION) {
                diffuse(curr, draft, forward, north, west, down, south, east, up);
            } else if (curr.k < RELOC) {
                relocate(curr, draft, north, west, down);
            } else if (curr.k < REISSUE) {
                reissue(curr, draft, forward, north, west, down, south, east, up);
            } else if (curr.k < FLOOD) {
                flood(curr, draft, forward, north, west, down, south, east, up);
            }

            draft.k = (curr.k + 1) % FRAME;

            if (draft.k == 0) {
                if (curr.a == W_USED && curr.t <= RMAX)
                    draft.t++;
                else
                    draft.t = (curr.t + 1) % (2 * RMAX);
            }

#ifdef S2B_TRACE
            {
                const unsigned trNext = unsigned((curr.t + 1u) % (2u * RMAX));
                if (draft.s2B && !trS2BIn) {
                    ++s2bTraceFired;
                    if (draft.t != trTIn)     ++s2bTraceFiredTChanged;
                    if (draft.t != trNext)    ++s2bTraceFiredTOff;
                    if (trActIn && !draft.active) ++s2bTraceFiredActOff;
                }
                if (trActIn && !draft.active) ++s2bTraceActLost;
                if (draft.t == 0 && trTIn != 0 && trTIn != 2u * RMAX - 1u) ++s2bTraceClockReset;
            }
#endif
        }
    }

    // Fatia 2 — M/Mbar hook: conjugate islands at their breathing turnaround.
    // Runs BEFORE applyMomentum() so it sees the same draft t == RMAX state
    // the pair consumption uses.  Exact no-op when mm_eps == 0.
    commitSourceTick();
    applyChargeConjugation();

    // Apply source-center momentum and update pulsation centers.
    applyMomentum();
  }

  void update_lattice()
  {
#ifdef USE_CUDA
    if (isCudaEnabled()) {
        cudaSimulationStepWrapper();
        return;
    }
#endif
    update_lattice_cpu();
  }

  bool swap_lattices_cpu()
  {
    if (BLOCK == 0 || lattice_curr.empty())
        return false;

    bool newLightFrame = false;

    std::copy(
        lattice_draft.begin(),
        lattice_draft.begin() + BLOCK,
        lattice_curr.begin());

    Cell &repr = getCell(lattice_curr, 0, 0, 0, 0);

    if (repr.k == 0)
    {
      for (unsigned w = 0; w < W_USED; ++w)
      for (unsigned x = 0; x < ELX; ++x)
      for (unsigned y = 0; y < ELY; ++y)
      for (unsigned z = 0; z < ELZ; ++z)
      {
          Cell &curr = getCell(lattice_curr, x, y, z, w);
          Cell &partner = getCell(lattice_partner, x, y, z, w);
          partner = curr;
          partner.f = effective_t(partner.t);
      }
      newLightFrame = true;
    }

    if (repr.k < ENCOUNTER)
        rotatePartners();

    return newLightFrame;
  }

  bool swap_lattices()
  {
#ifdef USE_CUDA
    if (isCudaEnabled()) {
        Cell &repr = getCell(lattice_curr, 0, 0, 0, 0);
        return (repr.k == 0);
    }
#endif
    return swap_lattices_cpu();
  }

  bool simulation()
  {
    update_lattice();
    return swap_lattices();
  }

  // ============================================================
  // Neighbor accessor
  // ============================================================

  Cell &Cell::getNeighbor(int i)
  {
    static int disp[8][4] =
    {
        {+1,0,0,0},{-1,0,0,0},
        {0,+1,0,0},{0,-1,0,0},
        {0,0,+1,0},{0,0,-1,0},
        {0,0,0,+1},{0,0,0,-1}
    };

    int nx = x[0] + disp[i][0];
    int ny = x[1] + disp[i][1];
    int nz = x[2] + disp[i][2];
    int nw = x[3] + disp[i][3];

    spherical_wrap(nx, ny, nz, nw);

    return getCell(lattice_curr, nx, ny, nz, nw);
  }
} // namespace automaton
