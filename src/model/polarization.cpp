/*
 * polarization.cpp — Emergent polarization pair (u,v).
 *
 * Implementation of the three-stage mechanism described in the manuscript
 * section "Emergent polarization pair (u,v)":
 *
 *   ELECTION -> BROADCAST -> RECONSTRUCTION
 *
 * The broadcast stage is an exact port of the arbitrary-axis helical
 * walker from the spiral.c reference implementation (E:\spiral):
 *   - init-time constants  D[m] = e_m x AXIS,  K[k][m] = 2 D_k . D_m,
 *     TGT = R_CYL^2 |AXIS|^2, TOL ~ +/-1 cell radial band;
 *   - runtime walker updates use only +, -, << and comparisons
 *     (generalised sum-of-odds for both |v|^2 and |c|^2);
 *   - Bresenham interleaves orbital steps with climb steps along AXIS
 *     (multi-dimensional DDA).
 *
 * Init-time arithmetic (axis normalisation, orbit budget integral) is
 * host-side only, exactly like pulse_from_time() — the CA rules proper
 * never see multiplication.
 */

#include "model/polarization.h"
#include "model/polarization_candidate.h"

#include <cmath>
#include <vector>
#include <cstring>

namespace automaton
{
  namespace polarization
  {
    // ============================================================
    // Per-layer state
    // ============================================================

    // Axis-dependent constants, computed once per election
    // (host-side arithmetic allowed here, like spiral_set_axis).
    struct AxisConstants
    {
      int       axis[3];              // elected direction, |AXIS| = RMAX
      long long Dx[6], Dy[6], Dz[6];  // D[m] = e_m x AXIS
      long long Kk[6][6];             // K[k][m] = 2 D_k . D_m
      long long TGT;                  // R_CYL^2 * |AXIS|^2
      long long TOL;                  // radial band ~ +/-1 cell
      int       absa[3];              // |ax|,|ay|,|az|
      int       climb_total;          // |ax|+|ay|+|az| (DDA period)
      int       orbit_total;          // orbital budget for one revolution
    };

    // Runtime walker state (integers only during the walk).
    struct Walker
    {
      bool      live      = false;  // walk advancing
      bool      done      = true;   // walk reached theta = pi / boxed in
      bool      diffusing = false;  // stamps still spreading
      bool      sweepForward = true;
      long long steps     = 0;
      int       tip[3]    = { 0, 0, 0 };
      int       v[3]      = { 0, 0, 0 };
      long long q2        = 0;      // |v|^2 (sum of odds)
      long long cxv = 0, cyv = 0, czv = 0; // c = u x AXIS, u = v - P
      long long E         = 0;      // |c|^2
      long long G[6]      = { 0, 0, 0, 0, 0, 0 };
      int       tip_acc   = 0;      // orbital/climb Bresenham accumulator
      int       dda[3]    = { 0, 0, 0 };
    };

    static std::vector<AxisConstants> g_axis;
    static std::vector<Walker>        g_walk;
    // packed visited bits, (ELX*ELY*ELZ)/8 bytes per layer
    static std::vector<std::vector<uint8_t>> g_visit;

#ifdef POLAR_BOOTSTRAP_ADDRESS
    // One-shot bootstrap bookkeeping (experimental): per layer, whether the
    // deterministic address-derived axis has already been installed.
    static std::vector<uint8_t> g_boot;

    // 26 non-zero neighbour directions; axis index = w % 26.  Consecutive
    // W addresses (the copies of a family) map to distinct directions.
    static const int kBootDir[26][3] = {
      {-1,-1,-1},{-1,-1, 0},{-1,-1, 1},{-1, 0,-1},{-1, 0, 0},{-1, 0, 1},
      {-1, 1,-1},{-1, 1, 0},{-1, 1, 1},
      { 0,-1,-1},{ 0,-1, 0},{ 0,-1, 1},{ 0, 0,-1},{ 0, 0, 1},
      { 0, 1,-1},{ 0, 1, 0},{ 0, 1, 1},
      { 1,-1,-1},{ 1,-1, 0},{ 1,-1, 1},{ 1, 0,-1},{ 1, 0, 0},{ 1, 0, 1},
      { 1, 1,-1},{ 1, 1, 0},{ 1, 1, 1}
    };
#endif

