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
    ISLAND_COUNT = 9 * EL;
    ISLAND_SIZE  = (ISLAND_COUNT > 0) ? (W_USED / ISLAND_COUNT) : 0;
    if (ISLAND_SIZE == 0) ISLAND_SIZE = 1;

    printf("calculateParameters: EL=%u, W_USED=%u, RMAX=%u, CENTER=%u, FRAME=%u, ISLAND_SIZE=%u, ISLAND_COUNT=%u\n",
           EL, W_USED, RMAX, CENTER, FRAME, ISLAND_SIZE, ISLAND_COUNT);

    initCenters(W_USED);
  }

} // namespace automaton
