/*
 * wavefront.cpp — wavefront-fidelity instrumentation.
 * See wavefront.h for the operational definitions.
 */

#include "model/wavefront.h"
#include "model/simulation.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <array>

namespace automaton
{
  extern unsigned EL;
  extern unsigned W_USED;
  extern unsigned RMAX;
  extern unsigned long BLOCK;
  extern std::vector<std::array<unsigned, 3>> lcenters;

  namespace wavefront
  {
    namespace
    {
      // Theoretical number of integer lattice points on the shell of integer
      // radius r: (dx,dy,dz) with r^2 <= dx^2+dy^2+dz^2 < (r+1)^2.
      long long shellPoints(int r)
      {
        long long n = 0;
        const long long r2 = (long long)r * r;
        const long long r2hi = (long long)(r + 1) * (long long)(r + 1);
        for (int dx = -r; dx <= r; ++dx)
          for (int dy = -r; dy <= r; ++dy)
            for (int dz = -r; dz <= r; ++dz)
            {
              const long long d2 = (long long)dx * dx +
                                   (long long)dy * dy +
                                   (long long)dz * dz;
              if (d2 >= r2 && d2 < r2hi)
                ++n;
            }
        return n;
      }

      // Accumulators, indexed by radius r (0..RMAX).
      std::vector<long long> shellTheory_;   // shellPoints(r)
      std::vector<long long> shellActive_;   // sum of active counts when r was the target
      std::vector<long long> shellTargets_;  // number of frames r was the target of any layer
      std::vector<long long> sumAbsU_;       // sum of |u| over all cells at radius r
      std::vector<long long> cntU_;          // cell count per radius
      double peakErrSum_ = 0.0;              // sum of |peak(r) - target r| per layer-frame
      long long peakErrN_ = 0;               // number of layer-frame samples
      unsigned long long frames_ = 0;

      // Propagation-speed ledger.  During the ascending branch of the
      // breathing clock (source-centre t <= RMAX) we record, per layer,
      // (t, target radius effective_t(t), observed active-shell radius).
      // The active shell is the set of cells with c.active true; its
      // outermost radius is the front.  A constant-speed front must advance
      // the active shell by exactly one cell per tick:
      //   delta(observed_active_radius) = 1 with zero variance.
      struct SpeedSample
      {
        unsigned tick;        // source-centre light clock t at this frame
        unsigned targetR;     // effective_t(t)
        long long  activeR;   // outermost observed radius with c.active
      };
      std::vector<std::vector<SpeedSample>> speedLedger_;   // indexed by layer w
    }

    void begin()
    {
      const size_t n = static_cast<size_t>(RMAX) + 1u;
      shellTheory_.assign(n, 0);
      shellActive_.assign(n, 0);
      shellTargets_.assign(n, 0);
      sumAbsU_.assign(n, 0);
      cntU_.assign(n, 0);
      for (int r = 0; r <= (int)RMAX; ++r)
        shellTheory_[static_cast<size_t>(r)] = shellPoints(r);
      peakErrSum_ = 0.0;
      peakErrN_ = 0;
      frames_ = 0;
      speedLedger_.clear();
      speedLedger_.resize(W_USED);
    }

