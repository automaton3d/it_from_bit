// charges.cpp
//
// Fatia 1 — charge census & virgin-wrap ledger (instrumentation only).
//
// Measures the three quantities the bariogenesis study (virada.c) needs
// before any dynamics change:
//   (1) the emergent matter/antimatter composition of the live lattice;
//   (2) the fraction of source turnarounds (t == RMAX crossings) that
//       happen WITHOUT any interaction in between ("virgin wraps") —
//       the pbase(lap) the toy model had to prescribe by hand;
//   (3) [idea B / C-retina] the split of anti content into "hidden" —
//       fragments bound inside a registered P formation (kind==P,
//       charge-complementary by canFormPair) — vs "visible" free
//       leftover.  A hidden anti fragment never scatters as a free
//       particle, so a plain color-weight census over-counts visible
//       antimatter by exactly pairA = anti-colored P cells.
//
// READ-ONLY over the lattice: nothing here writes to cells, so the
// automaton dynamics are bit-identical with or without this module.
// CPU path only (update_lattice_cpu); the CUDA bridge is untouched.
//
// Charge encoding (initSim.cpp:76):  ch = color | q<<3 | w0<<4 | w1<<5
//   color = ch & 0x07 = (c2 c1 c0)
//   Matter colors  (SIG < 2 set bits): N(000), R(001), G(010), B(100)
//   Antimatter     (SIG >= 2):         B~(011), G~(101), R~(110), N~(111)
// — the same classification used by the virada.c / combine.c toy studies.

#include "model/simulation.h"

#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>

namespace automaton
{
  // Globals owned by the lattice/simulation translation units.
  extern unsigned EL;
  extern unsigned W_USED;
  extern std::vector<std::array<unsigned, 3>> lcenters;

  namespace
  {
    constexpr unsigned REPORT_EVERY = 256;   // census cadence (ticks)

    std::vector<uint64_t> matCount;          // per layer, last census
    std::vector<uint64_t> antiCount;
    std::vector<uint64_t> orphanMat;         // cells outside any island (a == W_USED)
    std::vector<uint64_t> orphanAnti;
    std::vector<unsigned> prevT;             // last seen source-center clock
    std::vector<uint8_t>  dirty;             // island interacted since last turnaround
    uint64_t turnVirgin = 0;
    uint64_t turnDirty  = 0;
    uint64_t pairFormations = 0;   // cumulative registered P formations (idea B)
    uint64_t blobFormations  = 0;   // cumulative blob groups (manuscript "Blob")
    uint64_t annihilationEvents = 0;// cumulative representative annihilations (rule 5)
    // Attractor sector-flux scaffolding (referenced by chargesReset):
    // per-sector foreign-affinity account + per-layer attachment markers.
    uint64_t foreignAff[2] = {0, 0};
    std::vector<uint8_t> layerAttach;

    inline unsigned popcount3(unsigned color)
    {
      return (color & 1u) + ((color >> 1) & 1u) + ((color >> 2) & 1u);
    }

    // Sector from the charge word: bit5 = w1 (0 = Orbis, 1 = Umbra).
    inline unsigned sectorOf(unsigned char ch)
    {
      return (ch >> 5) & 1u;
    }

    inline bool ledgerReady()
    {
      return (!dirty.empty() && dirty.size() == W_USED &&
              lcenters.size() >= W_USED);
    }
  }

  void chargesReset()
  {
    matCount.assign(W_USED, 0);
    antiCount.assign(W_USED, 0);
    orphanMat.assign(W_USED, 0);
    orphanAnti.assign(W_USED, 0);
    prevT.assign(W_USED, 0);
    dirty.assign(W_USED, 0);
    turnVirgin = 0;
    turnDirty  = 0;
    pairFormations = 0;
    blobFormations  = 0;
    foreignAff[0] = foreignAff[1] = 0;
    layerAttach.assign(W_USED, 0);
  }

  void chargesMarkInteraction(unsigned w)
  {
    if (w < dirty.size())
      dirty[w] = 1;
  }

  void chargesMarkPair()
  {
    ++pairFormations;
  }

  void chargesMarkAnnihilation()
  {
    ++annihilationEvents;
  }

  void chargesMarkBlob()
  {
    ++blobFormations;
  }

