/*
 * subregion_box.cpp
 *
 * See subregion_box.h.  Pure data: bounds, clamping, counts and the one-line
 * report.  No GL, no text rendering, no model dependency (the cell size is
 * passed in by whoever prints).
 */

#include "subregion_box.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

// 1234567 -> "1,234,567"
std::string thousands(unsigned long long value)
{
    const std::string digits = std::to_string(value);
    std::string out;
    out.reserve(digits.size() + digits.size() / 3);

    for (size_t i = 0; i < digits.size(); ++i)
    {
        if (i > 0 && ((digits.size() - i) % 3) == 0)
            out.push_back(',');
        out.push_back(digits[i]);
    }
    return out;
}

std::string humanBytes(double bytes)
{
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;
    char buf[64];

    if (bytes >= gib)      std::snprintf(buf, sizeof(buf), "%.2f GB", bytes / gib);
    else if (bytes >= mib) std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / mib);
    else                   std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);

    return buf;
}

// The model runs the region as a lattice of its own, and needs odd edges: the
// seed sits on the centre cell and the periodic wrap has to be symmetric (see
// automaton::configureLatticeFromRegion).  A face therefore never produces an
// even edge -- it stays on the parity of the opposite face, which is the same as
// saying that a face moves two cells at a time.
//
// `v` is the requested cell index for the moving face, `ref` the opposite bound
// and [lo, hi] the range that face may take.  There is always a value of the
// right parity in that range, because the range always contains `ref`.
int snapOddExtent(int v, int ref, int lo, int hi)
{
    if (((v - ref) & 1) != 0)
        v += (v > ref) ? 1 : -1;

    if (v < lo) v = lo + (int)(((lo - ref) & 1) != 0);
    if (v > hi) v = hi - (int)(((hi - ref) & 1) != 0);

    return v;
}

} // namespace

// ============================================================================
// Bounds
// ============================================================================

bool SubRegionBox::setLatticeSize(int L)
{
    if (L < 1) L = 1;
    if (L == lattice_) return false;

    // "The whole lattice" is a state, not three numbers: when the side changes,
    // a full region follows it.
    const bool wasFull = isFull();

    lattice_ = L;

    if (wasFull)
    {
        x0_ = y0_ = z0_ = 0;
        x1_ = y1_ = z1_ = L - 1;
        return true;
    }

    const int before[6] = { x0_, x1_, y0_, y1_, z0_, z1_ };
    clampBounds();

    return (before[0] != x0_ || before[1] != x1_ ||
            before[2] != y0_ || before[3] != y1_ ||
            before[4] != z0_ || before[5] != z1_);
}

void SubRegionBox::clampBounds()
{
    const int last = (lattice_ > 0) ? lattice_ - 1 : 0;

    x0_ = std::max(0, std::min(x0_, last));
    x1_ = std::max(0, std::min(x1_, last));
    y0_ = std::max(0, std::min(y0_, last));
    y1_ = std::max(0, std::min(y1_, last));
    z0_ = std::max(0, std::min(z0_, last));
    z1_ = std::max(0, std::min(z1_, last));

    // At least one cell per axis, so the region is never empty.
    if (x1_ < x0_) std::swap(x0_, x1_);
    if (y1_ < y0_) std::swap(y0_, y1_);
    if (z1_ < z0_) std::swap(z0_, z1_);
}

bool SubRegionBox::resetToFull()
{
    if (isFull()) return false;

    x0_ = y0_ = z0_ = 0;
    x1_ = y1_ = z1_ = lattice_ - 1;
    return true;
}

bool SubRegionBox::restore(int xa, int xb, int ya, int yb, int za, int zb)
{
    const int last = lattice_ - 1;

    // Same rules the editing path enforces, applied to a pair of bounds at once:
    // inside the lattice, at least one cell, and an odd extent.  There is no
    // moving face here, so the *far* bound is the one that gives way (it snaps to
    // the parity of the near one), which keeps the region anchored where the file
    // said and is what the editing path does when a face moves.
    struct Axis { int lo, hi; };

    Axis axes[3] = { { xa, xb }, { ya, yb }, { za, zb } };

    for (int a = 0; a < 3; ++a)
    {
        int lo = axes[a].lo;
        int hi = axes[a].hi;

        if (lo > hi) std::swap(lo, hi);                    // inverted: read it as a range
        if (hi < 0 || lo > last) { lo = 0; hi = last; }    // entirely outside

        if (lo < 0)    lo = 0;
        if (hi > last) hi = last;

        if (((hi - lo) & 1) != 0)                          // even extent: make it odd
        {
            if (hi > lo) --hi;                             // prefer keeping the near bound
            else         ++hi;
        }

        axes[a].lo = lo;
        axes[a].hi = hi;
    }

    const bool changed =
        axes[0].lo != x0_ || axes[0].hi != x1_ ||
        axes[1].lo != y0_ || axes[1].hi != y1_ ||
        axes[2].lo != z0_ || axes[2].hi != z1_ ||
        axes[0].lo != xa || axes[0].hi != xb ||
        axes[1].lo != ya || axes[1].hi != yb ||
        axes[2].lo != za || axes[2].hi != zb;

    x0_ = axes[0].lo; x1_ = axes[0].hi;
    y0_ = axes[1].lo; y1_ = axes[1].hi;
    z0_ = axes[2].lo; z1_ = axes[2].hi;

    return changed;
}

bool SubRegionBox::isFull() const
{
    const int last = lattice_ - 1;
    return x0_ == 0 && y0_ == 0 && z0_ == 0 &&
           x1_ == last && y1_ == last && z1_ == last;
}

