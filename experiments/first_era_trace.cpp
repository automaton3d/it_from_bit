/*
 * first_era_trace.cpp -- headless trace of the FIRST ERA of the model.
 *
 * For each completed light frame of the superposed Platonic seed it prints the shell
 * radius, the attractor census (K, D, S, P halves, unresolved, groups of L/3, distinct
 * centres), the number of distinct charge words over the W source addresses, how many
 * addresses are STRUCTURALLY PAIRABLE (their charge word admits one of R1..R6 against
 * another word present in the lattice), the active-cell count and the shell population.
 *
 * Build (MSVC, repository root, no GUI, no CUDA).  utils.cpp includes <GUI.h>, hence the
 * vcpkg include path; no GUI symbol is called, so nothing from GLFW/GL is linked:
 *
 *   cl /nologo /std:c++20 /O2 /EHsc /MD /DNOMINMAX /Isrc /Isrc/include
 *      /IE:/vcpkg/installed/x64-windows/include
 *      experiments\first_era_trace.cpp
 *      src\config.cpp src\model\initSim.cpp src\model\simulation.cpp
 *      src\model\interaction.cpp src\model\utils.cpp src\model\geometry.cpp
 *      src\model\polarization.cpp src\model\charges.cpp src\model\attractor.cpp
 *      src\model\wavefront.cpp
 *      /Fe:build\first_era_trace.exe /link /SUBSYSTEM:CONSOLE
 *
 * Run from the repository root (automaton.cfg is read from the working directory):
 *   build\first_era_trace.exe <L> <s2b_target> <frames>
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "model/simulation.h"
#include "model/attractor.h"

using namespace automaton;

/* Torus-normalised difference: a centre that crosses the lattice boundary moves by one cell, but the
 * raw coordinate difference is +-(L-1).  Every displacement and every offset in this trace must go
 * through here, or a wrap looks like an off-octant jump (that artefact was found by the pending-
 * impulse dump: all booked impulses were aligned, while the displacements computed here were not). */
static int torusDelta(int now, int prev, unsigned len)
{
  int d = now - prev;
  const int half = (int)len / 2;
  if (d >  half) d -= (int)len;
  if (d < -half) d += (int)len;
  return d;
}

/* GUI-side globals that the model sources reference but a headless run never needs.
 * They live in src/globals.cpp, which pulls the renderer; these stubs let the trace
 * link without the GUI.  Defaults are the ones the GUI starts from. */
std::vector<unsigned int> voxels;
namespace automaton
{
  bool convol_delay  = false;
  bool diffuse_delay = false;
  bool reloc_delay   = false;
}

/* ---- the six pair rules, transcribed from interaction.cpp ------------------- */
static inline unsigned bit_w1(unsigned c) { return (c >> 5) & 1u; }
static inline unsigned bit_w0(unsigned c) { return (c >> 4) & 1u; }
static inline unsigned bit_q(unsigned c)  { return (c >> 3) & 1u; }
static inline unsigned bit_col(unsigned c){ return c & 7u; }

static bool rule1(unsigned a, unsigned b)          /* graviton: six complementary bits */
{ return ((a ^ b) & 0x3Fu) == 0x3Fu; }
static bool rule2(unsigned a, unsigned b)          /* gluon */
{
  return bit_q(a) != bit_q(b) && bit_w1(a) == bit_w1(b) && bit_w0(a) != bit_w0(b)
      && (bit_col(a) ^ bit_col(b)) == 7u;
}
static bool rule3(unsigned a, unsigned b) { return a == 0u    && b == 0u;    } /* neutrino     */
static bool rule4(unsigned a, unsigned b) { return a == 0x3Fu && b == 0x3Fu; } /* antineutrino */
static bool rule5(unsigned a, unsigned b)          /* up quark, w1 = 0 branch */
{
  return bit_q(a) == 0 && bit_q(b) == 0 && bit_w1(a) == 0 && bit_w1(b) == 0
      && bit_w0(a) == 1 && bit_w0(b) == 1 && bit_col(a) == bit_col(b)
      && bit_col(a) != 0 && bit_col(a) != 7;
}
static bool rule6(unsigned a, unsigned b)          /* up quark, w1 = 1 branch */
{
  return bit_q(a) == 1 && bit_q(b) == 1 && bit_w1(a) == 1 && bit_w1(b) == 1
      && bit_w0(a) == 0 && bit_w0(b) == 0 && bit_col(a) == bit_col(b)
      && bit_col(a) != 0 && bit_col(a) != 7;
}
static bool anyRule(unsigned a, unsigned b)
{
  return rule1(a,b) || rule2(a,b) || rule3(a,b) || rule4(a,b) || rule5(a,b) || rule6(a,b);
}