    // Lattice moves: 0:+x 1:-x 2:+y 3:-y 4:+z 5:-z
    static const int kMvx[6] = {  1, -1, 0,  0, 0,  0 };
    static const int kMvy[6] = {  0,  0, 1, -1, 0,  0 };
    static const int kMvz[6] = {  0,  0, 0,  0, 1, -1 };

    // Cylindrical-helix parameters (spiral.h): compile-time integer
    // approximation 93/512 ~= 0.18164 of r_max.
    static inline int cylRadius() { return (int)(((long long)RMAX * 93 + 256) >> 9); }
    static inline int emergentR() { return (int)RMAX - 2; }  // R = L/2 - 2

    static inline void ensureSized()
    {
      if (g_axis.size() != W_USED || g_walk.size() != W_USED)
      {
        g_axis.assign(W_USED, AxisConstants());
        g_walk.assign(W_USED, Walker());
        g_visit.assign(W_USED, std::vector<uint8_t>());
        for (unsigned w = 0; w < W_USED; ++w)
          g_visit[w].assign((((size_t)ELX * ELY * ELZ) + 7) / 8, 0);
#ifdef POLAR_BOOTSTRAP_ADDRESS
        g_boot.assign(W_USED, 0);
#endif
      }
    }

    void resetAll()
    {
      g_axis.clear();
      g_walk.clear();
      g_visit.clear();
#ifdef POLAR_BOOTSTRAP_ADDRESS
      g_boot.clear();
#endif
    }

    bool walkLive(unsigned w)
    {
      return (w < g_walk.size()) && g_walk[w].live;
    }

    const int* electedAxis(unsigned w)
    {
      return (w < g_axis.size() && g_axis[w].climb_total != 0)
               ? g_axis[w].axis : nullptr;
    }

    // ============================================================
    // Polarization-only candidate selection (experimental).
    // ============================================================
    // ============================================================
    // installAxis — spiral_set_axis port.
    // ============================================================
    static void installAxis(unsigned w, int ax, int ay, int az)
    {
      AxisConstants& C = g_axis[w];
      const int R_MAX_ = (int)RMAX;

      if (ax == 0 && ay == 0 && az == 0) { ax = 0; ay = 0; az = R_MAX_; }

      // Normalise to length RMAX = L/2.
      {
        double n  = sqrt((double)ax * ax + (double)ay * ay + (double)az * az);
        double s  = (n > 0.0) ? ((double)R_MAX_ / n) : 1.0;
        C.axis[0] = (int)floor((double)ax * s + 0.5);
        C.axis[1] = (int)floor((double)ay * s + 0.5);
        C.axis[2] = (int)floor((double)az * s + 0.5);
        if (C.axis[0] == 0 && C.axis[1] == 0 && C.axis[2] == 0)
          C.axis[2] = R_MAX_;
      }

      const int axv = C.axis[0], ayv = C.axis[1], azv = C.axis[2];

      C.absa[0] = axv < 0 ? -axv : axv;
      C.absa[1] = ayv < 0 ? -ayv : ayv;
      C.absa[2] = azv < 0 ? -azv : azv;
      C.climb_total = C.absa[0] + C.absa[1] + C.absa[2];

      // Orbital step budget for one revolution: R_CYL times the mean
      // Manhattan norm of the unit tangent around the tilted circle
      // (spiral.c evaluates this integral once at init time).
      {
        double A[3]  = { (double)axv, (double)ayv, (double)azv };
        double aa[3] = { fabs(A[0]), fabs(A[1]), fabs(A[2]) };
        int wi = 0;
        if (aa[1] < aa[wi]) wi = 1;
        if (aa[2] < aa[wi]) wi = 2;
        double wv[3] = { 0, 0, 0 };
        wv[wi] = 1.0;

        double e1[3], e2[3], n1, n2;
        e1[0] = A[1] * wv[2] - A[2] * wv[1];
        e1[1] = A[2] * wv[0] - A[0] * wv[2];
        e1[2] = A[0] * wv[1] - A[1] * wv[0];
        n1 = sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
        for (int i = 0; i < 3; i++) e1[i] /= n1;
        e2[0] = A[1] * e1[2] - A[2] * e1[1];
        e2[1] = A[2] * e1[0] - A[0] * e1[2];
        e2[2] = A[0] * e1[1] - A[1] * e1[0];
        n2 = sqrt(e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]);
        for (int i = 0; i < 3; i++) e2[i] /= n2;

        const double TWO_PI = 6.283185307179586;
        const int N = 720;
        double acc = 0.0;
        for (int i = 0; i < N; i++)
        {
          double th = TWO_PI * (double)i / (double)N;
          double s  = sin(th), cth = cos(th);
          double tx = -s * e1[0] + cth * e2[0];
          double ty = -s * e1[1] + cth * e2[1];
          double tz = -s * e1[2] + cth * e2[2];
          acc += (fabs(tx) + fabs(ty) + fabs(tz)) * TWO_PI / (double)N;
        }
        int rc = cylRadius();
        C.orbit_total = (int)floor((double)rc * acc + 0.5);
        if (C.orbit_total < (rc << 3)) C.orbit_total = (rc << 3); // CYL_CIRC
      }