  void chargesSampleTurnarounds()
  {
    if (!ledgerReady() || lattice_curr.empty())
      return;

    for (unsigned w = 0; w < W_USED; ++w)
    {
      const std::array<unsigned, 3>& c3 = lcenters[w];
      const Cell& c = getCell(lattice_curr, (int)c3[0], (int)c3[1], (int)c3[2], (int)w);
      const unsigned t    = c.t;
      const unsigned prev = prevT[w];
      prevT[w] = t;

      // The breathing clock crossed into the expansion limit this window:
      // a turnaround of island w.  Virgin iff no reemission happened since
      // the previous crossing (reemitSourceAt resets t to 0, which also
      // cannot produce a false crossing: prev >= t then).
      if (prev < RMAX && t >= RMAX)
      {
        if (dirty[w] != 0)
        {
          ++turnDirty;
          dirty[w] = 0;
        }
        else
        {
          ++turnVirgin;
          printf("[charges] VIRGIN turnaround w=%u tick=%u t=%u\n",
                 w, pulse_tick, t);
          fflush(stdout);
        }
      }
    }
  }

  void chargesReport(unsigned tick)
  {
    if (tick % REPORT_EVERY != 1)   // baseline at tick 1, then every 256
      return;
    if (!ledgerReady() || lattice_curr.empty())
      return;

    for (unsigned w = 0; w < W_USED; ++w)
    {
      matCount[w]  = 0;  antiCount[w]  = 0;
      orphanMat[w] = 0;  orphanAnti[w] = 0;
    }

    uint64_t totMat = 0, totAnti = 0, totOrphanMat = 0, totOrphanAnti = 0;
    uint64_t sMat[2] = {0,0}, sAnti[2] = {0,0};              // per sector (w1)
    uint64_t sOrphanMat[2] = {0,0}, sOrphanAnti[2] = {0,0};
    uint64_t pairMat[2] = {0,0}, pairAnti[2] = {0,0};        // inside P formations

    for (unsigned w = 0; w < W_USED; ++w)
    {
      for (unsigned x = 0; x < ELX; ++x)
      for (unsigned y = 0; y < ELY; ++y)
      for (unsigned z = 0; z < ELZ; ++z)
      {
        const Cell& c = getCell(lattice_curr, (int)x, (int)y, (int)z, (int)w);
        const bool isMatter = popcount3(c.ch & 0x07u) < 2u;
        const bool orphan   = (c.a == W_USED);
        const unsigned sec  = sectorOf(c.ch);           // 0 = Orbis, 1 = Umbra
        // C-retina label (idea B): is this cell the member of a *registered*
        // formation?  kind==P marks source-center cells whose charge word is
        // the C-partner of the other P half (canFormPair guarantees
        // complementarity), i.e. a neutral pair.  Such anti fragments are
        // hidden: never visible to free-particle / leftover scattering.
        const bool inPair   = (c.kind == SourceKind::P);

        if (isMatter)
        {
          ++matCount[w]; ++totMat; ++sMat[sec];
          if (inPair) { ++pairMat[sec]; }
          if (orphan) { ++orphanMat[w]; ++totOrphanMat; ++sOrphanMat[sec]; }
        }
        else
        {
          ++antiCount[w]; ++totAnti; ++sAnti[sec];
          if (inPair) { ++pairAnti[sec]; }
          if (orphan) { ++orphanAnti[w]; ++totOrphanAnti; ++sOrphanAnti[sec]; }
        }
      }
    }

    const double total = (double)totMat + (double)totAnti;
    const double ratio = (total > 0.0) ? (double)totMat / total : 0.0;

    printf("[charges] tick=%u MAT=%llu ANTI=%llu mat/total=%.6f | orphanM=%llu orphanA=%llu | turnarounds V=%llu D=%llu\n",
           tick,
           (unsigned long long)totMat, (unsigned long long)totAnti,
           ratio,
           (unsigned long long)totOrphanMat, (unsigned long long)totOrphanAnti,
           (unsigned long long)turnVirgin, (unsigned long long)turnDirty);

    // -------------------------------------------------------------
    // Closure report (idea A): the charge balance D(t) per bucket.
    //   D_islands = mat(t) - anti(t) restricted to affiliated cells
    //   D_orphans = mat(t) - anti(t) restricted to orphan cells (a==W_USED)
    //   D_total   = D_islands + D_orphans
    //
    // If pair formation / capture / escape conserve the charge balance,
    // D_total(t) is EXACTLY constant across census ticks (baseline skew
    // is the seed's intrinsic imbalance, not dynamics).  The M/Mbar hook
    // is the only thing that can move D_total: each A->M flip adds +2,
    // each M->A flip subtracts 2 (see [mm] netCellBias).
    //
    // "Does leftover hide antimatter?"  D_islands drifting upward against
    // a matching D_orphans compensating drift = antimatter being parked
    // in the orphan vacuum sea while the visible islands stay matter-rich.
    // -------------------------------------------------------------
    const long long dIslands = ((long long)totMat - (long long)totOrphanMat) -
                               ((long long)totAnti - (long long)totOrphanAnti);
    const long long dOrphans = (long long)totOrphanMat - (long long)totOrphanAnti;
    const long long dTotal   = dIslands + dOrphans;
    const long long dOrb     = (long long)sMat[0] - (long long)sAnti[0];
    const long long dUmb     = (long long)sMat[1] - (long long)sAnti[1];
    // Affiliated-cell balance per sector. Compare only with attractor's
    // AffinityCells observable (baseline + net flux + charge conversion),
    // not with its default chief-constituent census, which counts sources.
    const long long dIslOrb  = ((long long)sMat[0] - (long long)sOrphanMat[0]) -
                               ((long long)sAnti[0] - (long long)sOrphanAnti[0]);
    const long long dIslUmb  = ((long long)sMat[1] - (long long)sOrphanMat[1]) -
                               ((long long)sAnti[1] - (long long)sOrphanAnti[1]);
    printf("[charges] closure tick=%u Disl=%+lld Dorph=%+lld Dtot=%+lld Dorb=%+lld Dumb=%+lld DslOrb=%+lld DslUmb=%+lld | orbM=%llu orbA=%llu umbM=%llu umbA=%llu\n",
           tick, dIslands, dOrphans, dTotal, dOrb, dUmb, dIslOrb, dIslUmb,
           (unsigned long long)sMat[0], (unsigned long long)sAnti[0],
           (unsigned long long)sMat[1], (unsigned long long)sAnti[1]);

    // -------------------------------------------------------------
    // Retina report (idea B): the second, orthogonal label.
    //
    //   pairM / pairA : cells inside registered P formations, by
    //                   color-weight.  Each such anti cell is a fragment
    //                   whose C-partner is the other half of the pair —
    //                   HIDDEN antimatter (never scatters as a free
    //                   particle / leftover).
    //   dPair         : pairM - pairA.  The net matter excess carried
    //                   INSIDE neutral formations.  Non-zero iff pairs
    //                   are not strictly M+A symmetric (neutrino-like
    //                   R3/R4 content contributes +2/-2).
    //   freeM / freeA : matter / anti cells NOT in a formation — the
    //                   visible leftover + free particles.
    //   freeD         : freeM - freeA.  The imbalance the visible sector
    //                   carries.  Identity:  D_tot = dPair + freeD.
    //
    // "Does leftover hide antimatter?"  pairA answers quantitatively:
    // the plain census's ANTI over-counts VISIBLE antimatter by exactly
    // the anti fragments locked inside neutral pairs.
    // -------------------------------------------------------------
    const uint64_t pairM_all = pairMat[0] + pairMat[1];
    const uint64_t pairA_all = pairAnti[0] + pairAnti[1];
    const long long dPair    = (long long)pairM_all - (long long)pairA_all;
    const long long freeM    = (long long)totMat - (long long)pairM_all;
    const long long freeA    = (long long)totAnti - (long long)pairA_all;
    const long long freeD    = freeM - freeA;               // == dTotal - dPair
    const bool retinaOk = (freeD == (dTotal - dPair));
    printf("[charges] retina tick=%u pairM=%llu pairA=%llu dPair=%+lld freeM=%llu freeA=%llu freeD=%+lld | hidAnti orb=%llu umb=%llu form=%llu blob=%llu ann=%llu | Dtot==dPair+freeD: %s\n",
           tick,
           (unsigned long long)pairM_all, (unsigned long long)pairA_all, dPair,
           (unsigned long long)freeM, (unsigned long long)freeA, freeD,
           (unsigned long long)pairAnti[0], (unsigned long long)pairAnti[1],
           (unsigned long long)pairFormations,
           (unsigned long long)blobFormations,
           (unsigned long long)annihilationEvents,
           retinaOk ? "OK" : "FAIL");

    for (unsigned w = 0; w < W_USED; ++w)
      printf("[charges]   w=%u mat=%llu anti=%llu\n",
             w, (unsigned long long)matCount[w], (unsigned long long)antiCount[w]);
    fflush(stdout);
  }
}
