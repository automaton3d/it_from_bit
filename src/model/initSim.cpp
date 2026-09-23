/*
 * initSim.cpp
 *
 * Gather all initialization routines.
 */

#include "model/simulation.h"
#include "model/polarization.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cassert>
#include "globals.h"
#include "layers.h"

namespace automaton
{
  using namespace std;

  // Global variables for lattice
  extern std::vector<Cell> lattice_curr;
  extern std::vector<Cell> lattice_draft;
  extern std::vector<Cell> lattice_partner;

  extern std::vector<std::array<unsigned, 3>> lcenters;

  inline size_t index(unsigned x, unsigned y, unsigned z, unsigned w)
  {
    return (((size_t)w * ((size_t)ELY * ELZ) + x) * ELY + y) * ELZ + z;
  }

  /**
   * Function to initialize the lattice with general data.
   */
void initGeneral()
{
    printf("initGeneral: EL=%u, RMAX=%u\n", EL, RMAX);

    // Reset pulsating sphere tick counter
    pulse_tick = 0;

    // Fatia 1: zero the charge-census / virgin-wrap ledgers.
    chargesReset();

    // Reset emergent polarization broadcast state (walkers, elected axes).
    polarization::resetAll();
    resetSourceTransactions();
    
    for (unsigned w = 0; w < W_USED; ++w)
    {
        // Layer-specific center
        unsigned cx = lcenters[w][0];
        unsigned cy = lcenters[w][1];
        unsigned cz = lcenters[w][2];
        
        printf("  Layer %u: center=(%u,%u,%u)\n", w, cx, cy, cz);
        
        for (unsigned x = 0; x < ELX; ++x)
        {
            for (unsigned y = 0; y < ELY; ++y)
            {
                for (unsigned z = 0; z < ELZ; ++z)
                {
                    Cell& cell = getCell(lattice_curr, x, y, z, w);
                    // Reinitialization must discard the previous run's clocks and flags.
                    cell = Cell{};

                    // Basic configuration
                    cell.w = static_cast<WIndex>(w);
                    cell.is_core = false;

                    unsigned island = islandOf(cell.w);
                    WIndex chiefW   = firstWOfIsland(island);

                    char w0 = island % 2;
                    char w1 = (island >> 1) % 2;
                    char q = w0 ^ w1;
                    
                    cell.ch = (island % 8) | (q << 3) | (w0 << 4) | (w1 << 5);
                    cell.x[0] = x;
                    cell.x[1] = y;
                    cell.x[2] = z;
                    cell.x[3] = w;
                    
                    // Static W-island identity, independent of spatial geometry.
                    // This address does not assign the dynamical chief role K.
                    cell.leader_w = chiefW;
                    cell.a = (unsigned)chiefW;

                    // Platonic seed: only the source knows its radius. All other
                    // distances must be reached by update_pulsating_wavefront().
                    const bool source = (x == cx && y == cy && z == cz);
                    cell.r2 = source ? 0u : INF_R2;
                    cell.r = source ? 0 : -1;
                    cell.u = source ? 2048 : 0;
                    cell.v = 0;
                    cell.active = 0;

                    // Emergent polarization broadcast state
                    cell.bstamp = 0;
                    cell.pol_u  = 0;
                    cell.pol_v  = 0;

                    // Initialize flags
                    cell.pB = false;
                    cell.sB = false;
                    cell.phiB = false;
                    cell.t = 0;
                    cell.f = 0;
                    cell.s2B = false;
                    cell.kB = false;
                    cell.bB = false;
                    cell.homB = false;
                    cell.cB = false;
                    cell.c[0] = 0;
                    cell.c[1] = 0;
                    cell.c[2] = 0;

                    // No chief is selected by the static W address at birth.
                    cell.kind       = SourceKind::S;
                    cell.parent     = NO_PARENT;
                    cell.spin_target= 0;
                    cell.pair_idx   = NO_PAIR;
                    cell.pair_count = 0;

                    // Momentum vector m and consumable relocation impulse reloc.
                    // Static init always leaves m null; a non-zero m is written only by
                    // the dynamic election path (polarization::elect / installAxis at
                    // t == RMAX), which installs a vector of modulus RMAX = L/2.
                    // The inertia mechanism treats m as immutable and only reads it to
                    // accumulate impulses into reloc (see encounter P×K / P×D).
                    cell.m[0] = cell.m[1] = cell.m[2] = 0;
                    cell.reloc[0] = cell.reloc[1] = cell.reloc[2] = 0;
                }
            }
        }
    }
    
    puts("initGeneral ok.");
}