      // D[m] = e_m x AXIS (constant increment of c per lattice move).
      C.Dx[0] =  0;               C.Dy[0] = -(long long)azv; C.Dz[0] =  (long long)ayv; // +x
      C.Dx[1] =  0;               C.Dy[1] =  (long long)azv; C.Dz[1] = -(long long)ayv; // -x
      C.Dx[2] =  (long long)azv;  C.Dy[2] =  0;               C.Dz[2] = -(long long)axv; // +y
      C.Dx[3] = -(long long)azv;  C.Dy[3] =  0;               C.Dz[3] =  (long long)axv; // -y
      C.Dx[4] = -(long long)ayv;  C.Dy[4] =  (long long)axv;  C.Dz[4] =  0;              // +z
      C.Dx[5] =  (long long)ayv;  C.Dy[5] = -(long long)axv;  C.Dz[5] =  0;              // -z

      // K[k][m] = 2 D_k . D_m (how G[m] shifts after taking move k).
      for (int k = 0; k < 6; k++)
        for (int m = 0; m < 6; m++)
          C.Kk[k][m] = 2 * (C.Dx[k] * C.Dx[m] +
                            C.Dy[k] * C.Dy[m] +
                            C.Dz[k] * C.Dz[m]);

      // TGT = R_CYL^2 * |AXIS|^2 ; TOL = (2 R_CYL + 1) * |AXIS|^2.
      {
        long long rc  = cylRadius();
        long long rc2 = rc * rc;
        long long a2  = (long long)axv * axv + (long long)ayv * ayv
                      + (long long)azv * azv;
        C.TGT = rc2 * a2;
        C.TOL = (2 * rc + 1) * a2;
      }
    }


    // ============================================================
    // Visited-bit helpers (packed, module-owned scratch)
    // ============================================================
    static inline size_t cellCode(unsigned x, unsigned y, unsigned z)
    {
      return ((size_t)x * ELY + y) * ELZ + z;
    }

    static inline bool visitGet(unsigned w, int x, int y, int z)
    {
      size_t code = cellCode((unsigned)x, (unsigned)y, (unsigned)z);
      return (g_visit[w][code >> 3] >> (code & 7)) & 1u;
    }

    static inline void visitSet(unsigned w, int x, int y, int z)
    {
      size_t code = cellCode((unsigned)x, (unsigned)y, (unsigned)z);
      g_visit[w][code >> 3] |= (uint8_t)(1u << (code & 7));
    }

