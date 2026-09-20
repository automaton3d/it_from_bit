#ifndef ATTRACTOR_H_
#define ATTRACTOR_H_

/* Read-only frame diagnostics. Default: one source bubble per W, grouped by
 * its live chief (K.w or D.parent). Dangling D parents are unresolved, not
 * islands. AffinityCells is a separate occupancy observable keyed by exact a;
 * it never establishes island identity or a constituent population.
 * Membership flows use consecutive snapshots with no turnaround suppression.
 */

#include <string>
#include <vector>
#include <cstdint>

namespace automaton
{
  namespace attractor
  {
    enum class Observable : uint32_t { ChiefConstituents = 0, AffinityCells = 1 };

    struct Census {
      unsigned chiefs=0, delegates=0, singletons=0, pairs=0;
      unsigned unresolved=0, occupiedCenters=0, groupsOfTarget=0;
    };

    struct IslandStats
    {
      unsigned island  = 0;
      double   meanN   = 0.0;   // mean population
      double   stdN    = 0.0;   // population std deviation
      double   minN    = 0.0;
      double   maxN    = 0.0;
      long     samples = 0;     // number of frames sampled for this island

      // OLS fit of dN(t) = N(t+1) - N(t) against N(t):
      double   slope     = 0.0;  // descriptive coefficient; negative does not prove stability
      double   slopeSE   = 0.0;  // standard error of slope
      double   intercept = 0.0;
      double   r2        = 0.0;
      bool     hasFit    = false;
      double   nStar     = 0.0;  // -intercept/slope when slope < 0

      double   capRate   = 0.0;  // gross captures per frame
      double   escRate   = 0.0;  // gross escapes per frame
    };

    struct Report
    {
      Observable observable = Observable::ChiefConstituents;
      Census census; // latest sample; groupsOfTarget uses EL/3 only when divisible
      unsigned frames    = 0;
      unsigned nIslands  = 0; // live chiefs at the latest sample
      unsigned nBuckets  = 0; // possible W addresses, not an island count
      double   pooledSlope = 0.0, pooledSlopeSE = 0.0;
      double   pooledIntercept = 0.0, pooledR2 = 0.0;
      bool     pooledHasFit = false;
      double   pooledNStar  = 0.0;
      long     pooledPoints = 0;
      double   meanTotalPop = 0.0;   // mean sum of all island populations
      double   meanCaptures = 0.0;   // gross captures per frame (all islands)
      double   meanEscapes  = 0.0;   // gross escapes  per frame (all islands)
      std::vector<IslandStats> islands;
    };

    /// Allocate the previous-state snapshot. Call after initializing the lattice.
    void begin(Observable observable = Observable::ChiefConstituents);

    /// Recompute the previous-state snapshot from lattice_curr WITHOUT
    /// counting events. With history present, rejects mismatched checkpoints.
    void resyncPrev();

    /// Persist the sampled time series (frames_, nBuckets_, pop_, cap_, esc_).
    bool saveSeries(const std::string& path);

    /// Restore the time series previously written by saveSeries().
    /// Returns false on I/O error, legacy format, observable or topology mismatch.
    bool loadSeries(const std::string& path);


    /// Scan lattice_curr after a completed light frame (frame = 1-based id).
    void sampleFrame(unsigned frame);

    /// Number of completed frames sampled so far.
    unsigned framesSampled();

    /// Per-island statistics + pooled regression across all islands.
    Report summarize();

    /// Console report (stdout).
    void printReport(const Report& rep);

    /// Time-series CSV keyed by chief_w (constituents) or affinity (occupied_cells).
    bool writeCSV(const std::string& path, const Report& rep);

    /// Live chief counts, unresolved delegates and spatial diagnostics (chief mode only).
    bool writeCensusCSV(const std::string& path);

    /// Per-frame sector flux (8 capture/escape columns plus two charge conversions).
    bool writeSectorCSV(const std::string& path);
  }
}

#endif /* ATTRACTOR_H_ */