int main(int argc, char** argv)
{
  const int L  = (argc > 1) ? atoi(argv[1]) : 9;
  const int S  = (argc > 2) ? atoi(argv[2]) : 16384;
  const int NF = (argc > 3) ? atoi(argv[3]) : 9;

  loadConfig("automaton.cfg");
  gConfig.simulation.scenario = 0;
  gConfig.simulation.lattice  = L;
  gConfig.simulation.layers   = 3 * L * L;
  gConfig.simulation.subX0 = 0; gConfig.simulation.subX1 = L - 1;
  gConfig.simulation.subY0 = 0; gConfig.simulation.subY1 = L - 1;
  gConfig.simulation.subZ0 = 0; gConfig.simulation.subZ1 = L - 1;
  s2b_target = S;

  /* the GUI's setup screen calls this before the init loop; a cubic region goes through
   * calculateParameters + tryAllocate, i.e. the whole-lattice path the runs use. */
  if (!configureLatticeFromRegion(unsigned(3 * L * L), 0, L - 1, 0, L - 1, 0, L - 1))
  {
    printf("# configureLatticeFromRegion refused L=%d (edge must be odd and >= 5)\n", L);
    return 2;
  }

  int step = 0;
  while (!initSimulation(step++)) { }

  /* Per-cell snapshot for the erosion analysis.  Read-only: the model is untouched. */
  struct Slot { unsigned char act, s2b, kind, ch, pb, sb; int r, fld; };
  std::vector<Slot> prev;
  std::map<unsigned, unsigned> lostWords;       /* charge words of cells that left the shell */
  std::map<int, unsigned>      lostDeltaRF;     /* (r - f) of cells that left the shell     */
  std::map<int, unsigned>      deltaRF_total;   /* (r - f) over the whole lattice, summed   */
  unsigned long long lostTotal = 0, gainTotal = 0, lostS2BTotal = 0;
  unsigned long long nReached = 0;
  std::vector<unsigned> sAct, sLostS2B, sS2BTot, sK, sD, sS, sP;

  printf("# first-era trace  L=%d  W_USED=%u  RMAX=%u  ISLAND_SIZE=%u  CENTER=%u  s2b_target=%d\n",
         L, W_USED, RMAX, ISLAND_SIZE, CENTER, s2b_target);
  printf("# frame  r_clk  active  shell_rf  lost  gain  lost_s2B  lost_K  lost_S  lost_D  lost_P"
         "  s2B_tot   K     D   S   P_halves   dev     dt    t_share\n");
  fflush(stdout);

  attractor::begin();
  for (int frame = 1; frame <= NF; ++frame)
  {
    while (!simulation()) { }                       /* run one complete light frame */

    attractor::sampleFrame(frame);
    const attractor::Report rep = attractor::summarize();

    /* charge words over the W source addresses (source = r2 == 0, one per layer) */
    std::map<unsigned, unsigned> hist;
    unsigned r_src = 0, active = 0, shell_rf = 0, s2b_tot = 0;
    for (unsigned w = 0; w < W_USED; ++w)
      r_src = unsigned(effective_t(getCell(lattice_curr, CENTER, CENTER, CENTER, w).t));
    for (const Cell& c : lattice_curr)
    {
      if (c.active) ++active;
      if (c.s2B)    ++s2b_tot;
      if (c.r >= 0 && int(c.f) == c.r) ++shell_rf;
    }
    for (unsigned w = 0; w < W_USED; ++w)
      ++hist[getCell(lattice_curr, CENTER, CENTER, CENTER, w).ch & 0x3Fu];

    /* the shell predicate the code documents: a cell is on the active shell exactly when
     * its propagated integer radius r equals its breathing phase f.  shell_rf counts that,
     * self-consistently per cell; `active` is the flag the model itself carries. */

    std::vector<Slot> cur(lattice_curr.size());
    for (size_t i = 0; i < lattice_curr.size(); ++i)
    {
      const Cell& c = lattice_curr[i];
      cur[i] = { (unsigned char)(c.active ? 1 : 0), (unsigned char)(c.s2B ? 1 : 0),
                 (unsigned char)c.kind, (unsigned char)(c.ch & 0x3Fu),
                 (unsigned char)(c.pB ? 1 : 0), (unsigned char)(c.sB ? 1 : 0),
                 c.r, int(c.f) };
    }

    unsigned lost = 0, gain = 0, lost_s2b = 0, lostKind[4] = {0,0,0,0};
    if (prev.size() == cur.size())
      for (size_t i = 0; i < cur.size(); ++i)
      {
        if (prev[i].act && !cur[i].act)
        {
          ++lost; lostS2BTotal += prev[i].s2b; lost_s2b += prev[i].s2b;
          if (prev[i].kind < 4) ++lostKind[prev[i].kind];
          ++lostWords[prev[i].ch]; ++lostDeltaRF[prev[i].r - prev[i].fld];
        }
        if (!prev[i].act && cur[i].act) ++gain;
      }
    lostTotal += lost; gainTotal += gain;
    prev = std::move(cur);
    sAct.push_back(active); sLostS2B.push_back(lost_s2b); sS2BTot.push_back(s2b_tot);
    sK.push_back(rep.census.chiefs); sD.push_back(rep.census.delegates);
    sS.push_back(rep.census.singletons); sP.push_back(rep.census.pairs);

    /* whole-lattice diagnostics: the radius field r is set by the BFS, the per-cell
     * breathing phase f by the cell's own clock t.  r - f != 0 means the cell is not on
     * the shell it would occupy if its clock had kept step with its radius; a spread in
     * t means the clocks themselves have desynchronised. */
    unsigned dev = 0, dt = 0, t_share = 0;
    {
      std::map<int, unsigned> dRF;
      std::map<unsigned, unsigned> tHist;
      for (const Cell& c : lattice_curr)
        if (c.r >= 0) { ++dRF[c.r - int(c.f)]; ++tHist[c.t]; if (c.r != int(c.f)) ++dev; }
      dt = unsigned(tHist.size());
      for (const auto& a : tHist) if (a.second > t_share) t_share = a.second;
      for (const auto& a : dRF) deltaRF_total[a.first] += a.second;
      nReached = 0;
      for (const auto& a : tHist) nReached += a.second;
    }

    printf("%5d %5u  %-6u %-9u %-5u %-5u %-9u %-6u %-6u %-6u %-6u %-8u %-4u %-3u %-3u %u"
           "   %-6u %-4u %-6u\n",
           frame, r_src, active, shell_rf, lost, gain, lost_s2b,
           lostKind[0], lostKind[1], lostKind[2], lostKind[3], s2b_tot,
           rep.census.chiefs, rep.census.delegates, rep.census.singletons, rep.census.pairs,
           dev, dt, t_share);
    fflush(stdout);

    /* homB/cB latch of the Encounter stage (interaction.cpp:1566-1586): the paper's
     * dispersion hook.  It fires when a shell cell with pB or sB (every third layer)
     * sees effective_t == RMAX/2, i.e. at radius floor(RMAX/2).  homb_events is the
     * model's own counter, cumulative. */
    {
      static long long pHomb = 0;
      printf("#   frame %d encounter latch: homb_events=+%lld (cumulative %lld)\n",
             frame, homb_events - pHomb, homb_events);
      pHomb = homb_events;
      fflush(stdout);
    }

    /* Physical separation: is the relocation vector c used?  occupiedCenters = distinct
     * cells hosting a source (the seed puts all W of them in ONE cell); kB gates the SLOT IV
     * c-write (interaction.cpp:2260), cB/homB are the homing pair, reloc is what the RELOC
     * stage consumes and applies.  moved = sources whose centre changed this frame. */
    {
      unsigned cB_n = 0, kB_n = 0, homB_n = 0, reloc_n = 0, moved = 0, src = 0,
               m_src = 0, m_any = 0, pol_n = 0;
      for (const Cell& c : lattice_curr)
      {
        if (c.cB) ++cB_n;
        if (c.kB) ++kB_n;
        if (c.homB) ++homB_n;
        if (c.m[0] || c.m[1] || c.m[2]) ++m_any;
        if (c.pol_u || c.pol_v) ++pol_n;
        if (c.r2 == 0)
        {
          ++src;
          if (c.reloc[0] || c.reloc[1] || c.reloc[2]) ++reloc_n;
          if (c.m[0] || c.m[1] || c.m[2]) ++m_src;
        }
      }
      static std::vector<int> prevX;
      static std::vector<char> seen;
      if (prevX.size() != 3u * W_USED) { prevX.assign(3u * W_USED, -1); seen.assign(W_USED, 0); }
      /* Movement alignment: of the layers whose centre moved this frame, how many moved along
         their own charge octant (all three signs) or against it (none).  m is measured to be
         charge-aligned (axis-align line below), so this counter says whether the channel that
         actually displaces the layers is charge-correlated or stirs them at random.  Computed
         here, before prevX is refreshed.

         From the cohesion pass on, the two channels are reported separately: a layer flagged by
         s2bTraceCohesionFlag received a cohesion step (the single-axis face step of
         resolveInternalContacts, axial by design), everything else is flight (the charge
         dispersal, the own-axis exchange thrust, the pair walk).  A layer can receive both in one
         frame; the flag then wins, so the cohesion column is an upper bound. */
      unsigned movedN = 0, mvAlign[4] = {0,0,0,0}, cohN = 0, cohAlign[4] = {0,0,0,0},
               flightN = 0, flightAlign[4] = {0,0,0,0};
      unsigned singleN = 0, singleAl = 0, multiN = 0, multiAl = 0;
      /* Drift test (prediction: the walk uses shortestDelta on the torus, so once a layer's centre
         has crossed the lattice centre in an axis -- its offset there no longer has the octant's
         sign -- the shortest offset to a same-octant partner picks the other way round and the
         step runs against the octant).  `pm` counts the axes in which the layer is still on its
         octant's side; the two buckets below report the flight alignment for pm == 3 and pm < 3. */
      unsigned onSideN = 0, onSideAligned = 0, offSideN = 0, offSideAligned = 0;
      for (const Cell& c : lattice_curr)
        if (c.r2 == 0 && c.w < W_USED)
        {
          const bool known = seen[c.w] != 0;
          const int dx = known ? torusDelta((int)c.x[0], prevX[3u * c.w],     ELX) : 0;
          const int dy = known ? torusDelta((int)c.x[1], prevX[3u * c.w + 1], ELY) : 0;
          const int dz = known ? torusDelta((int)c.x[2], prevX[3u * c.w + 2], ELZ) : 0;
          if (known && (dx || dy || dz))
          {
            ++moved;
            const unsigned ch6 = c.ch & 0x3Fu;
            unsigned a = 0;
            if (dx * ((ch6 & 4u) ? +1 : -1) > 0) ++a;
            if (dy * ((ch6 & 2u) ? +1 : -1) > 0) ++a;
            if (dz * ((ch6 & 1u) ? +1 : -1) > 0) ++a;
            ++mvAlign[a]; ++movedN;
#ifndef S2B_TRACE
            ++flightAlign[a]; ++flightN;   // no flags compiled: everything is flight
#else
            /* Writer-mask bucket: how many DISTINCT movers touched this layer in one frame.  The
               drain bit (64) is the transport of a booking, not an independent mover, so it is
               excluded here -- with it included the cohesion path counted twice (its own booking
               plus its drain) and looked like a mixture.  Bits counted: 1 walk funnel, 2 relay, 4
               relay contact, 8 cohesion, 16 charge dispersion, 32 own-axis thrust. */
            unsigned bits = 0;
            if (c.w < s2bTraceWriterMask.size())
            {
              unsigned m = s2bTraceWriterMask[c.w] & 0x3Fu;
              for (unsigned b = 0; b < 6u; ++b) if (m & (1u << b)) ++bits;
            }
            if (bits <= 1u) { ++singleN; if (a == 3u) ++singleAl; }
            else            { ++multiN;  if (a == 3u) ++multiAl; }
            if (c.w < s2bTraceCohesionFlag.size() && s2bTraceCohesionFlag[c.w])
            { ++cohAlign[a]; ++cohN; }
            else
            {
              ++flightAlign[a]; ++flightN;
              unsigned pm = 0;
              if (((int)c.x[0] - (int)CENTER) * ((ch6 & 4u) ? +1 : -1) > 0) ++pm;
              if (((int)c.x[1] - (int)CENTER) * ((ch6 & 2u) ? +1 : -1) > 0) ++pm;
              if (((int)c.x[2] - (int)CENTER) * ((ch6 & 1u) ? +1 : -1) > 0) ++pm;
              if (pm == 3u) { ++onSideN; if (a == 3u) ++onSideAligned; }
              else          { ++offSideN; if (a == 3u) ++offSideAligned; }
            }
#endif
          }
          prevX[3u * c.w] = c.x[0]; prevX[3u * c.w + 1] = c.x[1]; prevX[3u * c.w + 2] = c.x[2];
          seen[c.w] = 1;
        }
      printf("#   frame %d move-align: moved=%u  sign-match 0/1/2/3 = %u/%u/%u/%u\n",
             frame, movedN, mvAlign[0], mvAlign[1], mvAlign[2], mvAlign[3]);
      printf("#   frame %d move-split: flight=%u  0/1/2/3 = %u/%u/%u/%u  |  cohesion=%u"
             "  0/1/2/3 = %u/%u/%u/%u\n",
             frame, flightN, flightAlign[0], flightAlign[1], flightAlign[2], flightAlign[3],
             cohN, cohAlign[0], cohAlign[1], cohAlign[2], cohAlign[3]);
      printf("#   frame %d move-drift: flight on its octant side (all 3 axes) = %u, octant-aligned"
             " = %u  |  off side (<3 axes) = %u, octant-aligned = %u\n",
             frame, onSideN, onSideAligned, offSideN, offSideAligned);
      printf("#   frame %d move-mask: one writer = %u (octant-aligned %u)  |  two or more writers"
             " = %u (octant-aligned %u)\n",
             frame, singleN, singleAl, multiN, multiAl);
#ifdef S2B_TRACE
      /* The mask is per frame: clear it now that this frame has been read, so the next frame's
         writers start from zero.  (The model's writers only set bits.) */
      for (unsigned w = 0; w < s2bTraceWriterMask.size(); ++w) s2bTraceWriterMask[w] = 0u;
#endif
      fflush(stdout);
      printf("#   frame %d separation: occupiedCenters=%u sources=%u moved-this-frame=%u"
             "  cB=%u kB=%u homB=%u reloc!=0(sources)=%u  m!=0(sources)=%u/%u m!=0(cells)=%u"
             "  pol!=0(cells)=%u\n",
             frame, rep.census.occupiedCenters, src, moved, cB_n, kB_n, homB_n, reloc_n,
             m_src, src, m_any, pol_n);
      fflush(stdout);

      /* Acceptance observable for the charge-seeded dispersion: the sector is w1 (bit 5),
         so if the octant rule runs, the Orbis group (w1 = 0) drifts to -y and the Umbra
         group (w1 = 1) to +y, while the global centre of mass stays at the lattice centre. */
      {
        double ox = 0, oy = 0, oz = 0, ux = 0, uy = 0, uz = 0, cx2 = 0, cy2 = 0, cz2 = 0;
        unsigned no = 0, nu = 0;
        for (const Cell& c : lattice_curr)
          if (c.r2 == 0)
          {
            const bool umbra = ((c.ch >> 5) & 1u) != 0u;
            // Offsets relative to the lattice centre, torus-normalised: a wrapped coordinate would
            // otherwise bias every mean and the centre of mass (see torusDelta).
            const double dx0 = double(torusDelta((int)c.x[0], (int)CENTER, ELX));
            const double dy0 = double(torusDelta((int)c.x[1], (int)CENTER, ELY));
            const double dz0 = double(torusDelta((int)c.x[2], (int)CENTER, ELZ));
            if (umbra) { ux += dx0; uy += dy0; uz += dz0; ++nu; }
            else       { ox += dx0; oy += dy0; oz += dz0; ++no; }
            cx2 += dx0; cy2 += dy0; cz2 += dz0;
          }
        if (no) { ox /= no; oy /= no; oz /= no; }
        if (nu) { ux /= nu; uy /= nu; uz /= nu; }
        const double ns = double(no + nu);
        if (ns > 0) { cx2 /= ns; cy2 /= ns; cz2 /= ns; }
        printf("#   frame %d positions: Orbis n=%u mean=(%.3f,%.3f,%.3f) | Umbra n=%u"
               " mean=(%.3f,%.3f,%.3f) | CoM-(centre)=(%.3f,%.3f,%.3f)\n",
               frame, no, ox, oy, oz, nu, ux, uy, uz, cx2, cy2, cz2);
        fflush(stdout);
      }

      /* Per-class displacement: the eight colour classes are island mod 8 (the seed's own
         family map), so this line is the displacement spectrum a magnitude rule produces.
         With a unit step every class sits at distance 1 (its octant); a magnitude splits
         them into shells. */
      {
        double sx[8] = {0,0,0,0,0,0,0,0}, sy[8] = {0,0,0,0,0,0,0,0},
               sz[8] = {0,0,0,0,0,0,0,0};
        unsigned cn[8] = {0,0,0,0,0,0,0,0};
        for (const Cell& c : lattice_curr)
          if (c.r2 == 0 && ISLAND_SIZE > 0u)
          {
            const unsigned cls = (c.w / ISLAND_SIZE) % 8u;
            sx[cls] += c.x[0]; sy[cls] += c.x[1]; sz[cls] += c.x[2]; ++cn[cls];
          }
        const double C2 = double(CENTER);
        printf("#   frame %d classes:", frame);
        for (unsigned k = 0; k < 8; ++k)
          if (cn[k])
            printf("  k%u(n=%u)d=(%+.2f,%+.2f,%+.2f)", k, cn[k],
                   sx[k] / cn[k] - C2, sy[k] / cn[k] - C2, sz[k] / cn[k] - C2);
        printf("\n");
        fflush(stdout);
      }

      /* Axis alignment: how many of the three signs of a layer's momentum agree with the
         octant of its own charge word, over the W layer centres.  This decides whether m is
         charge-correlated (3 = parallel to the octant, 0 = antiparallel) or blind to the
         charge (2/1 = mixed).  Also the number of layers with m == 0. */
      {
        unsigned agree[4] = {0,0,0,0}, noM = 0;
        for (unsigned w = 0; w < W_USED; ++w)
        {
          const Cell& c = getCell(lattice_curr, lcenters[w][0], lcenters[w][1], lcenters[w][2], w);
          const unsigned ch6 = c.ch & 0x3Fu;
          if (!c.m[0] && !c.m[1] && !c.m[2]) { ++noM; continue; }
          const int ox = (ch6 & 4u) ? +1 : -1;
          const int oy = (ch6 & 2u) ? +1 : -1;
          const int oz = (ch6 & 1u) ? +1 : -1;
          unsigned a = 0;
          if (c.m[0] * ox > 0) ++a;
          if (c.m[1] * oy > 0) ++a;
          if (c.m[2] * oz > 0) ++a;
          ++agree[a];
        }
        printf("#   frame %d axis-align: m==0=%u  sign-match 0/1/2/3 = %u/%u/%u/%u\n",
               frame, noM, agree[0], agree[1], agree[2], agree[3]);
        fflush(stdout);
      }
    }

#ifdef S2B_TRACE
    /* in-loop s2B/clock counters: cumulative, so the frame line carries the deltas */
    static unsigned long long pFired = 0, pTChanged = 0, pTOff = 0, pActOff = 0,
                              pActLost = 0, pReset = 0, pReemit = 0, pCB = 0,
                              pFloodPull = 0, pFloodReset = 0, pImpulse = 0,
                              pSeen = 0, pApplied = 0,
                              pCommitPending = 0, pBooked = 0, pRelocSum = 0,
                              pTicks = 0, pCaptured = 0, pDrained = 0, pCommitCalls = 0, pWriteback = 0,
                              pNetLayers = 0, pChargeDisp = 0, pPolarSeed = 0,
                              pReseat = 0, pReseatAl = 0, pReseatOther = 0, pReseatContact = 0,
                              pReseed = 0;
    printf("#   frame %d clocks: resets=%llu  [reemit=%llu (non-zero impulse=%llu)  reloc-at-centre=%llu applied=%llu  cB=%llu  flood-to-0=%llu]"
           "  flood-pulls=%llu | commit: pending-impulse=%llu, |reloc| at commit=%llu"
           " | booked-on-lattice=%llu drained=%llu writeback-sum=%llu net-layers=%llu net-max=%llu charge-dispersion=%llu polar-seed=%llu commit-calls=%llu | ticks=%llu captured-sum=%llu"
           " | s2B fired in pass=%llu (t-off=%llu, left-active=%llu)  cells-left-active=%llu"
           " | relay: reseat-step=%llu (octant-aligned=%llu other=%llu) reseat-at-contact=%llu"
           " | reseed-carried-reloc=%llu\n",
           frame,
           s2bTraceClockReset - pReset, s2bTraceReemitResets - pReemit,
           s2bTraceReemitImpulse - pImpulse,
           s2bTraceRelocSeen - pSeen, s2bTraceRelocApplied - pApplied,
           s2bTraceCBResets - pCB, s2bTraceFloodResets - pFloodReset,
           s2bTraceFloodPulls - pFloodPull,
           s2bTraceCommitPending - pCommitPending,
           s2bTraceRelocSumAtCommit - pRelocSum,
           s2bTraceImpulseBooked - pBooked,
           s2bTraceImpulseDrained - pDrained, s2bTraceDrainWriteback - pWriteback,
           s2bTraceNetLayers - pNetLayers, s2bTraceNetMax,
           s2bTraceChargeDispersion - pChargeDisp,
           s2bTracePolarSeed - pPolarSeed,
           s2bTraceCommitCalls - pCommitCalls,
           s2bTraceTicks - pTicks, s2bTraceReapplySum - pCaptured,
           s2bTraceFired - pFired, s2bTraceFiredTOff - pTOff, s2bTraceFiredActOff - pActOff,
           s2bTraceActLost - pActLost,
           s2bTraceReseatSteps - pReseat, s2bTraceReseatAligned - pReseatAl,
           s2bTraceReseatOther - pReseatOther, s2bTraceReseatAtContact - pReseatContact,
           s2bTraceReseedCarried - pReseed);
    pFired = s2bTraceFired; pTChanged = s2bTraceFiredTChanged; pTOff = s2bTraceFiredTOff;
    pActOff = s2bTraceFiredActOff; pActLost = s2bTraceActLost; pReset = s2bTraceClockReset;
    pReemit = s2bTraceReemitResets; pCB = s2bTraceCBResets; pImpulse = s2bTraceReemitImpulse;
    pSeen = s2bTraceRelocSeen; pApplied = s2bTraceRelocApplied;
    pCommitPending = s2bTraceCommitPending;
    pBooked = s2bTraceImpulseBooked; pRelocSum = s2bTraceRelocSumAtCommit;
    pDrained = s2bTraceImpulseDrained; pCommitCalls = s2bTraceCommitCalls;
    pWriteback = s2bTraceDrainWriteback;
    pNetLayers = s2bTraceNetLayers;
    pChargeDisp = s2bTraceChargeDispersion;
    pPolarSeed = s2bTracePolarSeed;
    pReseat = s2bTraceReseatSteps; pReseatAl = s2bTraceReseatAligned;
    pReseatOther = s2bTraceReseatOther; pReseatContact = s2bTraceReseatAtContact;
    pReseed = s2bTraceReseedCarried;
    pTicks = s2bTraceTicks; pCaptured = s2bTraceReapplySum;
    pFloodPull = s2bTraceFloodPulls; pFloodReset = s2bTraceFloodResets;
    fflush(stdout);
#endif

    if (frame <= 2)
    {
      printf("#   frame %d charge words (word : addresses):", frame);
      for (const auto& a : hist) printf("  %06u:%u", a.first, a.second);
      printf("\n");
      fflush(stdout);
    }

    if (frame == 1)
    {
      /* addresses whose word admits a rule against another word present in the lattice */
      unsigned pairable = 0;
      for (const auto& a : hist)
      {
        bool ok = false;
        for (const auto& b : hist)
        {
          if (b.first == a.first) ok = ok || (b.second > 1u && anyRule(a.first, b.first));
          else                    ok = ok || anyRule(a.first, b.first) || anyRule(b.first, a.first);
        }
        if (ok) pairable += a.second;
      }
      printf("#   distinct words=%zu  addresses that admit a pair rule=%u\n", hist.size(), pairable);
      fflush(stdout);
    }
  }

  /* period detection: the smallest p such that the recorded series repeats with period p,
   * tested over the second half of the run (so the seed transient is skipped). */
  {
    const size_t n = sAct.size();
    const size_t span = (n > 0) ? n - n / 2 : 0;   /* frames available for the test */
    auto periodOf = [&](const std::vector<const std::vector<unsigned>*>& vs, const char* name)
    {
      /* at least TWO full periods must fit in the window, otherwise the comparison loop
       * is empty and the test would pass vacuously. */
      for (size_t p = 1; 2 * p <= span; ++p)
      {
        bool ok = true;
        for (size_t i = n / 2; i + p < n && ok; ++i)
          for (const std::vector<unsigned>* v : vs)
            if ((*v)[i] != (*v)[i + p]) { ok = false; break; }
        if (ok) { printf("# period of %-34s : %zu frames\n", name, p); return; }
      }
      printf("# period of %-34s : none up to %zu frames\n", name, span / 2);
    };
    periodOf({ &sAct }, "active-shell size");
    periodOf({ &sAct, &sS2BTot }, "shell size + s2B occupancy");
    periodOf({ &sAct, &sLostS2B }, "shell size + s2B departures");
    periodOf({ &sAct, &sS2BTot, &sLostS2B, &sK, &sD, &sS, &sP }, "the full recorded state");
    printf("# series (frame >= %zu) active / lost_s2B / s2B_tot:", n / 2);
    for (size_t i = n / 2; i < n; ++i)
      printf("  %zu:%u/%u/%u", i + 1, sAct[i], sLostS2B[i], sS2BTot[i]);
    printf("\n");
  }

  printf("# totals: lost=%llu gained=%llu  of the lost cells %llu carried s2B\n",
         lostTotal, gainTotal, lostS2BTotal);
  printf("#   whole-lattice (r - f) summed over the run (r-f : cell-frames):");
  for (const auto& a : deltaRF_total)
    printf("  %d:%llu", a.first, (unsigned long long)a.second);
  printf("\n#   charge words of cells that left the shell (word : count):");
  for (const auto& a : lostWords) printf("  %06u:%u", a.first, a.second);
  printf("\n#   (r - f) of cells that left the shell:");
  for (const auto& a : lostDeltaRF) printf("  %d:%u", a.first, a.second);
  printf("\n");
  return 0;
}