  /*
   * Replicate data to draft and partner
   */
  void replicate()
  {
    std::copy(lattice_curr.begin(), lattice_curr.begin() + BLOCK, lattice_draft.begin());
    std::copy(lattice_curr.begin(), lattice_curr.begin() + BLOCK, lattice_partner.begin());
    puts("replicate ok.");
  }

  /**
   * Executes initialization steps
   */
  bool initSimulation(int step)
  {
    switch(step)
    {
      case 0:
        initGeneral();
        break;
        
      case 1:
      case 2:
      case 3:
        // Deprecated: static momentum/spiral/sine initialisation removed;
        // polarisation and active wavefront now emerge from phase_step().
        break;
        
      case 4:
        printParams();
        break;
        
      case 5:
        // Previously used for debug topological relocation; removed.
        break;
        
      case 6:
        replicate();
        break;
        
      case 7:
        assert(sanityTest());
        break;
        
      default:
        return true;
    }
    
    return false;
  }

  /**
   * Tentative allocation
   */
  bool tryAllocate(int EL, int W)
  {
    try
    {
      size_t total = static_cast<size_t>(EL) * EL * EL * W;
      
      lattice_curr.resize(total);
      lattice_draft.resize(total);
      lattice_partner.resize(total);
      
      const size_t totalVoxels = static_cast<size_t>(EL) * EL * EL;
      
      if (voxels.size() != totalVoxels)
        voxels.resize(totalVoxels);
      
      printf("tryAllocate: allocated %zu cells (%u^3 * %u)\n", total, EL, W);
      
      return true;
    }
    catch (const std::bad_alloc& e)
    {
      lastAllocationError = "Memory allocation failed: " + std::string(e.what());
      std::cerr << lastAllocationError << std::endl;
      return false;
    }
    catch (...)
    {
      lastAllocationError = "Unknown error during memory allocation";
      std::cerr << lastAllocationError << std::endl;
      return false;
    }
  }

  /**
   * Region-driven configuration (the setup screen's "subregion of the lattice").
   *
   * The region's EXTENTS become the lattice edges.  Its position inside the
   * L x L x L grading is deliberately not used: the lattice is periodic and the
   * initial condition is translation-invariant (every bubble is born at the
   * lattice centre -- see initCenters), so a translated region is the same
   * experiment on the same torus.  What the region changes, and the only thing
   * the memory bill depends on, is the size.
   *
   * A cubic region goes through the legacy cubic path (calculateParameters +
   * tryAllocate), so selecting the whole lattice is bit-identical to the way the
   * runs were configured before.  A non-cubic region takes the anisotropic path
   * the model already had (tryAllocateTube: per-axis edges, RMAX = short side,
   * schedule scale = long edge).
   *
   * Bounds are inclusive cell indices, in the same L-grading the setup screen
   * draws.  Returns false (with lastAllocationError set) when the region cannot
   * be a lattice: an even edge would break the centre/parity assumptions the
   * seed and the sieve algebra rely on, and an edge below 5 cannot hold a shell.
   */
  bool configureLatticeFromRegion(unsigned W, int x0, int x1, int y0, int y1, int z0, int z1)
  {
    auto refuse = [](const char* why) -> bool
    {
      lastAllocationError = std::string("region: ") + why;
      std::cerr << lastAllocationError << std::endl;
      return false;
    };

    const int dx = x1 - x0 + 1;
    const int dy = y1 - y0 + 1;
    const int dz = z1 - z0 + 1;

    if (dx < 1 || dy < 1 || dz < 1)
      return refuse("empty region.");
    if (W < 1)
      return refuse("W must be >= 1.");
    if ((dx & 1) == 0 || (dy & 1) == 0 || (dz & 1) == 0)
      return refuse("every region edge must be odd (move a face by two cells).");
    if (dx < 5 || dy < 5 || dz < 5)
      return refuse("every region edge must be >= 5.");

    printf("configureLatticeFromRegion: region %d x %d x %d (from x %d..%d y %d..%d z %d..%d), W = %u\n",
           dx, dy, dz, x0, x1, y0, y1, z0, z1, W);

    if (dx == dy && dy == dz)
    {
      // Cubic: the legacy path, cell for cell.
      calculateParameters((unsigned)dx, W);
      return tryAllocate(dx, (int)W);
    }

    // Anisotropic: tryAllocateTube sets the per-axis parameters and allocates.
    return tryAllocateTube((unsigned)dx, (unsigned)dy, (unsigned)dz, W);
  }

