/*
 * simulation.h
 */

#ifndef SIMULATION_H_
#define SIMULATION_H_

#include <vector>
#include <array>
#include <iostream>
#include <cstdint>
#include <limits>

// Simulation symbols
#define NORTH     0
#define EAST      1
#define SOUTH     2
#define WEST      3
#define UP        4
#define DOWN      5
#define FORWARD   6
#define BACKWARD  7

// Enable simulation IDE
#define GRAPH

// Macros
#define ZERO(v)   (!(v[0] | v[1] | v[2]))

// Charge masks
#define C0_MASK     0x01
#define C1_MASK     0x02
#define C2_MASK     0x04
#define Q_MASK      0x08
#define W0_MASK     0x10
#define W1_MASK     0x20
#define COLOR_MASK  (C0_MASK | C1_MASK | C2_MASK)
#define WEAK_MASK   (W0_MASK | W1_MASK)
#define CHARGE_MASK (W0_MASK | W1_MASK | C0_MASK | C1_MASK | C2_MASK | Q_MASK)

/// Integer square root (binary method, table-free).
/// Keep this `inline`: the body lives in a header included by ~20 translation
/// units, and a non-inline definition breaks the link (LNK2005/LNK1169).
/// The `check-odr` make target guards this regression.
inline int isqrt(int n)
{
    if (n <= 0) return 0;
    int result = 0;
    int bit = 1 << 30;
    while (bit > n) bit >>= 2;
    while (bit != 0)
    {
        if (n >= result + bit)
        {
            n -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

// Platform-independent color type (RGBA)
struct Color {
  uint8_t r, g, b, a;
  
  Color() : r(0), g(0), b(0), a(255) {}
  Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
    : r(red), g(green), b(blue), a(alpha) {}
  
  // Convert to 32-bit integer (RGBA)
  uint32_t toUInt32() const {
    return (static_cast<uint32_t>(r) << 24) |
           (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
  }
  
  // Create from 32-bit integer (RGBA)
  static Color fromUInt32(uint32_t color) {
    return Color(
      (color >> 24) & 0xFF,
      (color >> 16) & 0xFF,
      (color >> 8) & 0xFF,
      color & 0xFF
    );
  }
};

namespace framework
{
  extern void sound(bool loop);
}

namespace automaton
{
  using namespace std;

  using WIndex = uint32_t;
  constexpr WIndex NO_LEADER_W = std::numeric_limits<WIndex>::max();

  // Source kinds for the spin-rev source model (K/S/D/P)
  enum class SourceKind : uint8_t { K = 0, S = 1, D = 2, P = 3 };
  constexpr uint32_t NO_PARENT = std::numeric_limits<uint32_t>::max();
  constexpr uint32_t NO_PAIR   = std::numeric_limits<uint32_t>::max();

  extern unsigned EL;
  // Per-axis edges.  In the default cubic mode ELX == ELY == ELZ == EL.
  // Declared here (before the inline accessors below) so getCell can index
  // with per-axis strides; tube allocators may set them unequal once the
  // per-axis geometry slices are complete.
  extern unsigned ELX;
  extern unsigned ELY;
  extern unsigned ELZ;
  extern unsigned W_USED;

  // ---------------------------------------------------------------------------
  // Pending source impulses for the current housekeeping tick.
  //
  // reemitSourceAt() books a displacement per source and commitSourceTick() drains the
  // queue into the draft lattice's source-centre cell just before applyMomentum() consumes
  // it.  External linkage on purpose: the source-level working array (sourceBefore/
  // sourceAfter, interaction.cpp) is re-seeded from the lattice at every tick and discarded
  // at the end of it, so a booking had nowhere durable to live -- measured with the sieve
  // open (L=7, first era): 1456 non-zero bookings in one tick, none applied, no source ever
  // moved, and all W bubbles stayed in one cell for the whole era.
  // ---------------------------------------------------------------------------
  struct ImpulseBooking
  {
    unsigned w;      // W address of the booked source
    int dx, dy, dz;  // displacement added to its reloc[]
  };
  extern std::vector<ImpulseBooking> g_pendingImpulses;
  extern bool convol_delay;
  extern bool diffuse_delay;
  extern bool reloc_delay;
  extern std::vector<std::array<unsigned, 3>> lcenters;


struct NeighborResult
{
    int x, y, z, w;

    bool wrapped;
    bool antipodal;
};

  extern string lastAllocationError;

  // Cell Class
  class Cell
  {
    public:
      // Physical properties
      WIndex w;           // Intrinsic W identity
      WIndex leader_w;    // Auxiliary W identity copied from the core
      bool is_core;       // Winding core flag
      unsigned char ch;   // Charge bits q, w1, w0, c2, c1, c0
      bool pB;            // local in-phase wave sign (pB = (u>0)); electric channel trigger
      bool sB;            // emergent transverse polarisation (sB = (v>0)); magnetic channel trigger
      unsigned a;         // Affinity
      unsigned x[4];      // Relative position
      // Wavefront
      unsigned d;         // Euclidean distance
      bool phiB;          // Active wavefront marker (phiB = active)
      unsigned t;         // Light frame counter
      unsigned f;         // Triangular breathing phase f = effective_t(t)
      // Operational variables
      unsigned c[3] = { 0, 0, 0 }; // Relocation offset
      unsigned k;         // Tick counter
      bool s2B;           // Sieve test result
      // Interaction control
      bool kB;            // Collapse flag
      bool bB;            // Blob flag
      bool homB;          // Homing flag
      bool cB;            // Contraction flag
      // Glider (antipodal transport)
      bool gB;            // Glider active flag
      int  g[3] = {0,0,0}; // Signed displacement to antipodal
      // Pulsating sphere
      unsigned int r2;    // Squared distance from center (BFS-propagated)
      int r;              // Integer radius propagated/corrected from r2
      int u, v;           // Radial polarisation pair (u: in-phase, v: quadrature)
      unsigned int active; // 1 when the cell is on the pulsating wavefront
      // Emergent polarisation broadcast (manuscript Sect. "Emergent
      // polarization pair"): b(x) arrival stamp + reconstructed pair.
      unsigned int bstamp; // Arrival tick of the elected-momentum news (0 = never reached)
      int pol_u, pol_v;    // Reconstructed transverse pair (approximating pol_u^2+pol_v^2 = R^4 via isqrt)
      // Spin-rev source model
      SourceKind kind;      // K (chief), S (singleton), D (delegate), P (pair)
      uint32_t parent;      // Parent source index (for D/P)
      int8_t spin_target;   // +1 outward / -1 inward / 0 neutral
      uint32_t pair_idx;    // Pair partner index for P sources
      uint8_t pair_count;   // Number of overlapping pairs in a P source (frequency = 2 * pair_count)
      int m[3];             // Momentum direction vector (long-term stable)
      int reloc[3];         // Consumable relocation offset / impulse
      // Default constructor
      Cell()
        : w(0), leader_w(NO_LEADER_W), is_core(false),
          ch(0), pB(false), sB(false), a(0),
          d(0), phiB(false), t(0), f(0),
          k(0), s2B(false), kB(false), bB(false), homB(false), cB(false),
          gB(false), r2(0xFFFFFFFFu), r(-1), u(0), v(0), active(0),
          bstamp(0), pol_u(0), pol_v(0),
          kind(SourceKind::S), parent(NO_PARENT), spin_target(0), pair_idx(NO_PAIR), pair_count(0)
      {
        fill(begin(x), end(x), 0);
        fill(begin(c), end(c), 0);
        fill(begin(g), end(g), 0);
        fill(begin(m), end(m), 0);
        fill(begin(reloc), end(reloc), 0);
      }
      // Serialization functions
      void serialize(ofstream& out) const;
      void deserialize(ifstream& in);

      bool Q() { return ch & Q_MASK; }
      bool W1() { return ch & W1_MASK; }
      bool W0() { return ch & W0_MASK; }
      bool C2() { return ch & C2_MASK; }
      bool C1() { return ch & C1_MASK; }
      bool C0() { return ch & C0_MASK; }
      unsigned char COLOR() { return ch & COLOR_MASK; }
      unsigned char ANTICOLOR() { return ~ch & COLOR_MASK; }
      Cell &getNeighbor(int i);
  };


  // Inline accessor for 4D indexing.  Per-axis strides: the spatial index is
  // ((x*ELY + y)*ELZ + z); for the default cube (ELX==ELY==ELZ==EL) this is
  // identical to the legacy ((x*EL + y)*EL + z) layout.
  inline Cell& getCell(vector<Cell>& lattice, int x, int y, int z, int w)
  {
    const size_t s = ((size_t)x * ELY + (size_t)y) * ELZ + (size_t)z;
    return lattice[s * W_USED + (size_t)w];
  }

  
  inline const Cell& getCell(const vector<Cell>& lattice, int x, int y, int z, int w)
  {
    const size_t s = ((size_t)x * ELY + (size_t)y) * ELZ + (size_t)z;
    return lattice[s * W_USED + (size_t)w];
  }

  /// Function prototypes ///
  void calculateParameters(unsigned L, unsigned W);
  // Region-driven configuration: the setup screen's region selects the lattice.
  bool configureLatticeFromRegion(unsigned W, int x0, int x1, int y0, int y1, int z0, int z1);
  void* SimulationLoop();
  void DeleteAutomaton();
  bool swap_lattices();
  void update();
  bool initSimulation(int step);
  void replicate();
  bool simulation();
  bool encounter(Cell& curr, Cell &draft, Cell &partner);
  // CPU source transactions: collect contacts without overwriting source writes.
  void beginSourceTick();
  void commitSourceTick();
  void resetSourceTransactions();
  bool isBoundPropeller(const Cell& source);
  bool hadInternalContact(WIndex a, WIndex b); // last/current frame, read-only
  // Legacy declaration only (no definition in this tree; pre-rename API).
  bool encounter7(Cell& curr, Cell &draft, Cell &partner);
  void diffuse(Cell& curr, Cell &draft, Cell &forward, Cell &north, Cell &west, Cell &down, Cell &south, Cell &east, Cell &up);
  void relocate(Cell& curr, Cell &draft, Cell &north, Cell &west, Cell &down);
  void reissue(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up);
  void flood(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up);
  void updateBuffer();
  void printLattice(int w);
  bool neutralColor(Cell &a, Cell &b);
  bool neutralWeak(Cell &a, Cell &b);
  void rotatePartners();
  bool sanityTest3();
  bool tryAllocate(int EL, int W);
  // Rectangular-tube allocator (S1 slice 1).  Equal edges delegate to the
  // cubic path; unequal edges are refused until the per-axis
  // indexing/wrap/distance-field slices are implemented (S1 slices 2-3).
  bool tryAllocateTube(unsigned LX, unsigned LY, unsigned LZ, unsigned W);
  unsigned int getRandomUnsigned(unsigned int modulus);
  void relocateGlobal(unsigned dx, unsigned dy, unsigned dz);

  // Encounter diagnostics (interaction.cpp).  Primary names are enc_*.
  // The conv_* spellings are kept as aliases so the campaign logs/scripts
  // and the headless runners keep working unchanged.
  extern long long enc_calls;
  extern long long enc_s2b;
  extern long long enc_pair;
  extern long long enc_self;
  extern long long enc_collapse;
  extern long long enc_adiah;
  extern long long enc_repel;
  extern long long recruit_events;
  extern long long recruit_repel;
  extern long long recruit_attract;
  extern long long annihilations;
  extern long long homb_events;      // candidate HOMB_PRODUCER_FSM (zero unless compiled)
  extern long long homb_seen;        // SLOT II consumer found a homer (same macro)
  extern long long c_at_center;      // c[] nonzero at a source centre (same macro)
  extern long long cB_at_center;     // ... with cB set (same macro)
  extern long long reloc_moves;      // source centres relocated (same macro)
  extern long long reloc_cells;      // cells migrated by the c[]-driven relocate() (same macro)
  extern long long consumer_transports;  // c[] decoded into reloc[] (HOMB_CONSUMER_TRANSPORT)
  extern long long c_with_reloc;     // diagnostic: c[] nonzero AND reloc[] pending
  extern long long fam_all_agree;    // local-step probe: family copies step alike
  extern long long fam_split;        // local-step probe: family copies disagree
  extern long long fam_now_ge2;      // local-step probe: >= 2 copies had a field at once
  extern long long address_walks;    // candidate ADDRESS_TARGET_FSM: steps toward the address site
  extern long long c_center_c0;      // arrivals at the first copy of a family
  extern long long c_center_c1;      // ... second
  extern long long c_center_c2;      // ... third
  extern long long loc_step_total;   // copies whose local decode gives a non-zero step

  // Deprecated aliases of the enc_* counters above.
  extern long long& conv_calls;
#ifdef SURFACE_ESCAPE_FSM
  extern unsigned surface_escapes;   // candidate: members released by surface escape
#endif
#ifdef SPIN_GATED_FSM
  extern unsigned spin_vetoes;       // J-programme S3: releases vetoed by group spin
  extern unsigned long long spin_jmax2;   // S4 cap on |J|^2 (0 = disabled)
  extern unsigned spin_cap_evictions;     // S4: members shed by the cap
#endif
#ifdef WINDING_GATED_FSM
  extern unsigned winding_vetoes;    // T1: releases vetoed by planted/group winding
  extern int chief_W[3];             // (Wx,Wy,Wz); W2 != 0 => protect
#endif
#ifdef PAIR_STACK_ABSORB_FSM
  extern long long enc_absorb;       // candidate: formations that absorbed identical stacks
#endif
#ifdef MULTIFREQ_RAY_FSM
  extern long long mf_reads;         // candidate: ray walks for frequency-bearing sides
  extern long long mf_hits;          // candidate: ... whose detection bit fired
  extern long long mf_gated;         // candidate: pB/sB openings suppressed by freq_hit
  extern std::vector<unsigned char> mfHitState;    // per-source freq_hit of the current tick
  extern std::vector<unsigned>      mfDetU;        // ... u read at the detection cell
  extern std::vector<std::array<int, 3>> mfDetPos; // ... detection cell coordinates
#endif

  extern long long& conv_s2b;
  extern long long& conv_pair;
  extern long long& conv_self;
  extern long long& conv_collapse;
  extern long long& conv_adiah;
  extern long long& conv_repel;

  // Electroweak sieve modulus (manuscript, ``s2B'' gate):
  //   s2B is set iff  active && ((u * (t+1)) mod s2b_target) < u.
  // Runtime-tunable so the headless runners can sweep the parameter space
  // (tests/scatter_main.cpp --sieve, tests/s2b_sweep_main.cpp).  The
  // reference value 16384 reproduces the numbers reported in Sect. Results.
  extern int s2b_target;

#ifdef S2B_TRACE
  // Opt-in in-loop instrumentation of the s2B channel (definitions and semantics in
  // simulation.cpp, next to s2b_target).  Compiled only with /DS2B_TRACE.
  extern unsigned long long s2bTraceFired, s2bTraceFiredTOff, s2bTraceFiredTChanged,
                            s2bTraceFiredActOff, s2bTraceActLost, s2bTraceClockReset,
                            s2bTraceReemitResets, s2bTraceCBResets,
                            s2bTraceFloodPulls, s2bTraceFloodResets,
                            s2bTraceReemitImpulse, s2bTraceRelocSeen, s2bTraceRelocApplied,
                            s2bTraceReemitAtCentre, s2bTraceReemitOffCentre,
                            s2bTraceCommitPending, s2bTraceCommitWiped, s2bTraceImpulseCommitted,
                            s2bTraceImpulseBooked, s2bTraceImpulseSkipped, s2bTraceRelocSumAtCommit,
                            s2bTraceTicks, s2bTraceReapplySum, s2bTraceCommitAccSum,
                            s2bTraceImpulseDrained, s2bTraceCommitCalls, s2bTraceDrainWriteback,
                            s2bTraceNetLayers, s2bTraceNetMax, s2bTraceChargeDispersion,
                            s2bTracePolarSeed, s2bTraceReseatSteps, s2bTraceReseatAligned,
                            s2bTraceReseatOther, s2bTraceReseatAtContact, s2bTraceReseedCarried;
  // Per-layer flag: a cohesion step (the moves[] table of resolveInternalContacts) was booked for
  // this layer in the current light frame.  Guarded by S2B_TRACE with the counters above; the
  // harness uses it to report the cohesion and flight channels' alignment separately.
  extern std::vector<unsigned char> s2bTraceCohesionFlag;
  // Per-layer writer mask (same frame): one bit per site that can contribute a displacement, so a
  // layer whose mask has two bits was moved by TWO movers in one frame -- the "mixture" the trace is
  // looking for.  Bits: 1 reemitSourceAt (walk funnel), 2 reseatStepToward (relay), 4
  // reseatAtContact (relay contact), 8 cohesion table, 16 charge dispersion, 32 the own-axis
  // exchange thrust, 64 the impulse-queue drain (draft).  See S2B_DUMP_PENDING.
  extern std::vector<unsigned int> s2bTraceWriterMask;
  // Thrust word probe (same frame, per layer): set when the own-axis exchange thrust booked this
  // layer's displacement while reading a charge word that is NOT the one the commit installs for it
  // (the rules can rewrite the draft's ch during the tick; the commit copies sourceAfter[w].ch, which
  // was captured at the start of it).  s2bTraceThrustCalls counts the thrust applications and
  // s2bTraceThrustWordDiff the subset that read a different word.  The harness cross-tabulates the
  // flag against the octant alignment (line `move-thrust`); /D THRUST_WORD_TRACE prints each case.
  extern unsigned long long s2bTraceThrustCalls, s2bTraceThrustWordDiff;
  extern std::vector<unsigned char> s2bTraceThrustWordFlag;
  // The thrust vector actually written for each layer in the current light frame (3 ints per layer), so
  // the harness can compare it with the centre displacement it measures -- the last gap in the chain:
  // the decision point is provably octant-clean, so if a mover's measured displacement does NOT follow
  // this vector, that displacement is not the impulse.  Maintained always (bookkeeping only, no print);
  // the comparison is printed under /D THRUST_VEC_TRACE and is cleared by the harness per frame.
  extern std::vector<int> s2bTraceThrustVec;
  // Booking-side score of the same thrusts (cumulative): the thrust vector the site is about to write,
  // scored against the COMMITTED word's octant -- all three axes touched and every sign matching
  // ("aligned", what the harness's 3-of-3 test would accept), fewer than three axes touched
  // ("partial": a step along two of the octant's axes can never pass a 3-of-3 test, whatever the
  // direction), or at least one sign against the octant ("against").  Calls = aligned + partial +
  // against, by construction.
  extern unsigned long long s2bTraceThrustBookingAligned, s2bTraceThrustBookingPartial,
                            s2bTraceThrustBookingAgainst;
#endif

  // Tests

  void printParams();
  bool sanityTest();
  bool sanityTest2();

  /// Cross variables ///
  extern vector<Cell> lattice_curr;

  /// Cross constants ///
  extern unsigned ORDER;
  extern unsigned EL;
  extern unsigned L2;
  extern unsigned L3;
  extern unsigned W_DIM;
  extern unsigned W_USED;
  extern unsigned long BLOCK;
  extern unsigned CENTER;
  extern unsigned FCENTER;
  extern unsigned UPDATE;
  extern unsigned DIAG;
  extern unsigned RMAX;
  extern unsigned CONTRACT;
  extern unsigned ENCOUNTER;
  extern unsigned GSLOT_X;
  extern unsigned GSLOT_Y;
  extern unsigned GSLOT_Z;
  extern unsigned SLOT1;
  extern unsigned SLOT2;
  extern unsigned SLOT3;
  extern unsigned SLOT4;
  extern unsigned DIFFUSION;
  extern unsigned SLOT5;
  extern unsigned SLOT6;
  extern unsigned SLOT7;
  extern unsigned SLOT8;
  extern unsigned RELOC;
  extern unsigned REISSUE;
  extern unsigned FLOOD;
  extern unsigned FRAME;
  extern unsigned int pulse_tick;

  // ------------------------------------------------------------------
  // Fatia 1 — charge census / virgin-wrap ledger (instrumentation only).
  // Read-only over the lattice: these hooks never write cells, so the
  // automaton dynamics are bit-identical with or without them.
  // Defined in src/model/charges.cpp.  CPU path only (update_lattice_cpu);
  // the CUDA bridge is deliberately untouched in this slice.
  // ------------------------------------------------------------------
  void chargesReset();                      // zero ledgers (called at sim init)
  void chargesMarkInteraction(unsigned w);  // island w just reemitted (clock reset)
  void chargesMarkPair();                   // a registered P formation was created (idea B)
  void chargesMarkBlob();                   // a superposed-pair group formed a blob
  void chargesMarkAnnihilation();           // a representative pair annihilated (rule 5)
  void chargesSampleTurnarounds();          // per-tick t==RMAX crossing detector
  void chargesReport(unsigned tick);        // throttled matter/antimatter census

  // W-island topology (W = 3L^2 = (9L) * (L/3))
  extern unsigned ISLAND_SIZE;
  extern unsigned ISLAND_COUNT;

  inline unsigned islandOf(WIndex w)       { return (ISLAND_SIZE > 0) ? (unsigned)(w / ISLAND_SIZE) : 0; }
  inline WIndex firstWOfIsland(unsigned i) { return (WIndex)(i * ISLAND_SIZE); }
  inline bool   isIslandChief(WIndex w)    { return (ISLAND_SIZE > 0) && ((w % ISLAND_SIZE) == 0); }

  #define INF_R2 0xFFFFFFFFu

  void update_pulsating_wavefront();

  // Effective wavefront radius (triangle wave: expands 0→RMAX, contracts RMAX→0).
  // Period = 2*RMAX (= L in physics terms), amplitude = RMAX.
  // This is the local, constant-speed light-clock: a cell is on the active
  // shell exactly when its propagated integer radius r equals this value.
  inline unsigned effective_t(unsigned t)
  {
      unsigned cycle = 2 * RMAX;
      unsigned phase = t % cycle;
      if (phase <= RMAX)
          return phase;
      else
          return cycle - phase;
  }

#ifdef ORPHAN_GUIDANCE_FSM
  // Experimental P1: the ORPHAN SHELL.  With the spherical cavity the orphan
  // field is the thin concentric layer just AHEAD of the active wavefront
  // (radius f + 1, inside the cavity), i.e. the part of the layer that has not
  // yet been overlapped by the front.  It is DERIVED, not stored: no lattice
  // write is needed, the predicate is local, and concentricity is automatic
  // under translation (r is recomputed against the layer centre every tick, so
  // the shell moves with it).  Keeping the shell one cell thick keeps the
  // light-matter channel as rare as it is in nature.
  inline bool isOrphanShell(const Cell& c)
  {
#ifdef ORPHAN_NO_SHELL
      // Control (b) of section 6: the orphan flux is switched off, so the
      // recruit gate must never fire and the mediated channel disappears.
      (void)c;
      return false;
#else
      return c.r2 > 0u && c.r2 != INF_R2 &&
             c.r >= 0 && c.r == (int)c.f + 1 &&
             c.r <= (int)RMAX;
#endif
  }
#endif


/// Cross variables ///
extern std::vector<Cell> lattice_curr;
extern std::vector<Cell> lattice_draft; // Add or verify
extern std::vector<Cell> lattice_partner; // Add or verify

  /**
   * Tests if two vectors are equal.
   */
  inline bool EQUAL(unsigned v1[3], unsigned v2[3])
  {
    // Compare elements
    for (size_t i = 0; i < 3; ++i)
    {
      if (v1[i] != v2[i])
        return false;
    }
    return true;
  }

// ===================================================================
  // CUDA ACCELERATION FUNCTIONS
  // ===================================================================
  
  // These functions are always declared, regardless of USE_CUDA
  // When USE_CUDA is defined: implemented in cuda_automaton.cu
  // When USE_CUDA is NOT defined: implemented in bridge.cpp as stubs
  
  bool tryEnableCuda();
  void disableCuda();
  bool isCudaEnabled();
  
#ifdef USE_CUDA
  // Internal GPU wrapper functions - only declared when CUDA is enabled
  // Implementations are in cuda_automaton.cu
  bool swap_lattices_gpu();

  // Pointers for Device (GPU) memory
  extern Cell* d_lattice_curr;
  extern Cell* d_lattice_draft;
  extern Cell* d_lattice_partner;
  
#endif // USE_CUDA
  
}

#endif /* SIMULATION_H_ */