    // ============================================================
    // initWalker — spiral_init port: seed the walker at the lattice
    // centre on the cylinder of radius R_CYL offset by perpendicular P.
    // ============================================================
    static void initWalker(unsigned w)
    {
      const AxisConstants& C = g_axis[w];
      Walker& W = g_walk[w];

      // Build P: perpendicular to AXIS, length R_CYL (least-aligned cardinal).
      int Px, Py, Pz;
      {
        int wi = 0;
        if (C.absa[1] < C.absa[wi]) wi = 1;
        if (C.absa[2] < C.absa[wi]) wi = 2;

        double p0x, p0y, p0z;
        if (wi == 0)      { p0x = 0;                  p0y =  (double)C.axis[2]; p0z = -(double)C.axis[1]; }
        else if (wi == 1) { p0x = -(double)C.axis[2]; p0y =  0;                  p0z =  (double)C.axis[0]; }
        else              { p0x =  (double)C.axis[1]; p0y = -(double)C.axis[0]; p0z =  0; }
        double n = sqrt(p0x * p0x + p0y * p0y + p0z * p0z);
        if (n <= 0.0) n = 1.0;
        double s = (double)cylRadius() / n;
        Px = (int)floor(p0x * s + 0.5);
        Py = (int)floor(p0y * s + 0.5);
        Pz = (int)floor(p0z * s + 0.5);
      }

      W.tip[0] = (int)CENTER; W.tip[1] = (int)CENTER; W.tip[2] = (int)CENTER;
      W.v[0] = 0; W.v[1] = 0; W.v[2] = 0;
      W.q2 = 0;

      // u = v - P ; c = u x AXIS
      {
        long long ux = -Px, uy = -Py, uz = -Pz;
        W.cxv = uy * C.axis[2] - uz * C.axis[1];
        W.cyv = uz * C.axis[0] - ux * C.axis[2];
        W.czv = ux * C.axis[1] - uy * C.axis[0];
      }
      W.E = W.cxv * W.cxv + W.cyv * W.cyv + W.czv * W.czv;

      // G[m] = 2 (c . D_m) + |D_m|^2 — the generalised "2dx+1".
      for (int m = 0; m < 6; m++)
      {
        W.G[m] = 2 * (W.cxv * C.Dx[m] + W.cyv * C.Dy[m] + W.czv * C.Dz[m])
               + (C.Dx[m] * C.Dx[m] + C.Dy[m] * C.Dy[m] + C.Dz[m] * C.Dz[m]);
      }

      W.tip_acc = 0;
      W.dda[0] = W.dda[1] = W.dda[2] = 0;
      W.done = false;
      W.live = true;
      W.diffusing = true;
      W.sweepForward = true;
      W.steps = 0;

      // Mark the start cell visited and stamp the news there too.
      // Stamps store (arrival tick + 1): 0 stays reserved for "never reached".
      visitSet(w, W.tip[0], W.tip[1], W.tip[2]);
      Cell& origin = getCell(lattice_curr, W.tip[0], W.tip[1], W.tip[2], w);
      origin.bstamp = pulse_tick + 1u;
    }

    bool seedAxis(unsigned w, int ax, int ay, int az)
    {
      if (W_USED == 0 || w >= W_USED || EL == 0)
        return false;
      ensureSized();
      g_walk[w] = Walker{};
      std::memset(g_visit[w].data(), 0, g_visit[w].size());
      installAxis(w, ax, ay, az);
      initWalker(w);
      return true;
    }

    // ============================================================
    // applyMove — commit lattice move m (additions/shifts only).
    //
    //   E += G[m];  G[k] += K[m][k];  c += D[m];
    //   q2 += (2 v_i + 1) or -(2 v_i - 1)   (sum of odds)
    // ============================================================
    static void applyMove(unsigned w, int m)
    {
      const AxisConstants& C = g_axis[w];
      Walker& W = g_walk[w];

      W.E += W.G[m];
      for (int k = 0; k < 6; k++) W.G[k] += C.Kk[m][k];
      W.cxv += C.Dx[m];
      W.cyv += C.Dy[m];
      W.czv += C.Dz[m];

      switch (m)
      {
      case 0: W.q2 += (((long long)W.v[0]) << 1) + 1; W.v[0]++; W.tip[0]++; break;
      case 1: W.q2 -= (((long long)W.v[0]) << 1) - 1; W.v[0]--; W.tip[0]--; break;
      case 2: W.q2 += (((long long)W.v[1]) << 1) + 1; W.v[1]++; W.tip[1]++; break;
      case 3: W.q2 -= (((long long)W.v[1]) << 1) - 1; W.v[1]--; W.tip[1]--; break;
      case 4: W.q2 += (((long long)W.v[2]) << 1) + 1; W.v[2]++; W.tip[2]++; break;
      case 5: W.q2 -= (((long long)W.v[2]) << 1) - 1; W.v[2]--; W.tip[2]--; break;
      }
    }


    // In-bounds and not already part of the arm.
    static inline bool moveOK(unsigned w, int m, const Walker& W)
    {
      int nx = W.tip[0] + kMvx[m];
      int ny = W.tip[1] + kMvy[m];
      int nz = W.tip[2] + kMvz[m];
      if (nx < 0 || nx >= (int)ELX ||
          ny < 0 || ny >= (int)ELY ||
          nz < 0 || nz >= (int)ELZ) return false;
      return !visitGet(w, nx, ny, nz);
    }