  void initCenters(unsigned wDim);

  /**
   * Rectangular-tube allocator (S1 slice 1, Track B release).
   *
   * Equal edges (cube) delegate to tryAllocate so the default behaviour is
   * bit-identical.  Unequal edges now configure the full anisotropic
   * geometry (per-axis edges, RMAX = short side, schedule scale = long
   * edge) and allocate the rectangular volume.  The per-axis
   * indexing/wrap/distance/motion slices required for a trustworthy run
   * are in place (see gravity_probe_DESIGN Slice 1..5).
   */
  bool tryAllocateTube(unsigned LX, unsigned LY, unsigned LZ, unsigned W)
  {
    if (LX == LY && LY == LZ)
    {
      ELX = ELY = ELZ = LX;
      return tryAllocate((int)LX, (int)W);
    }

    auto refuse = [&](const char* why) -> bool
    {
      lastAllocationError = "tube: " + std::string(why);
      std::cerr << lastAllocationError << std::endl;
      return false;
    };

    // Odd edges keep the legacy centre/periodic assumptions (CENTER,
    // source parity, sieve algebra); short sides must fit a shell.
    if (LX < 5 || LY < 5 || LZ < 5)
      return refuse("every edge must be >= 5.");
    if ((LX & 1u) == 0u || (LY & 1u) == 0u || (LZ & 1u) == 0u)
      return refuse("every edge must be odd.");
    if (W < 2)
      return refuse("tube runs use the small-W (W >= 2) path.");

    const size_t plane   = (size_t)LY * LZ;        // one yz slice
    const size_t spatial = plane * LX;             // one layer volume
    const size_t total   = spatial * W;            // full 4D block
    if (total > 0xFFFFFFFFull || plane > 0xFFFFFFFFull)
      return refuse("edges too large for the 32-bit block/slot fields.");

    EL        = LX;                       // legacy alias: schedule scale = long edge
    ELX       = LX;
    ELY       = LY;
    ELZ       = LZ;
    L2        = (unsigned)plane;
    W_DIM     = (3 * L2 + 1);             // canonical W dimension (unused for small W)
    W_USED    = W;
    L3        = (unsigned)spatial;
    ORDER     = (unsigned)round(log2((double)LX));

    // Centres: source seeds / helix starts at the short-side centre, which
    // lies strictly inside every axis.  Explicit bubble placement (the
    // two-bubble harness) overwrites lcenters afterwards anyway.
    const unsigned S = (LX < LY) ? ((LX < LZ) ? LX : LZ)
                                 : ((LY < LZ) ? LY : LZ);
    CENTER    = (S - 1) / 2;
    FCENTER   = (S / 2);

    BLOCK     = (unsigned long)total;
    DIAG      = (unsigned)(EL * (unsigned)sqrt(3.0));

    RMAX      = ((LY < LZ) ? LY : LZ) / 2;    // short-side shell bound

    CONTRACT  = static_cast<int>(floor(sqrt(3.0) * CENTER));
    ENCOUNTER = W_USED;

    GSLOT_X   = ENCOUNTER + 2 * RMAX;
    GSLOT_Y   = GSLOT_X + 2 * RMAX;
    GSLOT_Z   = GSLOT_Y + 2 * RMAX;

    SLOT1     = GSLOT_Z + RMAX;
    SLOT2     = SLOT1 + 3 * (EL - 1);
    SLOT3     = SLOT2 + 3 * (EL - 1);
    SLOT4     = SLOT3 + 2 * W_USED;
    SLOT5     = SLOT4 + 3 * (EL - 1);

    DIFFUSION = SLOT5;

    SLOT6     = DIFFUSION + (EL - 1);
    SLOT7     = SLOT6 + (EL - 1);
    SLOT8     = SLOT7 + (EL - 1);

    RELOC     = SLOT8;
    REISSUE   = RELOC + 1;
    FLOOD     = REISSUE + 3 * (EL - 1);

    FRAME     = FLOOD;

    // W-island topology stays canonical (W = 3L^2): tube runs use the
    // small-W path and never derive W from the edges.
    ISLAND_COUNT = 9 * EL;
    ISLAND_SIZE  = (ISLAND_COUNT > 0) ? (W_USED / ISLAND_COUNT) : 0;
    if (ISLAND_SIZE == 0) ISLAND_SIZE = 1;

    printf("calculateParametersTube: ELX=%u ELY=%u ELZ=%u W_USED=%u RMAX=%u CENTER=%u FRAME=%u\n",
           ELX, ELY, ELZ, W_USED, RMAX, CENTER, FRAME);

    initCenters(W_USED);

    try
    {
      lattice_curr.resize(total);
      lattice_draft.resize(total);
      lattice_partner.resize(total);

      const size_t totalVoxels = spatial;

      if (voxels.size() != totalVoxels)
        voxels.resize(totalVoxels);

      printf("tryAllocateTube: allocated %zu cells (%u x %u x %u * %u)\n",
             total, ELX, ELY, ELZ, W_USED);

      return true;
    }
    catch (const std::bad_alloc& e)
    {
      lastAllocationError = "Memory allocation failed: " + std::string(e.what());
      std::cerr << lastAllocationError << std::endl;
      return false;
    }
    catch (...)
    {
      lastAllocationError = "Unknown error during memory allocation";
      std::cerr << lastAllocationError << std::endl;
      return false;
    }
  }

void initCenters(unsigned wDim)
{
    lcenters.resize(wDim);

    // Platonic seed premise: every bubble is born at the lattice centre with
    // zero radius.  All source centers therefore start superposed at
    // (CENTER, CENTER, CENTER); they separate only later through
    // interaction-driven relocation (applyMomentum), never at birth.
#ifdef PLACED_FAMILY_SEED
    // PREPARED SEED -- visualisation of the QUANTISED ISLANDS.
    //
    // The islands the model itself predicts are the ISLAND_COUNT = 9*EL groups
    // of ISLAND_SIZE = W/(9*EL) = L/3 layers (initSim.cpp:322-324), whose chief
    // is by convention the first layer of the group
    // (isIslandChief(w) = (w % ISLAND_SIZE == 0), simulation.h:405).  This seed
    // places every island at its OWN site, on a flat EL x EL grid of distinct
    // locations, so the quantised islands can be seen directly: one point per
    // island, at rest, each at a different location.
    //
    // It is a PREPARED initial condition (like the inertia fixtures): it does not
    // assert that the canonical superposed seed separates by itself.  The island
    // size is recomputed here from the model's own formula so the seed does not
    // depend on the order in which calculateParameters() ran.
    const unsigned islSize = (9u * EL > 0u) ? (W_USED / (9u * EL)) : 1u;
    for (unsigned w = 0; w < wDim; ++w)
    {
        const unsigned isl = (islSize > 0u) ? (w / islSize) : 0u;
        const unsigned gx  = (EL > 0u) ? (isl % EL) : 0u;
        const unsigned gy  = (EL > 0u) ? ((isl / EL) % EL) : 0u;
        lcenters[w][0] = gx;
        lcenters[w][1] = gy;
        lcenters[w][2] = CENTER;

        printf("initCenters(placed): w=%u, island=%u, center=(%u,%u,%u)\n",
               w, isl, lcenters[w][0], lcenters[w][1], lcenters[w][2]);
    }
#else
    for (unsigned w = 0; w < wDim; ++w)
    {
        lcenters[w][0] = CENTER;
        lcenters[w][1] = CENTER;
        lcenters[w][2] = CENTER;

        printf("initCenters: w=%u, center=(%u,%u,%u)\n",
               w, lcenters[w][0], lcenters[w][1], lcenters[w][2]);
    }
#endif
}