    void sampleFrame(unsigned)
    {
      if (lattice_curr.empty() || lcenters.size() < W_USED)
        return;

      ++frames_;

      // Per-layer target radius from the source-centre light clock.
      std::vector<unsigned> pulseR(W_USED);
      std::vector<unsigned> srcT(W_USED);
      for (unsigned w = 0; w < W_USED; ++w)
      {
        const std::array<unsigned, 3>& c3 = lcenters[w];
        const Cell& src = getCell(lattice_curr,
                                  (int)c3[0], (int)c3[1], (int)c3[2], (int)w);
        srcT[w]    = src.t;
        pulseR[w]  = effective_t(src.t);
      }

      // Per-radius active count on the target shell (all layers) + |u| profile.
      std::vector<long long> activeObs(RMAX + 1u, 0);   // cells with c.active && c.r == r
      std::vector<long long> absU(RMAX + 1u, 0);
      std::vector<long long> cntU(RMAX + 1u, 0);

      for (unsigned w = 0; w < W_USED; ++w)
      {
        long long activeR = -1;   // outermost radius with c.active in layer w
        for (unsigned x = 0; x < ELX; ++x)
          for (unsigned y = 0; y < ELY; ++y)
            for (unsigned z = 0; z < ELZ; ++z)
            {
              const Cell& c = getCell(lattice_curr, (int)x, (int)y, (int)z, (int)w);
              if (c.r < 0 || c.r > (int)RMAX)
                continue;
              const size_t r = static_cast<size_t>(c.r);
              cntU[r]++;
              absU[r] += (c.u < 0) ? -(long long)c.u : (long long)c.u;
              if (c.active)
              {
                activeObs[r]++;
                if ((long long)r > activeR)
                  activeR = (long long)r;
              }
            }

        // Peak radius of the mean |u| profile for this layer.
        unsigned peakR = 0;
        long long peakA = -1;
        for (unsigned r = 1; r <= RMAX; ++r)
        {
          const size_t i = static_cast<size_t>(r);
          if (cntU[i] > 0)
          {
            const long long meanAbs = absU[i] / cntU[i];
            if (meanAbs > peakA)
            {
              peakA = meanAbs;
              peakR = r;
            }
          }
        }
        const long long d = (long long)peakR - (long long)pulseR[w];
        peakErrSum_ += (d < 0) ? -(double)d : (double)d;
        peakErrN_++;

        // Propagation-speed ledger: on the ascending branch of the breathing
        // clock (t mod 2*RMAX <= RMAX) the front must advance one cell per
        // tick, so record (t, target, observed active-shell radius).
        const unsigned phase = srcT[w] % (2u * RMAX);
        if (phase <= RMAX)
        {
          if (w >= speedLedger_.size())
            speedLedger_.resize(w + 1);
          speedLedger_[w].push_back({srcT[w], pulseR[w], activeR});
        }
      }

      // Accumulate shell activity and |u| profile across frames.
      for (unsigned r = 0; r <= RMAX; ++r)
      {
        const size_t i = static_cast<size_t>(r);
        shellActive_[i] += activeObs[r];
        sumAbsU_[i]     += absU[r];
        cntU_[i]        += cntU[r];
      }
      for (unsigned w = 0; w < W_USED; ++w)
      {
        if (pulseR[w] <= RMAX)
          shellTargets_[static_cast<size_t>(pulseR[w])]++;
      }
    }