    // ============================================================
    // walkerStep — spiral_step port: advance the helix by one cell.
    //
    // Bresenham distributes climb_total climb steps among orbit_total
    // orbital steps.  Orbital steps maximise tangential progress inside
    // the radial tolerance band; climb steps follow AXIS by DDA.
    // ============================================================
    static void walkerStep(unsigned w)
    {
      const AxisConstants& C = g_axis[w];
      Walker& W = g_walk[w];
      if (!W.live || W.done) return;

      int period   = C.orbit_total + C.climb_total;
      int new_acc  = W.tip_acc + C.climb_total;
      int do_climb = 0;
      if (new_acc >= period) { new_acc -= period; do_climb = 1; }
      W.tip_acc = new_acc;

      int chosen = -1;

      if (do_climb)
      {
        // Climb step: multi-dimensional DDA along AXIS.
        W.dda[0] += C.absa[0];
        W.dda[1] += C.absa[1];
        W.dda[2] += C.absa[2];

        int i = 0;
        if (W.dda[1] > W.dda[i]) i = 1;
        if (W.dda[2] > W.dda[i]) i = 2;
        W.dda[i] -= C.climb_total;

        int sgn_neg = (i == 0) ? (C.axis[0] < 0)
                    : (i == 1) ? (C.axis[1] < 0)
                               : (C.axis[2] < 0);
        int m = (i << 1) + (sgn_neg ? 1 : 0);
        if (moveOK(w, m, W)) chosen = m;
        // If blocked, fall through to an orbital choice below.
      }

      if (chosen < 0)
      {
        // Orbital step: stay on the cylinder, keep turning.
        // Tangent t = AXIS x u = -c => t(+x) = -c_x, t(-x) = +c_x, ...
        long long t[6];
        t[0] = -W.cxv; t[1] =  W.cxv;
        t[2] = -W.cyv; t[3] =  W.cyv;
        t[4] = -W.czv; t[5] =  W.czv;

        long long best_err = 0, best_tan = 0;
        int best = -1;

        for (int m = 0; m < 6; m++)
        {
          if (!moveOK(w, m, W)) continue;
          if (t[m] <= 0) continue;                 // wrong rotation sense
          long long e = W.E + W.G[m] - C.TGT;
          if (e < 0) e = -e;
          if (e <= C.TOL)
          {
            if (best < 0 || best_err > C.TOL || t[m] > best_tan)
            {
              best = m; best_err = e; best_tan = t[m];
            }
          }
          else if (best < 0 || (best_err > C.TOL && e < best_err))
          {
            best = m; best_err = e; best_tan = t[m];
          }
        }

        if (best < 0)  // fallback: any move minimising radial error
        {
          for (int m = 0; m < 6; m++)
          {
            if (!moveOK(w, m, W)) continue;
            long long e = W.E + W.G[m] - C.TGT;
            if (e < 0) e = -e;
            if (best < 0 || e < best_err) { best = m; best_err = e; }
          }
        }
        if (best < 0) { W.done = true; W.live = false; return; } // boxed in
        chosen = best;
      }

      applyMove(w, chosen);

      visitSet(w, W.tip[0], W.tip[1], W.tip[2]);
      Cell& tipCell = getCell(lattice_curr,
                              (unsigned)W.tip[0], (unsigned)W.tip[1],
                              (unsigned)W.tip[2], w);
      tipCell.bstamp = pulse_tick + 1u;
      W.steps++;

      // theta = pi reached: the arm touched the outer shell r = L/2 - 2.
      long long rad_eq = emergentR();
      long long rad_sq = rad_eq * rad_eq;
      long long max_pts = (long long)(12 * (int)RMAX);
      if (W.q2 >= rad_sq || W.steps >= max_pts)
      {
        W.done = true;
        W.live = false;
      }
    }