  /**
   * Calculates parameters
   */
  void calculateParameters(unsigned L, unsigned W)
  {
    EL        = L;
    ELX = ELY = ELZ = L;   // cubic by default; tube sets them separately
    L2        = (EL * EL);
    W_DIM     = (3 * L2 + 1);
    W_USED    = W;
    L3        = L2 * EL;
    ORDER     = ((int)round(log2(EL)));
    CENTER    = ((EL - 1) / 2);
    FCENTER   = (EL / 2.0);
    BLOCK     = L3 * W_USED;
    DIAG      = (unsigned)EL * (unsigned)sqrt(3);
    
    RMAX      = L / 2;
    
    CONTRACT  = static_cast<int>(floor(sqrt(3.0) * CENTER));
    ENCOUNTER    = W_USED;
    
    GSLOT_X   = ENCOUNTER + 2 * RMAX;
    GSLOT_Y   = GSLOT_X + 2 * RMAX;
    GSLOT_Z   = GSLOT_Y + 2 * RMAX;
    
    SLOT1     = GSLOT_Z + RMAX;
    SLOT2     = SLOT1 + 3 * (EL - 1);
    SLOT3     = SLOT2 + 3 * (EL - 1);
    SLOT4     = SLOT3 + 2 * W_USED;
    SLOT5     = SLOT4 + 3 * (L - 1);
    
    DIFFUSION = SLOT5;
    
    SLOT6     = DIFFUSION + (EL - 1);
    SLOT7     = SLOT6 + (EL - 1);
    SLOT8     = SLOT7 + (EL - 1);
    
    RELOC     = SLOT8;
    REISSUE   = RELOC + 1;
    FLOOD     = REISSUE + 3 * (L - 1);
    
    FRAME     = FLOOD;

    // W-island topology: W = 3L^2 is partitioned into 9L islands of L/3 copies.
    // NOTE (provenance): this 9L x L/3 grouping is a residue of an abandoned line of
    // research, and it is NOT the ledger partition the paper quotes.  App. A ("The scale of
    // the lattice") factorises the same ledger as W = N_I * l, with N_I = 192L charge
    // fragments of l = L/64 addresses each.  The two are different objects at different
    // scales: l = L/64 is an integer only for L a multiple of 64 (and is below 1 for the
    // accessible sizes, L <= 31), while this runtime refuses even edges -- an even edge
    // breaks the unique-centre assumptions (CENTER = (L-1)/2, "every edge must be odd",
    // see initSim.cpp ~248/314).  Do not reuse this grouping as the model's declared
    // partition, and do not conflate it with the ledger, without a decision on the parity
    // of L.  The published reference census (K = 235 at L = 9, K = 667 at L = 15) was
    // produced with THIS grouping.  Two further points of record: (i) the odd-edge restriction
    // of this runtime is a convention of the seed, not a requirement of the rule -- the rule is
    // general in the parity of the edge, and App. A takes the physical ledger to be a multiple
    // of 64 (which also gives RMAX = L/2 exactly and a unique antipodal cell, measured: the
    // shells are identical for odd and even L at small radius, and only the end of the
    // expansion differs, 1 cell versus a 2x2x2 block); (ii) the divisibility-by-3 requirement
    // implicit here (ISLAND_SIZE = L/3) belongs to the abandoned grouping, and it is already
    // truncated in the L = 7 runs (W/(9*EL) = 147/63 = 2, not 7/3).
    ISLAND_COUNT = 9 * EL;
    ISLAND_SIZE  = (ISLAND_COUNT > 0) ? (W_USED / ISLAND_COUNT) : 0;
    if (ISLAND_SIZE == 0) ISLAND_SIZE = 1;

    printf("calculateParameters: EL=%u, W_USED=%u, RMAX=%u, CENTER=%u, FRAME=%u, ISLAND_SIZE=%u, ISLAND_COUNT=%u\n",
           EL, W_USED, RMAX, CENTER, FRAME, ISLAND_SIZE, ISLAND_COUNT);

    initCenters(W_USED);
  }

} // namespace automaton
