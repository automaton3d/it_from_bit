/*
 * subregion_box.h
 *
 * The region of the L x L x L lattice a run should use: its bounds, the
 * arithmetic on them (clamping, cell counts, memory estimate) and the report
 * line the setup screen prints.
 *
 * Pure data on purpose -- no GL, no text, no model header.  The editing happens
 * in the 3D overlay (subregion_modal.h), which draws this box inside the
 * lattice and moves its six faces; keeping the model separate means the splash
 * can display and print a region without touching GL, and that the overlay can
 * be replaced without losing the numbers.
 *
 * Bounds are INCLUSIVE lattice indices, so the default (0 .. L-1 on every axis)
 * is the whole lattice.
 *
 * Nothing in the model reads the region yet: the simulation still allocates
 * L^3 x W (see README, "Subregion editor").
 */

#ifndef SUBREGION_BOX_H_
#define SUBREGION_BOX_H_

#include <string>

class SubRegionBox
{
public:
    // The six faces, in the order the keyboard cycles through them.
    enum class Handle { XMin = 0, XMax, YMin, YMax, ZMin, ZMax };
    static constexpr int kHandleCount = 6;

    SubRegionBox() = default;

    // Lattice side.  The bounds are reclamped to [0, L-1]; returns true when
    // that changed them (the caller then re-prints the report).
    bool setLatticeSize(int L);
    int  latticeSize() const { return lattice_; }

    int x0() const { return x0_; } int x1() const { return x1_; }
    int y0() const { return y0_; } int y1() const { return y1_; }
    int z0() const { return z0_; } int z1() const { return z1_; }

    // Back to the whole lattice.  True when something changed.
    bool resetToFull();          // true when something changed
    bool isFull() const;

    unsigned long long cellsPerLayer() const;      // dx * dy * dz
    unsigned long long cellsFor(int W) const;      // cellsPerLayer * W

    // ---- the face the keyboard edits ---------------------------------------
    Handle activeHandle() const { return active_; }
    const char* handleName(Handle h) const;        // "X-", "X+", "Y-" ...
    int  axisOf(Handle h) const;                   // 0 = x, 1 = y, 2 = z
    bool setActiveHandle(Handle h);                // true when it changed
    bool cycleHandle(int dir);                     // true when it changed
    int  valueOf(Handle h) const;                  // the current bound of h
    bool assignValueOf(Handle h, int value);       // true when it changed
    bool moveActive(int delta);                    // true when it changed

    // ---- text ---------------------------------------------------------------
    std::string boundsLine() const;                // "x 4..17   y 0..20   z 8..20"

    // What to show about the region, one entry per line (the overlay) and one
    // long line for the log (report()).  Both come from the same numbers, so the
    // screen and the stdout line cannot drift apart.
    struct Summary
    {
        std::string bounds;   // "x 0..10   y 0..20   z 0..20"
        std::string cells;    // "4,851 per layer (52.4% of L^3)   46,200 with W = 10"
        std::string memory;   // "est. 21.1 MB  of 42.4 MB (3 lattices x 160 B/cell)"
    };
    Summary summarize(int W, unsigned long long cellBytes) const;

    // One-line report for stdout.  cellBytes is the caller's sizeof(auto::Cell):
    // the model deliberately has no dependency on the automaton headers.
    std::string report(int W, unsigned long long cellBytes) const;

private:
    void clampBounds();

    int lattice_ = 21;

    // Inclusive bounds in lattice coordinates.
    int x0_ = 0, x1_ = 20;
    int y0_ = 0, y1_ = 20;
    int z0_ = 0, z1_ = 20;

    Handle active_ = Handle::XMax;
};

#endif /* SUBREGION_BOX_H_ */