    // ============================================================
    // Election — payload tournament over the active shell.
    // The winner is identified once, at sowing time, while the payloads
    // are still unique (strict total order via the low-bit code).
    // ============================================================
    static void elect(unsigned w)
    {
      // Hypothesis: select from existing polarization only. No address,
      // hash, global dial, or scan order may decide a tie.
#ifdef POLAR_BOOTSTRAP_ADDRESS
      // --------------------------------------------------------------
      // One-shot deterministic bootstrap (experimental, /D
      // POLAR_BOOTSTRAP_ADDRESS).  The zero-polarisation seed is a fixed
      // point of the election->broadcast->reconstruction loop: no axis can
      // be elected without existing (pol_u,pol_v), and none can be
      // reconstructed without broadcast stamps.  Break that fixed point
      // exactly once per layer by installing a topological initial axis
      // derived from the immutable W address (copies of a family map to
      // distinct directions), so the first helical broadcast can run;
      // subsequent eras use the classic election below (real pol).
      // --------------------------------------------------------------
      if (w < g_boot.size() && !g_boot[w])
      {
        g_boot[w] = 1;
        bool hasPol = false;
        for (unsigned x = 0; x < ELX && !hasPol; ++x)
        for (unsigned y = 0; y < ELY && !hasPol; ++y)
        for (unsigned z = 0; z < ELZ && !hasPol; ++z)
        {
          const Cell& c = getCell(lattice_curr, x, y, z, w);
          if (c.active && (c.pol_u != 0 || c.pol_v != 0)) hasPol = true;
        }
        if (!hasPol)
        {
          const int (&d)[3] = kBootDir[w % 26u];
          installAxis(w, d[0], d[1], d[2]);

          // Publish m on the source centre (same tail as the classic path).
          {
            const int* ax = electedAxis(w);
            if (ax)
            {
              const unsigned cx = lcenters[w][0];
              const unsigned cy = lcenters[w][1];
              const unsigned cz = lcenters[w][2];
              Cell& src = getCell(lattice_curr, cx, cy, cz, w);
              src.m[0] = ax[0]; src.m[1] = ax[1]; src.m[2] = ax[2];
            }
          }
          // Reset the arrival stamps and start the helical broadcast.
          for (unsigned x = 0; x < ELX; ++x)
          for (unsigned y = 0; y < ELY; ++y)
          for (unsigned z = 0; z < ELZ; ++z)
            getCell(lattice_curr, x, y, z, w).bstamp = 0;
          std::memset(g_visit[w].data(), 0, g_visit[w].size());
          initWalker(w);
          return;
        }
      }
#endif
      PolarizationCandidate candidate;
      int bw[3] = {-1,-1,-1};
      for (unsigned x=0;x<ELX;++x)
      for (unsigned y=0;y<ELY;++y)
      for (unsigned z=0;z<ELZ;++z) {
        const Cell& c=getCell(lattice_curr,x,y,z,w);
        if (c.active && candidate.consider(c.pol_u,c.pol_v)) {
          bw[0]=x; bw[1]=y; bw[2]=z;
        }
      }
      if (!candidate.unique()) return;

      // Displacement from the lattice centre to the winning cell becomes
      // the momentum direction, rescaled to |m| = L/2 (= RMAX).
      installAxis(w, bw[0] - (int)CENTER, bw[1] - (int)CENTER, bw[2] - (int)CENTER);

      // Dynamic initialisation of Cell::m (inertia): publish the elected
      // axis onto the source-centre cell.  |m| = RMAX.  The inertia path
      // (encounter P×K / P×D, applyMomentum) treats m as immutable and
      // only reads it to accumulate reloc impulses.
      {
        const int* ax = electedAxis(w);
        if (ax)
        {
          const unsigned cx = lcenters[w][0];
          const unsigned cy = lcenters[w][1];
          const unsigned cz = lcenters[w][2];
          Cell& src = getCell(lattice_curr, cx, cy, cz, w);
          src.m[0] = ax[0];
          src.m[1] = ax[1];
          src.m[2] = ax[2];
        }
      }

      // Reset the second value lattice of this layer: 0 = never reached.
      for (unsigned x = 0; x < ELX; ++x)
      for (unsigned y = 0; y < ELY; ++y)
      for (unsigned z = 0; z < ELZ; ++z)
      {
        getCell(lattice_curr, x, y, z, w).bstamp = 0;
      }

      std::memset(g_visit[w].data(), 0, g_visit[w].size());
      initWalker(w);
    }