    void report()
    {
      printf("\n==============================================================\n");
      printf("WAVEFRONT FIDELITY REPORT\n");
      printf("==============================================================\n");
      printf("lattice: EL=%u W_USED=%u RMAX=%u frames=%llu\n",
             EL, W_USED, RMAX, (unsigned long long)frames_);
      printf("  r   shell_theory  active_observed  completeness  <|u|>\n");
      double compSum = 0.0;
      int compN = 0;
      for (unsigned r = 1; r <= RMAX; ++r)
      {
        const size_t i = static_cast<size_t>(r);
        const long long th = shellTheory_[i];
        const long long tg = shellTargets_[i];
        const double comp = (tg > 0 && th > 0)
          ? (double)shellActive_[i] / ((double)th * (double)tg)
          : 0.0;
        const double meanU = (cntU_[i] > 0)
          ? (double)sumAbsU_[i] / (double)cntU_[i]
          : 0.0;
        printf("  %2u  %11lld  %14lld  %12.4f  %8.2f\n",
               r, (long long)th, (long long)shellActive_[i], comp, meanU);
        if (tg > 0 && th > 0)
        {
          compSum += comp;
          compN++;
        }
      }
      if (compN > 0)
        printf("mean shell completeness (r=1..%u): %.4f\n", RMAX, compSum / compN);

      if (peakErrN_ > 0)
        printf("mean |peak-radius - target-radius|: %.3f cells per layer-frame\n",
               peakErrSum_ / (double)peakErrN_);

      // Measured propagation speed: on the ascending branch of the breathing
      // clock, the outermost active-shell radius (the front) must advance
      // exactly one cell per tick.  For each layer we report the mean and
      // standard deviation of the per-tick increment of the observed front
      // radius; a constant-speed front has mean 1 and std 0.
      printf("measured propagation speed (ascending branch, per layer):\n");
      for (unsigned w = 0; w < speedLedger_.size(); ++w)
      {
        const std::vector<SpeedSample>& s = speedLedger_[w];
        if (s.size() < 2)
          continue;

        // Split the ledger into ascending cycles (t resets to 0 each cycle).
        // Only interior transitions are counted: t must increase and the
        // target radius must increase strictly.  The transition leaving
        // t = 0 (the tick where the light clock resets) is excluded because
        // the observed active shell still carries the previous cycle's shell
        // at that instant, which would spuriously lower the per-tick advance.
        long long sumDR = 0;
        double sumDR2 = 0;
        long long nCyc = 0;
        long long nLag = 0;
        long long sumLag = 0;
        for (size_t i = 1; i < s.size(); ++i)
        {
          const long long dt = (long long)s[i].tick - (long long)s[i - 1].tick;
          const long long lag = s[i].targetR - s[i].activeR;   // clock ahead of shell
          sumLag += lag;
          ++nLag;
          if (dt > 0 && s[i].targetR > s[i - 1].targetR && s[i - 1].tick > 0)
          {
            const long long dr = s[i].activeR - s[i - 1].activeR;
            sumDR += dr;
            sumDR2 += (double)dr * (double)dr;
            ++nCyc;
          }
        }
        if (nCyc > 0)
        {
          const double mean = (double)sumDR / (double)nCyc;
          const double var  = (sumDR2 / (double)nCyc) - mean * mean;
          const double stdv = (var > 0.0) ? std::sqrt(var) : 0.0;
          const double meanLag = (nLag > 0) ? (double)sumLag / (double)nLag : 0.0;
          printf("  layer %u: samples=%zu  front advance per tick: mean=%.4f cells/tick  std=%.4f  (active shell lags clock by %.2f cells)\n",
                 w, s.size(), mean, stdv, meanLag);
        }
      }

      // Pearson correlation of <|u|>(r) with sin(r)/r over r = 1..RMAX.
      {
        long n = 0;
        double sx = 0, sy = 0;
        for (unsigned r = 1; r <= RMAX; ++r)
        {
          const size_t i = static_cast<size_t>(r);
          if (cntU_[i] <= 0) continue;
          const double x = std::sin((double)r) / (double)r;
          const double y = (double)sumAbsU_[i] / (double)cntU_[i];
          sx += x; sy += y; ++n;
        }
        if (n >= 3)
        {
          const double mx = sx / n, my = sy / n;
          double sxx = 0, syy = 0, sxy = 0;
          for (unsigned r = 1; r <= RMAX; ++r)
          {
            const size_t i = static_cast<size_t>(r);
            if (cntU_[i] <= 0) continue;
            const double x = std::sin((double)r) / (double)r;
            const double y = (double)sumAbsU_[i] / (double)cntU_[i];
            const double dx = x - mx, dy = y - my;
            sxx += dx * dx; syy += dy * dy; sxy += dx * dy;
          }
          const double denom = std::sqrt(sxx * syy);
          printf("Pearson <|u|>(r) vs sin(r)/r: %.4f%s\n",
                 (denom > 0.0) ? sxy / denom : 0.0,
                 (denom > 0.0) ? "" : " (degenerate profile)");
        }
      }
      printf("--------------------------------------------------------------\n");
    }
  }
}