unsigned long long SubRegionBox::cellsPerLayer() const
{
    return (unsigned long long)(x1_ - x0_ + 1) *
           (unsigned long long)(y1_ - y0_ + 1) *
           (unsigned long long)(z1_ - z0_ + 1);
}

unsigned long long SubRegionBox::cellsFor(int W) const
{
    return cellsPerLayer() * (unsigned long long)((W > 0) ? W : 0);
}


// ============================================================================
// The face the keyboard / the overlay edits
// ============================================================================

bool SubRegionBox::setActiveHandle(Handle h)
{
    if (h == active_) return false;
    active_ = h;
    return true;
}

bool SubRegionBox::cycleHandle(int dir)
{
    if (dir == 0) return false;

    int index = (int)active_ + ((dir > 0) ? 1 : -1);
    if (index < 0) index = kHandleCount - 1;
    if (index >= kHandleCount) index = 0;

    active_ = (Handle)index;
    return true;
}

int SubRegionBox::axisOf(Handle h) const
{
    switch (h)
    {
        case Handle::XMin:
        case Handle::XMax: return 0;
        case Handle::YMin:
        case Handle::YMax: return 1;
        default:           return 2;
    }
}

int SubRegionBox::valueOf(Handle h) const
{
    switch (h)
    {
        case Handle::XMin: return x0_;
        case Handle::XMax: return x1_;
        case Handle::YMin: return y0_;
        case Handle::YMax: return y1_;
        case Handle::ZMin: return z0_;
        default:           return z1_;
    }
}

const char* SubRegionBox::handleName(Handle h) const
{
    switch (h)
    {
        case Handle::XMin: return "X-";
        case Handle::XMax: return "X+";
        case Handle::YMin: return "Y-";
        case Handle::YMax: return "Y+";
        case Handle::ZMin: return "Z-";
        default:           return "Z+";
    }
}

bool SubRegionBox::assignValueOf(Handle h, int value)
{
    const int last = lattice_ - 1;

    switch (h)
    {
        case Handle::XMin: { const int v = snapOddExtent(value, x1_, 0,    x1_);    if (v == x0_) return false; x0_ = v; return true; }
        case Handle::XMax: { const int v = snapOddExtent(value, x0_, x0_,  last);   if (v == x1_) return false; x1_ = v; return true; }
        case Handle::YMin: { const int v = snapOddExtent(value, y1_, 0,    y1_);    if (v == y0_) return false; y0_ = v; return true; }
        case Handle::YMax: { const int v = snapOddExtent(value, y0_, y0_,  last);   if (v == y1_) return false; y1_ = v; return true; }
        case Handle::ZMin: { const int v = snapOddExtent(value, z1_, 0,    z1_);    if (v == z0_) return false; z0_ = v; return true; }
        default:           { const int v = snapOddExtent(value, z0_, z0_,  last);   if (v == z1_) return false; z1_ = v; return true; }
    }
}

bool SubRegionBox::moveActive(int delta)
{
    if (delta == 0) return false;
    return assignValueOf(active_, valueOf(active_) + delta);
}


// ============================================================================
// Text
// ============================================================================

std::string SubRegionBox::boundsLine() const
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "x %d..%d   y %d..%d   z %d..%d",
                  x0_, x1_, y0_, y1_, z0_, z1_);
    return buf;
}
SubRegionBox::Summary SubRegionBox::summarize(int W, unsigned long long cellBytes) const
{
    Summary s;

    const unsigned long long perLayer = cellsPerLayer();
    const unsigned long long l3 =
        (unsigned long long)lattice_ * (unsigned long long)lattice_ *
        (unsigned long long)lattice_;
    const unsigned long long total     = perLayer * (unsigned long long)W;
    const unsigned long long fullTotal = l3 * (unsigned long long)W;

    const double fraction = (l3 > 0) ? (100.0 * (double)perLayer / (double)l3) : 100.0;

    s.bounds = boundsLine();

    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "%s per layer (%.1f%% of L^3)      %s with W = %d",
                      thousands(perLayer).c_str(), fraction,
                      thousands(total).c_str(), W);
        s.cells = buf;
    }

    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "est. %s  of %s  (3 lattices x %llu B/cell)",
                      humanBytes(3.0 * (double)total * (double)cellBytes).c_str(),
                      humanBytes(3.0 * (double)fullTotal * (double)cellBytes).c_str(),
                      cellBytes);
        s.memory = buf;
    }

    return s;
}



std::string SubRegionBox::report(int W, unsigned long long cellBytes) const
{
    const unsigned long long perLayer = cellsPerLayer();
    const unsigned long long fullSide =
        (unsigned long long)lattice_ * (unsigned long long)lattice_ *
        (unsigned long long)lattice_;
    const unsigned long long total     = perLayer * (unsigned long long)W;
    const unsigned long long fullTotal = fullSide * (unsigned long long)W;

    const double fraction = (fullTotal > 0)
        ? (100.0 * (double)total / (double)fullTotal) : 100.0;

    const double subBytes  = 3.0 * (double)total * (double)cellBytes;
    const double fullBytes = 3.0 * (double)fullTotal * (double)cellBytes;

    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "[Subregion] %s | per layer %d x %d x %d = %s of %s cells (%.1f%%) | "
                  "W = %d -> %s of %s cells | est. RAM %s of %s "
                  "(3 lattices x %llu bytes/cell)",
                  boundsLine().c_str(),
                  x1_ - x0_ + 1, y1_ - y0_ + 1, z1_ - z0_ + 1,
                  thousands(perLayer).c_str(), thousands(fullSide).c_str(), fraction,
                  W, thousands(total).c_str(), thousands(fullTotal).c_str(),
                  humanBytes(subBytes).c_str(), humanBytes(fullBytes).c_str(),
                  cellBytes);

    return buf;
}