    // ============================================================
    // Broadcast diffusion — every other tick, alternating sweep order.
    //
    // Default (no /D POLAR_BROADCAST_WAVE): the original monotone max
    // relaxation b(x) <- max(b(x), b(x +/- e_i)); arrival stamps converge
    // to the final walker stamp (the validated reference behaviour).
    //
    // With /D POLAR_BROADCAST_WAVE (experimental): first-arrival distance
    // wave b(x) <- min over the six face neighbours of (b(n) + 1), with 0
    // reserved for "never reached".  The walker stamps the helix with
    // monotonically increasing ticks (one cell per tick), so the converged
    // field is  b(x) = min over helix cells p of (t_p + dist(x,p)), i.e.
    // each cell latches the sweep time of the helix arm nearest to it in
    // space-time.  That preserves the spatial phase gradient (the azimuth
    // of the broadcasted axis) instead of collapsing to the single final
    // stamp, which the max relaxation did.
    // Returns true when at least one stamp changed.
    // ============================================================
    static bool diffusePass(unsigned w)
    {
      Walker& W = g_walk[w];
      const int ELXi = (int)ELX, ELYi = (int)ELY, ELZi = (int)ELZ;
      bool changed = false;

      auto stampAt = [&](int x, int y, int z) -> unsigned int&
      {
        return getCell(lattice_curr, (unsigned)x, (unsigned)y, (unsigned)z, w).bstamp;
      };

#ifdef POLAR_BROADCAST_WAVE
      auto relax = [&](int x, int y, int z)
      {
        unsigned int cur = stampAt(x, y, z);
        unsigned int best = cur;                 // 0 == never reached
        auto upd = [&](int xx, int yy, int zz)
        {
          unsigned int n = stampAt(xx, yy, zz);
          if (n != 0u)
          {
            unsigned int cand = n + 1u;
            if (cand < best) best = cand;
          }
        };
        if (x > 0)             upd(x - 1, y, z);
        if (x < ELXi - 1)      upd(x + 1, y, z);
        if (y > 0)             upd(x, y - 1, z);
        if (y < ELYi - 1)      upd(x, y + 1, z);
        if (z > 0)             upd(x, y, z - 1);
        if (z < ELZi - 1)      upd(x, y, z + 1);
        if (best != cur) { stampAt(x, y, z) = best; changed = true; }
      };
#else
      auto relax = [&](int x, int y, int z)
      {
        unsigned int best = stampAt(x, y, z);
        unsigned int n;
        if (x > 0)             { n = stampAt(x - 1, y, z); if (n > best) best = n; }
        if (x < ELXi - 1)      { n = stampAt(x + 1, y, z); if (n > best) best = n; }
        if (y > 0)             { n = stampAt(x, y - 1, z); if (n > best) best = n; }
        if (y < ELYi - 1)      { n = stampAt(x, y + 1, z); if (n > best) best = n; }
        if (z > 0)             { n = stampAt(x, y, z - 1); if (n > best) best = n; }
        if (z < ELZi - 1)      { n = stampAt(x, y, z + 1); if (n > best) best = n; }
        unsigned int& cur = stampAt(x, y, z);
        if (best != cur) { cur = best; changed = true; }
      };
#endif

      if (W.sweepForward)
      {
        for (int x = 0; x < ELXi; ++x)
        for (int y = 0; y < ELYi; ++y)
        for (int z = 0; z < ELZi; ++z)
          relax(x, y, z);
      }
      else
      {
        for (int x = ELXi - 1; x >= 0; --x)
        for (int y = ELYi - 1; y >= 0; --y)
        for (int z = ELZi - 1; z >= 0; --z)
          relax(x, y, z);
      }
      W.sweepForward = !W.sweepForward;
      return changed;
    }

    // ============================================================
    // tick — per-tick orchestration for every layer.
    // ============================================================
    void tick()
    {
      if (W_USED == 0 || EL == 0 || BLOCK == 0)
        return;

      ensureSized();

      const int RMAXi = (int)RMAX;

      for (unsigned w = 0; w < W_USED; ++w)
      {
        Walker& W = g_walk[w];

        // Expansion limit: the exact tick where the breathing wavefront
        // turns from ascending to descending (source-centre t == RMAX).
        const unsigned cx = lcenters[w][0];
        const unsigned cy = lcenters[w][1];
        const unsigned cz = lcenters[w][2];
        unsigned tCentre  = getCell(lattice_curr, cx, cy, cz, w).t;

        // A bound propeller retains its transport direction. Elect once on
        // the frame edge, not on every housekeeping tick of the turnaround.
        const Cell& source = getCell(lattice_curr, cx, cy, cz, w);
        if (tCentre == (unsigned)RMAXi && source.k == 0 && !isBoundPropeller(source))
          elect(w);

        if (W.live)
          walkerStep(w);

        // Every other automaton tick, spread the news one neighbourhood.
        if (W.diffusing && ((pulse_tick & 1u) == 0u))
        {
          bool changed = diffusePass(w);
          if (!changed && !W.live)
            W.diffusing = false;  // monotonic field converged: coverage done
        }
      }
    }
  } // namespace polarization
} // namespace automaton
