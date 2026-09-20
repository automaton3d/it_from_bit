// cuda_automaton.cu - CUDA implementation with FULL CA logic
// Unified version: all constants and host functions are defined here.

#pragma nv_diag_suppress 177

#include "model/simulation.h"
#include "cuda_sim_optimized.h"
#include <cuda_runtime.h>
#include "cuda_constants.h" 
#include <iostream>
#include <algorithm>
#include "config.h"

// ===================================================================
// Constant memory — defined HERE so they are in the same compilation
// unit as the kernels (required without -dc separate compilation).
// ===================================================================
__constant__ unsigned dev_EL;
__constant__ unsigned dev_ELX;
__constant__ unsigned dev_ELY;
__constant__ unsigned dev_ELZ;
__constant__ unsigned dev_W_USED;
__constant__ unsigned dev_RMAX;
__constant__ unsigned dev_CENTER;
__constant__ unsigned dev_lcenters[32][3]; // per-W source centers (host copies each frame)
__constant__ unsigned dev_S2B;             // sieve modulus (host sets; default 16384)
__device__   int      dev_ctrl;

// Global device pointers (defined here, used by bridge)
::CellDevice* d_lattice_curr = nullptr;
::CellDevice* d_lattice_draft = nullptr;
::CellDevice* d_lattice_partner = nullptr;

static bool g_cuda_initialized = false;

#define CUDA_CHECK(call) \
    { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s (code %d)\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err), err); \
            fflush(stderr); \
            return false; \
        } \
    }

#define CUDA_CHECK_VOID(call) \
    { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s (code %d)\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err), err); \
            fflush(stderr); \
        } \
    }

// ===================================================================
// DEVICE HELPER FUNCTIONS
// ===================================================================
static __device__ inline ::CellDevice& d_getCell(::CellDevice* lattice, int x, int y, int z, int w)
{
    return lattice[(((x * dev_ELY + y) * dev_ELZ + z) * dev_W_USED) + w];
}

// Per-source center lookup (read from constant-memory copy of host lcenters).
static __device__ inline void dev_source_center(unsigned w, int& cx, int& cy, int& cz)
{
    cx = (int)dev_lcenters[w][0];
    cy = (int)dev_lcenters[w][1];
    cz = (int)dev_lcenters[w][2];
}

static __device__ inline int dev_shortest_delta(int a, int b, int mod)
{
    int d = b - a;
    int half = mod / 2;
    if (d > half) d -= mod;
    else if (d < -half) d += mod;
    return d;
}

static __device__ inline int dev_sign(int v)
{
    return (v > 0) - (v < 0);
}

static __device__ inline unsigned dev_wrap(int v, int mod)
{
    int r = v % mod;
    if (r < 0) r += mod;
    return (unsigned)r;
}

// Spherical antipodal wrap for spatial coordinates — matches CPU's get_sphere_cell()
static __device__ void dev_spherical_wrap(int& x, int& y, int& z)
{
    // Toroidal wrap into [0, dev_EL)
    if (x < 0) x += (int)dev_ELX;
    if (x >= (int)dev_ELX) x -= (int)dev_ELX;
    if (y < 0) y += (int)dev_ELY;
    if (y >= (int)dev_ELY) y -= (int)dev_ELY;
    if (z < 0) z += (int)dev_ELZ;
    if (z >= (int)dev_ELZ) z -= (int)dev_ELZ;

    int dx = x - (int)dev_CENTER;
    int dy = y - (int)dev_CENTER;
    int dz = z - (int)dev_CENTER;
    int r2 = dx*dx + dy*dy + dz*dz;
    int rmax2 = (int)dev_RMAX * (int)dev_RMAX;

    if (r2 > rmax2) {
        // Antipodal mapping through the centre
        x = 2 * (int)dev_CENTER - x;
        y = 2 * (int)dev_CENTER - y;
        z = 2 * (int)dev_CENTER - z;

        // Wrap again in case the antipode lands outside the array
        if (x < 0) x += (int)dev_ELX;
        if (x >= (int)dev_ELX) x -= (int)dev_ELX;
        if (y < 0) y += (int)dev_ELY;
        if (y >= (int)dev_ELY) y -= (int)dev_ELY;
        if (z < 0) z += (int)dev_ELZ;
        if (z >= (int)dev_ELZ) z -= (int)dev_ELZ;
    }
}

// Neighbor with spherical antipodal wrapping for spatial + periodic for W
static __device__ ::CellDevice d_getNeighbor(::CellDevice* d_curr_lattice,
                                           unsigned x_curr, 
                                           unsigned y_curr, 
                                           unsigned z_curr, 
                                           unsigned w_curr, 
                                           int i) 
{
    static const int disp[8][4] = {
        {+1, 0, 0, 0}, {-1, 0, 0, 0},
        { 0,+1, 0, 0}, { 0,-1, 0, 0},
        { 0, 0,+1, 0}, { 0, 0,-1, 0},
        { 0, 0, 0,+1}, { 0, 0, 0,-1}
    };

    int nx = (int)x_curr + disp[i][0];
    int ny = (int)y_curr + disp[i][1];
    int nz = (int)z_curr + disp[i][2];
    int nw = (int)w_curr + disp[i][3];

    // Antipodal spherical wrapping for spatial coordinates
    dev_spherical_wrap(nx, ny, nz);

    // Additional integer bounds guarantee
    if (nx < 0) nx = 0;
    if (nx >= (int)dev_ELX) nx = (int)dev_ELX - 1;
    if (ny < 0) ny = 0;
    if (ny >= (int)dev_ELY) ny = (int)dev_ELY - 1;
    if (nz < 0) nz = 0;
    if (nz >= (int)dev_ELZ) nz = (int)dev_ELZ - 1;

    // Periodic wrapping for W dimension
    nw = nw % (int)dev_W_USED;
    if (nw < 0) nw += (int)dev_W_USED;

    return d_getCell(d_curr_lattice, nx, ny, nz, nw);
}

#define ZERO_C(c) (!(c[0] | c[1] | c[2]))

// Charge-bit access for CellDevice (mirrors CPU)
#define DEV_W1(cell)  ((cell).ch & 0x20)
#define DEV_W0(cell)  ((cell).ch & 0x10)
#define DEV_Q(cell)   ((cell).ch & 0x08)
#define DEV_C2(cell)  ((cell).ch & 0x04)
#define DEV_C1(cell)  ((cell).ch & 0x02)
#define DEV_C0(cell)  ((cell).ch & 0x01)
#define DEV_COLOR(cell)     ((cell).ch & COLOR_MASK)
#define DEV_ANTICOLOR(cell) ((~(cell).ch) & COLOR_MASK)

// Color neutrality test (mirrors CPU neutralColor)
static __device__ inline bool dev_neutralColor(const ::CellDevice& a, const ::CellDevice& b)
{
    int color_a = a.ch & 0x07;
    int color_b = b.ch & 0x07;
    return (color_a ^ color_b) == 0x07;
}

// Weak neutrality test (mirrors CPU neutralWeak)
static __device__ inline bool dev_neutralWeak(const ::CellDevice& a, const ::CellDevice& b)
{
    int weak_a = (a.ch >> 3) & 0x03;
    int weak_b = (b.ch >> 3) & 0x03;
    return (weak_a ^ weak_b) == 0x03;
}

// Simple hash PRNG (for encounter1 random c[] values)
__device__ inline unsigned dev_hash_random(unsigned seed, unsigned mod)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    seed *= 2654435761u;
    return seed % mod;
}

// Device helper: effective wavefront radius (triangle wave)
static __device__ inline unsigned dev_effective_t(unsigned t) {
    unsigned period = 2 * dev_RMAX;
    unsigned raw = t % period;
    return (raw <= dev_RMAX) ? raw : (2 * dev_RMAX - raw);
}

// Device integer square root (table-free)
static __device__ inline int dev_isqrt(int n)
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

// Compute the next (u,v) for one cell using the 3-D integer wave equation.
static __device__ inline void dev_phase_step_cell(
    ::CellDevice& c,
    unsigned w,
    ::CellDevice* src,
    unsigned x, unsigned y, unsigned z,
    unsigned pulse_tick)
{
    if (dev_RMAX == 0)
    {
        c.u = 0; c.v = 0; c.active = 0;
        c.phiB = 0; c.pB = 0; c.sB = 0; c.s2B = 0;
        return;
    }

    int cx, cy, cz;
    dev_source_center(w, cx, cy, cz);
    int dx = (int)c.x[0] - cx;
    int dy = (int)c.x[1] - cy;
    int dz = (int)c.x[2] - cz;
    int r2_int = dx*dx + dy*dy + dz*dz;
    if (r2_int < 0) r2_int = 0;
    c.r2 = (uint32_t)r2_int;

    // Update integer radius from exact r^2 without isqrt.
    // Source centers move at most one cell per frame, so the previous r
    // is an excellent starting estimate; correct it with at most a few
    // square comparisons.
    int r = c.r;
    if (r < 0) r = 0;
    while (r > 0 && (uint32_t)r * (uint32_t)r > c.r2)
        r--;
    while ((uint32_t)(r + 1) * (uint32_t)(r + 1) <= c.r2)
        r++;
    c.r = r;

    // Wave parameters (same scaling as the former SincWave test).
    int R = (dev_RMAX > 0u) ? (int)dev_RMAX : 1;
    int shellR = (int)((dev_RMAX * 24u) / 100u);
    int shellW = (int)(dev_RMAX / 5u);
    if (shellW < 1) shellW = 1;
    int absorbW = (R / 27 > 2) ? (R / 27) : 2;

    int diffDivShift = 2;
    if (R >= 384)       diffDivShift = 6;
    else if (R >= 192)  diffDivShift = 5;
    else if (R >= 96)   diffDivShift = 4;
    else if (R >= 40)   diffDivShift = 3;

    int velDampShift = diffDivShift + 3;
    const int DIFF_SHIFT = 4;

    // Active wavefront: shell of integer radius pulseR moving at one cell per
    // light frame.  c.r is corrected by square-boundary tests above, so no
    // sqrt/isqrt is needed here.
    unsigned int pulseR = dev_effective_t(c.t);
    bool active = (c.r2 != 0xFFFFFFFFu && c.r >= 0 && c.r == (int)pulseR);

    // Hard zero on spatial boundaries and outside the processed sphere,
    // except for the source-center cell (r2 == 0) which may sit on a face.
    if (c.r2 != 0 && (x == 0 || x == dev_ELX - 1 ||
        y == 0 || y == dev_ELY - 1 ||
        z == 0 || z == dev_ELZ - 1 ||
        c.r < 0 || c.r >= R))
    {
        c.u = 0; c.v = 0;
        c.active = active ? 1u : 0u;
        c.phiB   = c.active;
        c.pB = 0; c.sB = 0; c.s2B = 0;
        return;
    }

    int u = c.u;
    int v = c.v;

    int neighbors_u = 0;
    if (x + 1 < dev_ELX) neighbors_u += d_getCell(src, (int)x + 1, (int)y, (int)z, (int)w).u;
    if (x > 0)          neighbors_u += d_getCell(src, (int)x - 1, (int)y, (int)z, (int)w).u;
    if (y + 1 < dev_ELY) neighbors_u += d_getCell(src, (int)x, (int)y + 1, (int)z, (int)w).u;
    if (y > 0)          neighbors_u += d_getCell(src, (int)x, (int)y - 1, (int)z, (int)w).u;
    if (z + 1 < dev_ELZ) neighbors_u += d_getCell(src, (int)x, (int)y, (int)z + 1, (int)w).u;
    if (z > 0)          neighbors_u += d_getCell(src, (int)x, (int)y, (int)z - 1, (int)w).u;

    int lap = neighbors_u - 6 * u;

    int diffShift = DIFF_SHIFT + 1 - (r >> diffDivShift);
    if (diffShift < DIFF_SHIFT - 1)
        diffShift = DIFF_SHIFT - 1;

    int v_new = v + (lap >> diffShift);
    int u_new = u + v_new;
    v_new -= (v_new >> velDampShift);

    // Spherical-shell source forcing.
    int dr = r - shellR;
    if (dr < 0) dr = -dr;
    if (dr <= shellW)
    {
        const int AMP_CAP = 16384;   // shell amplitude cap (not the s2B sieve)
        if (u > AMP_CAP)
            v_new -= (u - AMP_CAP) >> 4;
        else if ((pulse_tick & 3u) == 0u)
            v_new += ((AMP_CAP - u) >> 10) + 1;
    }

    // Absorbing outer boundary.
    if (R > absorbW && r > R - absorbW)
    {
        int dist = r - (R - absorbW);
        if (dist >= absorbW)
            u_new = 0;
        else if (dist > 0)
            u_new /= (1 << dist);
    }

    // Global damping.
    u_new -= (u_new >> 12);
    v_new -= (v_new >> 12);

    // Per-w angular offset so different W copies see distinct pB/sB patterns
    // while the underlying (u,v) wave field stays the same for all layers.
    int helixR = (int)dev_RMAX;
    unsigned int phase_full = 2u * (unsigned int)helixR * (unsigned int)helixR;
    unsigned int w_offset = (unsigned int)(((unsigned long long)w * (unsigned long long)phase_full) / (unsigned long long)dev_W_USED);
    unsigned int cell_phase = w_offset % phase_full;
    int m = (int)(cell_phase / (unsigned int)helixR);
    int cos_w, sin_w;
    if (m < helixR)
    {
        int arg = m * (helixR - m);
        int s = dev_isqrt(arg);
        cos_w = helixR - 2 * m;
        sin_w = 2 * s;
    }
    else
    {
        int m2 = m - helixR;
        int arg = m2 * (helixR - m2);
        int s = dev_isqrt(arg);
        cos_w = 2 * m - 3 * helixR;
        sin_w = -2 * s;
    }

    int ru = (u_new * cos_w - v_new * sin_w) / helixR;
    int rv = (u_new * sin_w + v_new * cos_w) / helixR;

    c.u      = u_new;
    c.v      = v_new;
    c.active = active ? 1u : 0u;
    c.phiB   = c.active;
    c.pB     = (ru > 0) ? 1 : 0;
    c.sB     = (rv > 0) ? 1 : 0;

    // Sieve trigger: probability proportional to positive wave amplitude.
    bool s2B_trigger = false;
    if (u_new > 0)
    {
        int64_t prod = (int64_t)u_new * (int64_t)(pulse_tick + 1);
        int64_t mod = prod % (int64_t)dev_S2B;
        if (mod < (int64_t)u_new) s2B_trigger = true;
    }
    c.s2B    = (active && s2B_trigger) ? 1u : 0u;
}

// ===================================================================
// PHASE STEP KERNEL — run before ca_update_kernel each tick
// ===================================================================
__global__ void phase_step_kernel(::CellDevice* src, ::CellDevice* dst, unsigned pulse_tick)
{
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (dev_EL == 0 || dev_W_USED == 0) return;

    unsigned total = dev_ELX * dev_ELY * dev_ELZ * dev_W_USED;
    if (tid >= total) return;

    unsigned w = tid % dev_W_USED;
    unsigned idx3d = tid / dev_W_USED;
    unsigned z = idx3d % dev_ELZ;
    unsigned y = (idx3d / dev_ELZ) % dev_ELY;
    unsigned x = idx3d / (dev_ELY * dev_ELZ);

    ::CellDevice c = d_getCell(src, (int)x, (int)y, (int)z, (int)w);
    dev_phase_step_cell(c, w, src, x, y, z, pulse_tick);
    d_getCell(dst, (int)x, (int)y, (int)z, (int)w) = c;
}

// ===================================================================
// DEVICE ENCOUNTER FUNCTIONS (partner interaction.cpp encounter())
// ===================================================================

__device__ inline void dev_encounter0(::CellDevice& /*curr*/, ::CellDevice& /*draft*/,
                                      ::CellDevice& /*partner*/, unsigned /*w*/, unsigned /*tid*/)
{
    // Scenario 0: no interaction
}

__device__ inline void dev_encounter1(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*partner*/, unsigned w, unsigned tid)
{
    if (curr.active && dev_effective_t(curr.t) == dev_RMAX / 2 && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1)
        {
            draft.c[0] = dev_hash_random(tid * 3 + 1, dev_EL);
            draft.c[1] = dev_hash_random(tid * 3 + 2, dev_EL);
            draft.c[2] = dev_hash_random(tid * 3 + 3, dev_EL);
        }
    }
}

__device__ inline void dev_encounter2(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*partner*/, unsigned w, unsigned /*tid*/)
{
    if (curr.active && dev_effective_t(curr.t) == dev_RMAX / 2 && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1) draft.a = dev_W_USED;
    }
}

__device__ inline void dev_encounter3(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*partner*/, unsigned w, unsigned /*tid*/)
{
    if (curr.active && dev_effective_t(curr.t) == dev_RMAX / 2 && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1)
        {
            draft.a = dev_W_USED;
            draft.leader_w = DEV_NO_LEADER_W;
            draft.cB = 1;
        }
    }
}

__device__ inline void dev_encounter4(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*partner*/, unsigned w, unsigned /*tid*/)
{
    if (curr.active && dev_effective_t(curr.t) == dev_RMAX / 2 && curr.sB && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1) draft.homB = 1;
    }
}

__device__ inline void dev_encounter5(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*partner*/, unsigned w, unsigned /*tid*/)
{
    if (curr.active && dev_effective_t(curr.t) == dev_RMAX / 2 && curr.pB && w == 0 &&
        !curr.cB && curr.a != dev_W_USED)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1)
        {
            draft.c[0] = curr.x[0];
            draft.c[1] = curr.x[1];
            draft.c[2] = curr.x[2];
            draft.cB = 1;
            draft.a = dev_W_USED;
            draft.leader_w = DEV_NO_LEADER_W;
        }
    }
}

__device__ inline void dev_encounter6(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& partner, unsigned /*w*/, unsigned /*tid*/)
{
    // Cells awaken?
    if (curr.active && partner.active)
    {
        // Test superposition
        if (curr.x[0] == partner.x[0] &&
            curr.x[1] == partner.x[1] &&
            curr.x[2] == partner.x[2])
        {
            // Test dispersion
            if (curr.a != dev_W_USED &&
                DEV_W1(curr) != DEV_W1(partner) &&
                !curr.cB &&
                dev_effective_t(curr.t) == dev_RMAX / 2)
            {
                if (curr.pB && partner.sB)
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.cB = 1;
                    draft.a = dev_W_USED;
                    draft.leader_w = DEV_NO_LEADER_W;
                }
                else if (curr.sB && !partner.pB)
                {
                    draft.homB = 1;
                    draft.cB = 1;
                    draft.a = dev_W_USED;
                    draft.leader_w = DEV_NO_LEADER_W;
                }
            }
        }
    }
}

__device__ inline void dev_encounter7_legacy(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& partner, unsigned /*w*/, unsigned /*tid*/,
                                      ::CellDevice* /*d_curr*/, ::CellDevice* /*d_draft*/)
{
    // Cells awaken?
    if (curr.active && partner.active)
    {
        // --- A) SAME POSITION (superposition) ---
        if (curr.x[0] == partner.x[0] &&
            curr.x[1] == partner.x[1] &&
            curr.x[2] == partner.x[2])
        {
            // Test dispersion
            if (DEV_W1(curr) != DEV_W1(partner) &&
                dev_effective_t(curr.t) == dev_RMAX / 2 &&
                !curr.cB && curr.a != dev_W_USED)
            {
                // Who has the pB true interacts once
                if (curr.pB && !partner.pB)
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.cB = 1;
                }
                // Who has pB false interacts with the last pB true
                if (!curr.pB && partner.pB)
                {
                    draft.homB = 1;
                    draft.cB = 1;
                }
            }
            // Test single pair
            else if (curr.f == curr.t && partner.f == partner.t)
            {
                // Different sectors?
                if (DEV_W1(curr) != DEV_W1(partner))
                {
                    // Momentum (Graviton)
                    if (curr.pB && partner.pB)
                    {
                        draft.f += curr.t;
                        draft.s2B &= curr.phiB;
                        draft.a = min(curr.a, partner.a);
                    }
                }
                else if ((DEV_Q(curr)  ^ DEV_Q(partner))  &&
                         (DEV_W1(curr) == DEV_W1(partner)) &&
                         (DEV_W0(curr) ^ DEV_W0(partner))  &&
                         (DEV_C2(curr) == DEV_C2(partner)) &&
                         (DEV_C1(curr) == DEV_C1(partner)) &&
                         (DEV_C0(curr) == DEV_C0(partner)))
                {
                    // Photon
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, partner.a);
                    draft.bB = 1;
                }
                else if ((curr.ch == 0 && partner.ch == 0) ||
                         (curr.ch == 63 && partner.ch == 63))
                {
                    // Neutrino
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, partner.a);
                }
                else if ((!DEV_Q(curr) && !DEV_Q(partner)) &&
                         (!DEV_W1(curr) && !DEV_W1(partner)) &&
                         (DEV_W0(curr) && DEV_W0(partner)) &&
                         (DEV_COLOR(curr) == DEV_COLOR(partner)) &&
                         (DEV_COLOR(curr) != 0 && DEV_COLOR(curr) != 7))
                {
                    // Boson W-
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, partner.a);
                    draft.bB = 1;
                }
                else if ((DEV_Q(curr) && DEV_Q(partner)) &&
                         (DEV_W1(curr) && DEV_W1(partner)) &&
                         (!DEV_W0(curr) && !DEV_W0(partner)) &&
                         (DEV_COLOR(curr) == DEV_COLOR(partner)) &&
                         (DEV_COLOR(curr) != 0 && DEV_COLOR(curr) != 7))
                {
                    // Boson W+
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, partner.a);
                    draft.bB = 1;
                }
                else if ((DEV_Q(curr) != DEV_Q(partner)) &&
                         (DEV_W1(curr) && DEV_W1(partner)) &&
                         (!DEV_W0(curr) && !DEV_W0(partner)) &&
                         (DEV_COLOR(curr) == DEV_COLOR(partner)) &&
                         (DEV_COLOR(curr) != 0 && DEV_COLOR(curr) != 7))
                {
                    // Boson Z
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, partner.a);
                    draft.bB = 1;
                }
            }
            // Blob formation
            else if (curr.f != curr.t && partner.f != partner.t && curr.bB)
            {
                draft.f += curr.f + partner.f;
                draft.s2B &= curr.phiB;
                draft.a = min(curr.a, partner.a);
            }
        }
        // --- B) DIFFERENT POSITION (distinct bubbles) ---
        else
        {
            // Same sector
            if (DEV_W1(curr) == DEV_W1(partner))
            {
                // Annihilation?
                if (!curr.kB && !partner.kB &&
                    DEV_Q(curr) != DEV_Q(partner) &&
                    DEV_W0(curr) != DEV_W0(partner) &&
                    DEV_COLOR(curr) == DEV_ANTICOLOR(partner) &&
                    curr.f == curr.t && partner.f == partner.t)
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.kB = 1;
                    draft.a = curr.x[3];
                    draft.leader_w = curr.x[3];
                }
                // Fermion cohesion?
                else if (curr.ch == partner.ch &&
                         curr.f == curr.t && partner.f == partner.t)
                {
                    if (curr.c[3] > partner.c[3])
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        draft.a = min(curr.a, partner.a);
                    }
                    else
                    {
                        draft.homB = 1;
                        draft.a = min(curr.a, partner.a);
                    }
                }
                // Same affinity?
                else if (curr.a == partner.a)
                {
                    if (curr.pB && !partner.pB)
                    {
                        draft.c[0] = curr.c[0];
                        draft.c[1] = curr.c[1];
                        draft.c[2] = curr.c[2];
                    }
                    // Parallel transport?
                    else if (!curr.pB && partner.pB)
                    {
                        draft.c[0] = dev_ELX + (curr.x[0] - partner.x[0]) % dev_ELX;
                        draft.c[1] = dev_ELY + (curr.x[1] - partner.x[1]) % dev_ELY;
                        draft.c[2] = dev_ELZ + (curr.x[2] - partner.x[2]) % dev_ELZ;
                    }
                }
                // Strong interaction
                else if (dev_neutralColor(curr, partner))
                {
                    // Gluon x gluon
                    if (curr.f > curr.t && partner.f > partner.t)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        draft.ch = (curr.ch & ~COLOR_MASK) | (partner.ch & COLOR_MASK);
                    }
                    // Quark x gluon
                    else if (curr.f == curr.t && partner.f > partner.t)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        draft.ch = (curr.ch & ~COLOR_MASK) | (partner.ch & COLOR_MASK);
                    }
                }
                // Electroweak interaction: Harmonic?
                else if (curr.phiB && partner.phiB)
                {
                    // Weak interaction
                    if (dev_neutralWeak(curr, partner))
                    {
                        if ((curr.pB && !partner.pB) || (curr.sB && partner.sB))
                        {
                            draft.c[0] = curr.x[0];
                            draft.c[1] = curr.x[1];
                            draft.c[2] = curr.x[2];
                            draft.kB = 1;
                        }
                    }
                    // Electric interaction
                    else if (curr.pB)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        if (partner.pB)
                        {
                            draft.kB = 1;
                        }
                        else
                        {
                            draft.a = partner.a;
                            draft.leader_w = (partner.a == dev_W_USED ? DEV_NO_LEADER_W : partner.a);
                            draft.t = partner.t;
                            draft.c[0] = partner.x[0];
                            draft.c[1] = partner.x[1];
                            draft.c[2] = partner.x[2];
                        }
                    }
                    // Magnetic interaction
                    else if (curr.sB)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        if (partner.sB)
                        {
                            draft.kB = 1;
                        }
                        else
                        {
                            draft.a = partner.a;
                            draft.leader_w = (partner.a == dev_W_USED ? DEV_NO_LEADER_W : partner.a);
                            draft.t = partner.t;
                            draft.c[0] = partner.x[0];
                            draft.c[1] = partner.x[1];
                            draft.c[2] = partner.x[2];
                        }
                    }
                }
            }
        }
    }
    // Different sectors
    else
    {
        // Singularization
        if (curr.ch == ((~partner.ch) & CHARGE_MASK))
        {
            draft.c[0] = curr.x[0];
            draft.c[1] = curr.x[1];
            draft.c[2] = curr.x[2];
            draft.a = curr.x[3];
            draft.leader_w = curr.x[3];
        }
        // Electroweak interaction: Harmonic?
        else if (curr.phiB && partner.phiB)
        {
            // Weak interaction
            if (dev_neutralWeak(curr, partner))
            {
                if ((curr.pB && !partner.pB) || (curr.sB && partner.sB))
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.kB = 1;
                }
            }
            // Electric interaction
            else if (curr.pB)
            {
                draft.c[0] = curr.x[0];
                draft.c[1] = curr.x[1];
                draft.c[2] = curr.x[2];
                if (partner.pB)
                {
                    draft.kB = 1;
                }
            }
            // Magnetic interaction
            else if (curr.sB)
            {
                draft.c[0] = curr.x[0];
                draft.c[1] = curr.x[1];
                draft.c[2] = curr.x[2];
                if (partner.sB)
                {
                    draft.kB = 1;
                }
            }
        }
    }
}

// ===================================================================
// K/S/D/P SOURCE-INTERACTION HELPERS
// ===================================================================

static __device__ inline int dev_Q(const ::CellDevice& c)
{
    return (c.ch & 0x08) ? 1 : 0;
}

static __device__ inline ::CellDevice& dev_source_center_cell(::CellDevice* lattice, int w)
{
    int cx, cy, cz;
    dev_source_center((unsigned)w, cx, cy, cz);
    return d_getCell(lattice, cx, cy, cz, w);
}

static __device__ inline void dev_reemitSourceAt(::CellDevice& srcDraft,
                                                  int dx, int dy, int dz,
                                                  ::CellDevice* /*d_draft*/)
{
    // Preserve the long-term momentum direction m; accumulate the pending
    // displacement in the consumable relocation vector reloc.
    srcDraft.reloc[0] += dx;
    srcDraft.reloc[1] += dy;
    srcDraft.reloc[2] += dz;
    srcDraft.t = 0;
    srcDraft.f = 0;
}

static __device__ inline void dev_moveOneStep(::CellDevice& srcDraft,
                                              int fromCx, int fromCy, int fromCz,
                                              int toCx, int toCy, int toCz,
                                              ::CellDevice* d_draft)
{
    int dx = dev_sign(dev_shortest_delta(fromCx, toCx, (int)dev_ELX));
    int dy = dev_sign(dev_shortest_delta(fromCy, toCy, (int)dev_ELY));
    int dz = dev_sign(dev_shortest_delta(fromCz, toCz, (int)dev_ELZ));
    dev_reemitSourceAt(srcDraft, dx, dy, dz, d_draft);
}

static __device__ inline void dev_moveOneStepAway(::CellDevice& srcDraft,
                                                  int selfCx, int selfCy, int selfCz,
                                                  int otherCx, int otherCy, int otherCz,
                                                  ::CellDevice* d_draft)
{
    int dx = dev_sign(dev_shortest_delta(otherCx, selfCx, (int)dev_ELX));
    int dy = dev_sign(dev_shortest_delta(otherCy, selfCy, (int)dev_ELY));
    int dz = dev_sign(dev_shortest_delta(otherCz, selfCz, (int)dev_ELZ));
    dev_reemitSourceAt(srcDraft, dx, dy, dz, d_draft);
}

static __device__ inline void dev_reemitAtContact(::CellDevice& srcDraft,
                                                  const ::CellDevice& contact,
                                                  ::CellDevice* d_draft)
{
    int dx = dev_shortest_delta((int)srcDraft.x[0], (int)contact.x[0], (int)dev_ELX);
    int dy = dev_shortest_delta((int)srcDraft.x[1], (int)contact.x[1], (int)dev_ELY);
    int dz = dev_shortest_delta((int)srcDraft.x[2], (int)contact.x[2], (int)dev_ELZ);
    dev_reemitSourceAt(srcDraft, dx, dy, dz, d_draft);
}

static __device__ inline bool dev_canFormPair(const ::CellDevice& a, const ::CellDevice& b)
{
    uint8_t ca = a.ch;
    uint8_t cb = b.ch;
    if (ca == 0x00 && cb == 0x00) return true;
    if (ca == 0x3F && cb == 0x3F) return true;
    if ((ca ^ cb) == 0x3F) return true;

    bool qa = (ca & 0x08) != 0;
    bool qb = (cb & 0x08) != 0;
    bool w1a = (ca & 0x20) != 0;
    bool w1b = (cb & 0x20) != 0;
    bool w0a = (ca & 0x10) != 0;
    bool w0b = (cb & 0x10) != 0;
    uint8_t cola = ca & 0x07;
    uint8_t colb = cb & 0x07;

    if ((qa ^ qb) && (w1a == w1b) && (w0a ^ w0b) && ((cola ^ colb) == 0x07)) return true;
    if (!qa && !qb && !w1a && !w1b && w0a && w0b && cola == colb && cola != 0x00 && cola != 0x07) return true;
    if (qa && qb && w1a && w1b && !w0a && !w0b && cola == colb && cola != 0x00 && cola != 0x07) return true;
    return false;
}

static __device__ inline void dev_adoptLeader(::CellDevice& dst, uint32_t leader)
{
    dst.leader_w = leader;
    dst.a = leader;
}

static __device__ inline uint32_t dev_dominantLeader(const ::CellDevice& a, const ::CellDevice& b)
{
    uint32_t leader = (a.leader_w < b.leader_w) ? a.leader_w : b.leader_w;
    if (leader == DEV_NO_LEADER_W)
        leader = (a.x[3] < b.x[3]) ? a.x[3] : b.x[3];
    return leader;
}

__device__ inline void dev_encounter7(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& partner, unsigned /*w*/, unsigned /*tid*/,
                                      ::CellDevice* d_curr, ::CellDevice* d_draft)
{
    if (!curr.active || !partner.active)
        return;
    if (curr.x[3] == partner.x[3])
        return;

    // Sieve: the electroweak interaction channel is only active where s2B is set.
    if (curr.s2B == 0)
        return;

    int currW   = (int)curr.x[3];
    int mirrorW = (int)partner.x[3];

    ::CellDevice& currSrc   = dev_source_center_cell(d_curr, currW);
    ::CellDevice& partnerSrc = dev_source_center_cell(d_curr, mirrorW);
    ::CellDevice& currDraft = dev_source_center_cell(d_draft, currW);
    ::CellDevice& partnerDraft = dev_source_center_cell(d_draft, mirrorW);

    int currCx, currCy, currCz;
    dev_source_center((unsigned)currW, currCx, currCy, currCz);
    int mirrorCx, mirrorCy, mirrorCz;
    dev_source_center((unsigned)mirrorW, mirrorCx, mirrorCy, mirrorCz);

    bool samePos = (curr.x[0] == partner.x[0] &&
                    curr.x[1] == partner.x[1] &&
                    curr.x[2] == partner.x[2]);
    bool sameT   = (curr.t == partner.t);

    // Pair formation (photon-like P sources).
    if (samePos && sameT && dev_canFormPair(currSrc, partnerSrc))
    {
        bool dressing = (currSrc.leader_w != DEV_NO_LEADER_W &&
                         currSrc.leader_w == partnerSrc.leader_w);
        uint32_t newLeader = dressing ? currSrc.leader_w : DEV_NO_LEADER_W;
        uint32_t newA      = dressing ? newLeader : dev_W_USED;
        uint32_t parent    = dressing ? currSrc.leader_w : DEV_NO_PARENT;

        bool alreadyPaired = (currSrc.kind == SRC_P &&
                              partnerSrc.kind == SRC_P &&
                              currSrc.pair_idx == (uint32_t)mirrorW &&
                              partnerSrc.pair_idx == (uint32_t)currW);
        if (alreadyPaired)
            return;

        uint32_t newCount = 1;
        if (currSrc.kind == SRC_P) newCount += currSrc.pair_count;
        if (partnerSrc.kind == SRC_P) newCount += partnerSrc.pair_count;

        currDraft.kind  = SRC_P;
        partnerDraft.kind = SRC_P;
        currDraft.pair_idx   = (uint32_t)mirrorW;
        partnerDraft.pair_idx = (uint32_t)currW;
        currDraft.pair_count = newCount;
        partnerDraft.pair_count = newCount;
        currDraft.leader_w  = newLeader;
        partnerDraft.leader_w = newLeader;
        currDraft.a  = newA;
        partnerDraft.a = newA;
        currDraft.parent  = parent;
        partnerDraft.parent = parent;

        dev_reemitAtContact(currDraft, curr, d_draft);
        dev_reemitAtContact(partnerDraft, partner, d_draft);
        return;
    }

    // pB triggers the electric channel, sB the magnetic channel.
    bool electricContact  = curr.pB || partner.pB;
    bool magneticContact  = curr.sB || partner.sB;
    bool electricCollapse = curr.pB && partner.pB;
    bool magneticCollapse = curr.sB && partner.sB;
    bool collapse         = electricCollapse || magneticCollapse;

    if (!electricContact && !magneticContact)
        return;

    if (collapse)
    {
        draft.kB = 1;
        draft.cB = 1;
    }
    else
    {
        uint32_t minLeader = dev_dominantLeader(currSrc, partnerSrc);
        dev_adoptLeader(currDraft, minLeader);
        dev_adoptLeader(partnerDraft, minLeader);
        uint32_t tmpT = currDraft.t;
        currDraft.t  = partnerDraft.t;
        partnerDraft.t = tmpT;

        dev_moveOneStep(currDraft,  currCx,  currCy,  currCz,
                        mirrorCx, mirrorCy, mirrorCz, d_draft);
        dev_moveOneStep(partnerDraft, mirrorCx, mirrorCy, mirrorCz,
                        currCx,  currCy,  currCz, d_draft);
        return;
    }

    // 1. K x K
    if (currSrc.kind == SRC_K && partnerSrc.kind == SRC_K)
    {
        dev_moveOneStepAway(currDraft, currCx, currCy, currCz,
                            mirrorCx, mirrorCy, mirrorCz, d_draft);
        return;
    }

    // 2. S x K (current = S, partner = K)
    if (currSrc.kind == SRC_S && partnerSrc.kind == SRC_K)
    {
        currDraft.kind = SRC_D;
        currDraft.parent = (uint32_t)mirrorW;
        dev_adoptLeader(currDraft, partnerSrc.leader_w == DEV_NO_LEADER_W ? (uint32_t)mirrorW : partnerSrc.leader_w);
        currDraft.spin_target = 1;
        dev_moveOneStep(currDraft, currCx, currCy, currCz,
                        mirrorCx, mirrorCy, mirrorCz, d_draft);
        return;
    }

    // 3. S x S
    if (currSrc.kind == SRC_S && partnerSrc.kind == SRC_S)
    {
        if (dev_Q(currSrc) == dev_Q(partnerSrc))
        {
            dev_moveOneStepAway(currDraft, currCx, currCy, currCz,
                                mirrorCx, mirrorCy, mirrorCz, d_draft);
        }
        else
        {
            uint32_t leader = dev_dominantLeader(currSrc, partnerSrc);
            currDraft.kind  = SRC_D;
            partnerDraft.kind = SRC_D;
            currDraft.parent  = leader;
            partnerDraft.parent = leader;
            dev_adoptLeader(currDraft, leader);
            dev_adoptLeader(partnerDraft, leader);
        }
        return;
    }

    // 4. S x D / D x S
    if ((currSrc.kind == SRC_S && partnerSrc.kind == SRC_D) ||
        (currSrc.kind == SRC_D && partnerSrc.kind == SRC_S))
    {
        ::CellDevice* sDraft  = (currSrc.kind == SRC_S ? &currDraft : &partnerDraft);
        ::CellDevice* dDraft  = (currSrc.kind == SRC_S ? &partnerDraft : &currDraft);
        ::CellDevice* dSrc    = (currSrc.kind == SRC_S ? &partnerSrc : &currSrc);

        int sCx = (currSrc.kind == SRC_S ? currCx : mirrorCx);
        int sCy = (currSrc.kind == SRC_S ? currCy : mirrorCy);
        int sCz = (currSrc.kind == SRC_S ? currCz : mirrorCz);
        int dCx = (currSrc.kind == SRC_S ? mirrorCx : currCx);
        int dCy = (currSrc.kind == SRC_S ? mirrorCy : currCy);
        int dCz = (currSrc.kind == SRC_S ? mirrorCz : currCz);

        uint32_t leader = (dSrc->leader_w == DEV_NO_LEADER_W ? dSrc->parent : dSrc->leader_w);
        if (leader == DEV_NO_LEADER_W) leader = dSrc->x[3];
        sDraft->kind = SRC_D;
        sDraft->parent = (dSrc->parent == DEV_NO_PARENT ? leader : dSrc->parent);
        dev_adoptLeader(*sDraft, leader);

        dev_moveOneStep(*sDraft, sCx, sCy, sCz, dCx, dCy, dCz, d_draft);
        dev_moveOneStep(*dDraft, dCx, dCy, dCz, sCx, sCy, sCz, d_draft);
        return;
    }

    // 5. D x D
    if (currSrc.kind == SRC_D && partnerSrc.kind == SRC_D)
    {
        if (currSrc.parent != partnerSrc.parent)
        {
            dev_moveOneStepAway(currDraft, currCx, currCy, currCz,
                                mirrorCx, mirrorCy, mirrorCz, d_draft);
            currDraft.reloc[0] += partnerSrc.m[0];
            currDraft.reloc[1] += partnerSrc.m[1];
            currDraft.reloc[2] += partnerSrc.m[2];
        }
        else
        {
            currDraft.spin_target = partnerSrc.spin_target;
            uint32_t leader = dev_dominantLeader(currSrc, partnerSrc);
            dev_adoptLeader(currDraft, leader);
        }
        return;
    }

    // 6. P x K / P x D / P x S
    if (currSrc.kind == SRC_P &&
        (partnerSrc.kind == SRC_K || partnerSrc.kind == SRC_S || partnerSrc.kind == SRC_D))
    {
        dev_reemitAtContact(currDraft, curr, d_draft);
        partnerDraft.reloc[0] += currSrc.m[0];
        partnerDraft.reloc[1] += currSrc.m[1];
        partnerDraft.reloc[2] += currSrc.m[2];
        return;
    }

    // 7. K/D/S x P
    if ((currSrc.kind == SRC_K || currSrc.kind == SRC_S || currSrc.kind == SRC_D) &&
        partnerSrc.kind == SRC_P)
    {
        dev_reemitAtContact(currDraft, curr, d_draft);
        currDraft.reloc[0] += partnerSrc.m[0];
        currDraft.reloc[1] += partnerSrc.m[1];
        currDraft.reloc[2] += partnerSrc.m[2];
        return;
    }

    // P x P is suppressed.
}

// ===================================================================
// MAIN UPDATE KERNEL - COMPLETE CA LOGIC
// ===================================================================

__global__ void ca_update_kernel(::CellDevice* d_curr, ::CellDevice* d_draft, ::CellDevice* d_mirror,
                                 unsigned ENCOUNTER, unsigned GSLOT_Z,
                                 unsigned SLOT1, unsigned SLOT2, unsigned SLOT3,
                                 unsigned SLOT4, unsigned DIFFUSION, unsigned SLOT5, unsigned SLOT6,
                                 unsigned SLOT7, unsigned SLOT8, unsigned RELOC, unsigned REISSUE,
                                 unsigned FLOOD, unsigned FRAME, int scenario)
{
    unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (dev_EL == 0 || dev_W_USED == 0) return;

    unsigned int total_cells = dev_ELX * dev_ELY * dev_ELZ * dev_W_USED;
    if (idx >= total_cells) return;

    // Map 1D -> 4D (spatial-major, per-axis strides)
    unsigned w = idx % dev_W_USED;
    unsigned idx_3d = idx / dev_W_USED;
    unsigned z = idx_3d % dev_ELZ;
    unsigned y = (idx_3d / dev_ELZ) % dev_ELY;
    unsigned x = idx_3d / (dev_ELY * dev_ELZ);
    
    ::CellDevice curr = d_getCell(d_curr, x, y, z, w);
    ::CellDevice draft = curr;
    ::CellDevice partner = d_getCell(d_mirror, x, y, z, w);

    // Neighbors (spherical antipodal wrap for spatial, periodic for W)
    ::CellDevice forward = d_getNeighbor(d_curr, x, y, z, w, 6); // FORWARD
    ::CellDevice north   = d_getNeighbor(d_curr, x, y, z, w, 0); // NORTH
    ::CellDevice east    = d_getNeighbor(d_curr, x, y, z, w, 1); // EAST
    ::CellDevice south   = d_getNeighbor(d_curr, x, y, z, w, 2); // SOUTH
    ::CellDevice west    = d_getNeighbor(d_curr, x, y, z, w, 3); // WEST
    ::CellDevice up      = d_getNeighbor(d_curr, x, y, z, w, 4); // UP
    ::CellDevice down    = d_getNeighbor(d_curr, x, y, z, w, 5); // DOWN

    // ===================================================================
    // ENCOUNTER PHASE (k < ENCOUNTER)
    // ===================================================================
    if (curr.k < ENCOUNTER) {
        switch (scenario) {
            case 0: dev_encounter0(curr, draft, partner, w, idx); break;
            case 1: dev_encounter1(curr, draft, partner, w, idx); break;
            case 2: dev_encounter2(curr, draft, partner, w, idx); break;
            case 3: dev_encounter3(curr, draft, partner, w, idx); break;
            case 4: dev_encounter4(curr, draft, partner, w, idx); break;
            case 5: dev_encounter5(curr, draft, partner, w, idx); break;
            case 6: dev_encounter6(curr, draft, partner, w, idx); break;
            case 7: dev_encounter7(curr, draft, partner, w, idx, d_curr, d_draft); break;
            default: break;
        }
    }
    // ===================================================================
    // GSLOT PHASES (not yet implemented)
    // ===================================================================
    else if (curr.k < GSLOT_Z) {
        // timing slots – reserved for glider transport
    }
    // ===================================================================
    // DIFFUSION PHASE (k < DIFFUSION)
    // ===================================================================
    else if (curr.k < DIFFUSION) {
        // SLOT I
        if (curr.k < SLOT1) {
            if ((north.a == dev_W_USED && curr.r2 >= north.r2) ||
                (west.a  == dev_W_USED && curr.r2 >= west.r2)  ||
                (down.a  == dev_W_USED && curr.r2 >= down.r2)  ||
                (south.a == dev_W_USED && curr.r2 >= south.r2) ||
                (east.a  == dev_W_USED && curr.r2 >= east.r2)  ||
                (up.a    == dev_W_USED && curr.r2 >= up.r2)) {
                draft.a = dev_W_USED;
                draft.leader_w = DEV_NO_LEADER_W;
            }
        }
        // SLOT II
        if (curr.k < SLOT2) {
            if ((north.a == dev_W_USED && curr.r2 >= north.r2) ||
                (west.a  == dev_W_USED && curr.r2 >= west.r2)  ||
                (down.a  == dev_W_USED && curr.r2 >= down.r2)  ||
                (south.a == dev_W_USED && curr.r2 >= south.r2) ||
                (east.a  == dev_W_USED && curr.r2 >= east.r2)  ||
                (up.a    == dev_W_USED && curr.r2 >= up.r2)) {
                draft.a = dev_W_USED;
                draft.leader_w = DEV_NO_LEADER_W;
            }
            // Homing using homB (matches CPU: active wavefront condition)
            if (curr.active) {
                if (north.homB) { draft.c[0] = north.c[0] + 1; curr.sB = !draft.homB; }
                else if (west.homB)  { draft.c[1] = west.c[1] + 1; curr.sB = !draft.homB; }
                else if (down.homB)  { draft.c[2] = down.c[2] + 1; curr.sB = !draft.homB; }
                else if (south.homB) { draft.c[1] = south.c[1] + 1; curr.sB = !draft.homB; }
                else if (east.homB)  { draft.c[0] = east.c[0] + 1; curr.sB = !draft.homB; }
                else if (up.homB)    { draft.c[2] = up.c[2] + 1; curr.sB = !draft.homB; }
            }
        }
        // SLOT III
        else if (curr.k < SLOT3) {
            // Propagate c[] to all cells in layer 0 (matches CPU)
            if (curr.x[3] == 0) {
                if (!ZERO_C(north.c)) {
                    draft.c[0] = north.c[0]; draft.c[1] = north.c[1]; draft.c[2] = north.c[2];
                    if (north.kB) draft.kB = north.kB;
                } else if (!ZERO_C(south.c)) {
                    draft.c[0] = south.c[0]; draft.c[1] = south.c[1]; draft.c[2] = south.c[2];
                    if (south.kB) draft.kB = south.kB;
                } else if (!ZERO_C(east.c)) {
                    draft.c[0] = east.c[0]; draft.c[1] = east.c[1]; draft.c[2] = east.c[2];
                    if (east.kB) draft.kB = east.kB;
                } else if (!ZERO_C(west.c)) {
                    draft.c[0] = west.c[0]; draft.c[1] = west.c[1]; draft.c[2] = west.c[2];
                    if (west.kB) draft.kB = west.kB;
                } else if (!ZERO_C(up.c)) {
                    draft.c[0] = up.c[0]; draft.c[1] = up.c[1]; draft.c[2] = up.c[2];
                    if (up.kB) draft.kB = up.kB;
                } else if (!ZERO_C(down.c)) {
                    draft.c[0] = down.c[0]; draft.c[1] = down.c[1]; draft.c[2] = down.c[2];
                    if (down.kB) draft.kB = down.kB;
                }
            }

            draft.f = max(down.f, max(west.f, max(north.f,
                        max(south.f, max(east.f, up.f)))));

            if (!curr.cB) {
                if (north.cB && north.r2 > curr.r2) {
                    draft.cB = 1;
                    if (north.a != dev_W_USED) { draft.a = north.a; draft.leader_w = north.a; }
                } else if (south.cB && south.r2 > curr.r2) {
                    draft.cB = 1;
                    if (south.a != dev_W_USED) { draft.a = south.a; draft.leader_w = south.a; }
                } else if (east.cB && east.r2 > curr.r2) {
                    draft.cB = 1;
                    if (east.a != dev_W_USED) { draft.a = east.a; draft.leader_w = east.a; }
                } else if (west.cB && west.r2 > curr.r2) {
                    draft.cB = 1;
                    if (west.a != dev_W_USED) { draft.a = west.a; draft.leader_w = west.a; }
                } else if (down.cB && down.r2 > curr.r2) {
                    draft.cB = 1;
                    if (down.a != dev_W_USED) { draft.a = down.a; draft.leader_w = down.a; }
                } else if (up.cB && up.r2 > curr.r2) {
                    draft.cB = 1;
                    if (up.a != dev_W_USED) { draft.a = up.a; draft.leader_w = up.a; }
                }
            }
        }
        // SLOT IV (matches CPU: centered coordinates + clamp)
        else if (curr.k < SLOT4) {
            if (forward.kB && forward.a == curr.a) {
                int hx = (int)dev_ELX / 2;
                int hy = (int)dev_ELY / 2;
                int hz = (int)dev_ELZ / 2;
                int cx = (int)curr.x[0] - hx;
                int cy = (int)curr.x[1] - hy;
                int cz = (int)curr.x[2] - hz;
                int fx = (int)forward.x[0] - hx;
                int fy = (int)forward.x[1] - hy;
                int fz = (int)forward.x[2] - hz;
                int delta_x = cx - fx;
                int delta_y = cy - fy;
                int delta_z = cz - fz;
                int ncx = (int)forward.c[0] + delta_x;
                int ncy = (int)forward.c[1] + delta_y;
                int ncz = (int)forward.c[2] + delta_z;
                ncx = max(0, min((int)dev_ELX - 1, ncx));
                ncy = max(0, min((int)dev_ELY - 1, ncy));
                ncz = max(0, min((int)dev_ELZ - 1, ncz));
                draft.c[0] = ncx;
                draft.c[1] = ncy;
                draft.c[2] = ncz;
                draft.kB = forward.kB;
                draft.cB = forward.cB;
            }
            draft.f = max(forward.f, curr.f);
        }
        // SLOT V
        else if (curr.k < SLOT5) {
            if (curr.a == dev_W_USED && curr.r2 < curr.t * curr.t) {
                draft.a = curr.x[3];
                draft.leader_w = curr.x[3];
            }
        }
    }
    // ===================================================================
    // RELOCATION PHASE (k < RELOC) — restore original coordinates (matches CPU)
    // ===================================================================
    else if (curr.k < RELOC) {
        // Save 3D address (CPU restores after relocation)
        unsigned save_x = curr.x[0];
        unsigned save_y = curr.x[1];
        unsigned save_z = curr.x[2];
        // SLOT VI (x direction)
        if (curr.k < SLOT6) {
            if (north.c[0] > 0) {
                draft = north;
                draft.c[0]--;
            }
        }
        // SLOT VII (y direction)
        else if (curr.k < SLOT7) {
            if (west.c[1] > 0) {
                draft = west;
                draft.c[1]--;
            }
        }
        // SLOT VIII (z direction)
        else if (curr.k < SLOT8) {
            if (down.c[2] > 0) {
                draft = down;
                draft.c[2]--;
            }
        }
        // Restore original 3D address (matches CPU relocate)
        draft.x[0] = save_x;
        draft.x[1] = save_y;
        draft.x[2] = save_z;
    }
    // ===================================================================
    // REISSUE PHASE (k < REISSUE)
    // ===================================================================
    else if (curr.k < REISSUE) {
        draft.kB = 0;
        draft.homB = 0;
        draft.bB = 0;
        if (curr.active) {
            if (north.r2 > curr.r2) { draft.a = north.a; draft.leader_w = (north.a == dev_W_USED ? DEV_NO_LEADER_W : north.a); }
            if (south.r2 > curr.r2) { draft.a = south.a; draft.leader_w = (south.a == dev_W_USED ? DEV_NO_LEADER_W : south.a); }
            if (east.r2 > curr.r2) { draft.a = east.a; draft.leader_w = (east.a == dev_W_USED ? DEV_NO_LEADER_W : east.a); }
            if (west.r2 > curr.r2) { draft.a = west.a; draft.leader_w = (west.a == dev_W_USED ? DEV_NO_LEADER_W : west.a); }
            if (up.r2 > curr.r2) { draft.a = up.a; draft.leader_w = (up.a == dev_W_USED ? DEV_NO_LEADER_W : up.a); }
            if (down.r2 > curr.r2) { draft.a = down.a; draft.leader_w = (down.a == dev_W_USED ? DEV_NO_LEADER_W : down.a); }
        }
        if (curr.cB) {
            draft.cB = 0;
            if (curr.a != dev_W_USED && curr.r2 < 4) {
                draft.t = 0;
            }
        }
    }
    // ===================================================================
    // FLOOD PHASE (k < FLOOD)
    // ===================================================================
    else if (curr.k < FLOOD) {
        if (curr.a != dev_W_USED) {
            draft.t = min(north.t, min(south.t, min(east.t,
                      min(west.t, min(down.t, up.t)))));
        }
    }

    // Update frame counters
    draft.k = (curr.k + 1) % FRAME;
    if (draft.k == 0) {
        if (curr.a == dev_W_USED && curr.t <= dev_RMAX) {
            draft.t++;
        } else {
            draft.t = (curr.t + 1) % (2 * dev_RMAX);
        }
    }

    d_getCell(d_draft, x, y, z, w) = draft;
}

// ===================================================================
// MIRROR UPDATE KERNEL (only when k == 0)
// ===================================================================
__global__ void updatePartnerKernel(CellDevice* lattice_curr,
                                   CellDevice* lattice_partner,
                                   unsigned totalCells)
{
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= totalCells) return;
    lattice_partner[tid] = lattice_curr[tid];
    lattice_partner[tid].f = lattice_partner[tid].t;
}

// ===================================================================
// SHIFT-MIRROR KERNEL (cyclic shift along w dimension)
// ===================================================================
__global__ void rotatePartnersKernel(const CellDevice* src,
                                  CellDevice* dst,
                                  unsigned totalCells)
{
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= totalCells) return;
    unsigned w = tid % dev_W_USED;
    unsigned idx3d = tid / dev_W_USED;
    unsigned src_w = (w + dev_W_USED - 1) % dev_W_USED;
    unsigned src_tid = idx3d * dev_W_USED + src_w;
    dst[tid] = src[src_tid];
}

// ===================================================================
// CONSTANT MEMORY SETUP (must be in same .cu as kernels)
// ===================================================================

extern "C" void setCudaConstants(unsigned EL, unsigned W_USED, unsigned RMAX)
{
    cudaError_t err;
    unsigned CENTER = (EL - 1) / 2;

    printf("Setting dev_EL = %u\n", EL);
    unsigned sieveDefault = 16384u;
    err = cudaMemcpyToSymbol(dev_S2B, &sieveDefault, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_S2B: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }
    err = cudaMemcpyToSymbol(dev_EL, &EL, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_EL: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    err = cudaMemcpyToSymbol(dev_ELX, &EL, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_ELX: %s (code %d)\n", cudaGetErrorString(err), err);
        return;
    }
    err = cudaMemcpyToSymbol(dev_ELY, &EL, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_ELY: %s (code %d)\n", cudaGetErrorString(err), err);
        return;
    }
    err = cudaMemcpyToSymbol(dev_ELZ, &EL, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_ELZ: %s (code %d)\n", cudaGetErrorString(err), err);
        return;
    }
    printf("Setting dev_W_USED = %u\n", W_USED);
    err = cudaMemcpyToSymbol(dev_W_USED, &W_USED, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_W_USED: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("Setting dev_RMAX = %u\n", RMAX);
    err = cudaMemcpyToSymbol(dev_RMAX, &RMAX, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_RMAX: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("Setting dev_CENTER = %u\n", CENTER);
    err = cudaMemcpyToSymbol(dev_CENTER, &CENTER, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_CENTER: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    int initCtrl = 1;
    err = cudaMemcpyToSymbol(dev_ctrl, &initCtrl, sizeof(int));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_ctrl: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("All constants set successfully\n");
}

extern "C" void setCudaTubeDimensions(unsigned LX, unsigned LY, unsigned LZ)
{
    cudaError_t err;
    err = cudaMemcpyToSymbol(dev_ELX, &LX, sizeof(unsigned));
    if (err != cudaSuccess) fprintf(stderr, "Error setting dev_ELX: %s (code %d)\n", cudaGetErrorString(err), err);
    err = cudaMemcpyToSymbol(dev_ELY, &LY, sizeof(unsigned));
    if (err != cudaSuccess) fprintf(stderr, "Error setting dev_ELY: %s (code %d)\n", cudaGetErrorString(err), err);
    err = cudaMemcpyToSymbol(dev_ELZ, &LZ, sizeof(unsigned));
    if (err != cudaSuccess) fprintf(stderr, "Error setting dev_ELZ: %s (code %d)\n", cudaGetErrorString(err), err);
    unsigned shortside = LX < LY ? (LX < LZ ? LX : LZ) : (LY < LZ ? LY : LZ);
    unsigned center = (shortside - 1u) / 2u;
    err = cudaMemcpyToSymbol(dev_CENTER, &center, sizeof(unsigned));
    if (err != cudaSuccess) fprintf(stderr, "Error setting dev_CENTER: %s (code %d)\n", cudaGetErrorString(err), err);
    err = cudaMemcpyToSymbol(dev_EL, &LX, sizeof(unsigned));
    if (err != cudaSuccess) fprintf(stderr, "Error setting dev_EL: %s (code %d)\n", cudaGetErrorString(err), err);
    printf("Set CUDA tube dimensions: ELX=%u ELY=%u ELZ=%u CENTER=%u\n", LX, LY, LZ, center);
}

extern "C" void setCudaSieve(unsigned S)
{
    cudaError_t err = cudaMemcpyToSymbol(dev_S2B, &S, sizeof(unsigned));
    if (err != cudaSuccess)
        fprintf(stderr, "Error setting dev_S2B (sieve): %s\n", cudaGetErrorString(err));
}

extern "C" void setCudaSourceCenters(const unsigned* centers, unsigned W)
{
    if (W > 32) W = 32;
    if (W == 0) return;
    cudaError_t err = cudaMemcpyToSymbol(dev_lcenters, centers, W * 3 * sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_lcenters: %s (code %d)\n",
                cudaGetErrorString(err), err);
    }
}

extern "C" void resetCudaCtrl()
{
    int val = 1;
    cudaError_t err = cudaMemcpyToSymbol(dev_ctrl, &val, sizeof(int));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error resetting dev_ctrl: %s (code %d)\n",
                cudaGetErrorString(err), err);
    } else {
        printf("dev_ctrl reset to 1\n");
    }
}

// ===================================================================
// HOST API FUNCTIONS
// ===================================================================

bool isCudaAvailable() 
{
    int devCount = 0;
    cudaError_t err = cudaGetDeviceCount(&devCount); 
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA not available: %s\n", cudaGetErrorString(err));
        return false;
    }
    if (devCount == 0) {
        fprintf(stderr, "No CUDA devices found\n");
        return false;
    }
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("Found CUDA device: %s (Compute %d.%d)\n", 
           prop.name, prop.major, prop.minor);
    return true;
}

bool init_cuda_memory(unsigned EL, unsigned W_USED)
{
    if (d_lattice_curr != nullptr) {
        fprintf(stderr, "CUDA memory already allocated\n");
        return true;
    }
    unsigned LX = automaton::ELX ? automaton::ELX : EL;
    unsigned LY = automaton::ELY ? automaton::ELY : EL;
    unsigned LZ = automaton::ELZ ? automaton::ELZ : EL;
    size_t total_cells = (size_t)LX * LY * LZ * W_USED;
    size_t size = total_cells * sizeof(::CellDevice);
    printf("Allocating CUDA memory: %zu cells, %zu MB per lattice\n", 
           total_cells, size / (1024 * 1024));
    if (size > 2ULL * 1024 * 1024 * 1024) {
        fprintf(stderr, "ERROR: Requested allocation too large: %zu MB\n", size / (1024 * 1024));
        return false;
    }
    CUDA_CHECK(cudaMalloc((void**)&d_lattice_curr, size));
    CUDA_CHECK(cudaMalloc((void**)&d_lattice_draft, size));
    CUDA_CHECK(cudaMalloc((void**)&d_lattice_partner, size));
    CUDA_CHECK(cudaMemset(d_lattice_curr, 0, size));
    CUDA_CHECK(cudaMemset(d_lattice_draft, 0, size));
    CUDA_CHECK(cudaMemset(d_lattice_partner, 0, size));
    printf("✓ CUDA Memory Allocated successfully\n");
    return true;
}

bool initCudaSimulation(unsigned EL, unsigned W_USED)
{
    if (g_cuda_initialized) {
        printf("CUDA already initialized\n");
        return true;
    }
    printf("Initializing CUDA simulation: EL=%u, W_USED=%u\n", EL, W_USED);
    CUDA_CHECK_VOID(cudaDeviceReset());
    CUDA_CHECK(cudaSetDevice(0));
    // Constants are set by setCudaConstants (called from bridge_cuda.cu)
    if (!init_cuda_memory(EL, W_USED)) return false;
    g_cuda_initialized = true;
    printf("✓ CUDA simulation initialized successfully\n");
    return true;
}

void free_cuda_memory()
{
    if (d_lattice_curr) cudaFree(d_lattice_curr);
    if (d_lattice_draft) cudaFree(d_lattice_draft);
    if (d_lattice_partner) cudaFree(d_lattice_partner);
    d_lattice_curr = d_lattice_draft = d_lattice_partner = nullptr;
    g_cuda_initialized = false;
}

void cudaCleanup()
{
    printf("Cleaning up CUDA resources...\n");
    free_cuda_memory();
    cudaDeviceReset();
    printf("✓ CUDA cleanup complete\n");
}

bool uploadLatticeToCuda(::CellDevice* hostCells, size_t totalCells)
{
    if (!g_cuda_initialized) {
        fprintf(stderr, "ERROR: CUDA not initialized\n");
        return false;
    }
    size_t size = totalCells * sizeof(::CellDevice);
    CUDA_CHECK(cudaMemcpy(d_lattice_curr, hostCells, size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_lattice_draft, d_lattice_curr, size, cudaMemcpyDeviceToDevice));
    CUDA_CHECK(cudaMemcpy(d_lattice_partner, d_lattice_curr, size, cudaMemcpyDeviceToDevice));
    return true;
}

bool downloadLatticeFromCuda(::CellDevice* hostCells, size_t totalCells)
{
    if (!g_cuda_initialized) return false;
    size_t size = totalCells * sizeof(::CellDevice);
    CUDA_CHECK(cudaMemcpy(hostCells, d_lattice_curr, size, cudaMemcpyDeviceToHost));
    return true;
}

void cudaSimulationStep(
    unsigned ENCOUNTER, unsigned GSLOT_Z,
    unsigned SLOT1, unsigned SLOT2, unsigned SLOT3,
    unsigned SLOT4, unsigned DIFFUSION, unsigned SLOT5, unsigned SLOT6,
    unsigned SLOT7, unsigned SLOT8, unsigned RELOC, unsigned REISSUE,
    unsigned FLOOD, unsigned FRAME, unsigned RMAX, int scenario,
    unsigned pulse_tick)
{
    if (!g_cuda_initialized) {
        fprintf(stderr, "ERROR: CUDA not initialized\n");
        return;
    }

    // Get dimensions from automaton namespace (they are host variables)
    unsigned L = automaton::EL;
    unsigned W = automaton::W_USED;
    unsigned LX = automaton::ELX ? automaton::ELX : L;
    unsigned LY = automaton::ELY ? automaton::ELY : L;
    unsigned LZ = automaton::ELZ ? automaton::ELZ : L;
    if (L == 0 || W == 0 || LX == 0 || LY == 0 || LZ == 0) {
        fprintf(stderr, "ERROR: EL or W_USED is zero\n");
        return;
    }

    size_t total_cells = (size_t)LX * LY * LZ * W;
    if (total_cells == 0) {
        fprintf(stderr, "ERROR: total_cells = 0\n");
        return;
    }

    const int BLOCK_SIZE = 256;
    int GRID = (int)((total_cells + BLOCK_SIZE - 1) / BLOCK_SIZE);
    // printf("Launching kernel: total_cells=%zu, GRID=%d, BLOCK=%d, EL=%u, W_USED=%u, RMAX=%u\n",
    //        total_cells, GRID, BLOCK_SIZE, L, W, RMAX);

    // Upload current source centers before the phase step uses them.
    if (W > 0 && !automaton::lcenters.empty())
        setCudaSourceCenters(automaton::lcenters[0].data(), W);

    // Phase step: update r2/r, (u,v), active and emergent pB/sB/phiB into d_lattice_draft,
    // then swap so the main CA kernel reads the updated phase.
    phase_step_kernel<<<GRID, BLOCK_SIZE>>>(d_lattice_curr, d_lattice_draft, pulse_tick);

    cudaError_t phaseErr = cudaGetLastError();
    if (phaseErr != cudaSuccess) {
        fprintf(stderr, "phase_step_kernel launch failed: %s\n", cudaGetErrorString(phaseErr));
        return;
    }
    phaseErr = cudaDeviceSynchronize();
    if (phaseErr != cudaSuccess) {
        fprintf(stderr, "phase_step_kernel execution failed: %s\n", cudaGetErrorString(phaseErr));
        return;
    }

    ::CellDevice* phaseTmp = d_lattice_curr;
    d_lattice_curr = d_lattice_draft;
    d_lattice_draft = phaseTmp;

    // Launch main CA kernel
    ca_update_kernel<<<GRID, BLOCK_SIZE>>>(
        d_lattice_curr, d_lattice_draft, d_lattice_partner,
        ENCOUNTER, GSLOT_Z, SLOT1, SLOT2, SLOT3, SLOT4, DIFFUSION,
        SLOT5, SLOT6, SLOT7, SLOT8, RELOC, REISSUE,
        FLOOD, FRAME, scenario
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "Kernel launch failed: %s\n", cudaGetErrorString(err));
        return;
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "Kernel execution failed: %s\n", cudaGetErrorString(err));
        return;
    }

    // Swap curr and draft
    ::CellDevice* temp = d_lattice_curr;
    d_lattice_curr = d_lattice_draft;
    d_lattice_draft = temp;

    // Apply per-source relocation impulse: move the source center, preserve the
    // long-term momentum direction m, and clear the old center.  This mirrors
    // CPU applyMomentum().
    for (unsigned iw = 0; iw < W; ++iw)
    {
        int cx = (int)automaton::lcenters[iw][0];
        int cy = (int)automaton::lcenters[iw][1];
        int cz = (int)automaton::lcenters[iw][2];
        size_t idx = (size_t)((((cx * (int)LY) + cy) * (int)LZ) + cz) * (int)W + iw;
        ::CellDevice centerCell;
        err = cudaMemcpy(&centerCell, d_lattice_curr + idx, sizeof(::CellDevice), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess)
            continue;

        // Free photon pairs expand and are gradually consumed. At maximum
        // radius (t == RMAX) one pair is consumed; when the stack empties the
        // two partner source centers are released as singletons moving apart.
        if (centerCell.kind == SRC_P &&
            centerCell.a == (uint32_t)W &&
            centerCell.pair_idx != DEV_NO_PAIR &&
            centerCell.pair_idx < (uint32_t)W &&
            centerCell.t == (uint32_t)RMAX &&
            centerCell.x[3] < centerCell.pair_idx)
        {
            if (centerCell.pair_count > 0)
                centerCell.pair_count--;

            uint32_t pw = centerCell.pair_idx;
            int pcx = (int)automaton::lcenters[pw][0];
            int pcy = (int)automaton::lcenters[pw][1];
            int pcz = (int)automaton::lcenters[pw][2];
            size_t idxPartner = (size_t)((((pcx * (int)LY) + pcy) * (int)LZ) + pcz) * (int)W + (size_t)pw;
            ::CellDevice partner;
            err = cudaMemcpy(&partner, d_lattice_curr + idxPartner, sizeof(::CellDevice), cudaMemcpyDeviceToHost);
            if (err == cudaSuccess)
            {
                if (centerCell.pair_count == 0)
                {
                    centerCell.kind       = SRC_S;
                    centerCell.pair_idx   = DEV_NO_PAIR;
                    centerCell.pair_count = 0;
                    centerCell.leader_w   = DEV_NO_LEADER_W;
                    centerCell.a          = (uint32_t)W;

                    partner.kind       = SRC_S;
                    partner.pair_idx   = DEV_NO_PAIR;
                    partner.pair_count = 0;
                    partner.leader_w   = DEV_NO_LEADER_W;
                    partner.a          = (uint32_t)W;

                    int axis = (int)(centerCell.x[3] % 3u);
                    int sign = ((centerCell.x[3] & 1u) ? +1 : -1);
                    centerCell.reloc[axis] += sign;
                    partner.reloc[axis]    -= sign;
                }
                else
                {
                    partner.pair_count = centerCell.pair_count;
                }
                cudaMemcpy(d_lattice_curr + idxPartner, &partner, sizeof(::CellDevice), cudaMemcpyHostToDevice);
            }
        }

        int dx = centerCell.reloc[0];
        int dy = centerCell.reloc[1];
        int dz = centerCell.reloc[2];
        if (dx == 0 && dy == 0 && dz == 0)
            continue;

        // Update the long-term momentum direction from the consumed impulse.
        int new_m[3] = { centerCell.m[0], centerCell.m[1], centerCell.m[2] };
        {
            int abs_dx = (dx < 0) ? -dx : dx;
            int abs_dy = (dy < 0) ? -dy : dy;
            int abs_dz = (dz < 0) ? -dz : dz;
            int axis = 0, best = abs_dx;
            if (abs_dy > best) { axis = 1; best = abs_dy; }
            if (abs_dz > best) { axis = 2; }
            int val = (axis == 0 ? dx : (axis == 1 ? dy : dz));
            new_m[0] = new_m[1] = new_m[2] = 0;
            new_m[axis] = (val < 0) ? -1 : +1;
        }

        int nx = (cx + dx) % (int)LX;
        int ny = (cy + dy) % (int)LY;
        int nz = (cz + dz) % (int)LZ;
        if (nx < 0) nx += (int)LX;
        if (ny < 0) ny += (int)LY;
        if (nz < 0) nz += (int)LZ;

        size_t idxNew = (size_t)((((nx * (int)LY) + ny) * (int)LZ) + nz) * (int)W + iw;
        ::CellDevice newCell;
        err = cudaMemcpy(&newCell, d_lattice_curr + idxNew, sizeof(::CellDevice), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess)
            continue;

        // Carry source identity, momentum direction, and re-seed the wave.
        newCell.kind        = centerCell.kind;
        newCell.parent      = centerCell.parent;
        newCell.spin_target = centerCell.spin_target;
        newCell.pair_idx    = centerCell.pair_idx;
        newCell.pair_count  = centerCell.pair_count;
        newCell.leader_w    = centerCell.leader_w;
        newCell.a           = centerCell.a;
        newCell.t           = 0;
        newCell.f           = 0;
        newCell.u           = 2048;
        newCell.v           = 0;
        newCell.m[0]        = new_m[0];
        newCell.m[1]        = new_m[1];
        newCell.m[2]        = new_m[2];
        newCell.reloc[0]    = newCell.reloc[1] = newCell.reloc[2] = 0;

        // Old cell is no longer a source center.
        centerCell.kind        = SRC_S;
        centerCell.parent      = DEV_NO_PARENT;
        centerCell.spin_target = 0;
        centerCell.pair_idx    = DEV_NO_PAIR;
        centerCell.pair_count  = 0;
        centerCell.leader_w    = DEV_NO_LEADER_W;
        centerCell.a           = (uint32_t)W;
        centerCell.t           = 0;
        centerCell.f           = 0;
        centerCell.u           = 0;
        centerCell.v           = 0;
        centerCell.m[0]        = centerCell.m[1] = centerCell.m[2] = 0;
        centerCell.reloc[0]    = centerCell.reloc[1] = centerCell.reloc[2] = 0;

        cudaMemcpy(d_lattice_curr + idxNew, &newCell, sizeof(::CellDevice), cudaMemcpyHostToDevice);
        cudaMemcpy(d_lattice_curr + idx, &centerCell, sizeof(::CellDevice), cudaMemcpyHostToDevice);

        automaton::lcenters[iw][0] = (unsigned)nx;
        automaton::lcenters[iw][1] = (unsigned)ny;
        automaton::lcenters[iw][2] = (unsigned)nz;
    }

    // Read new k from first cell (only after successful kernel execution)
    unsigned new_k = 0;
    err = cudaMemcpy(&new_k, &d_lattice_curr[0].k, sizeof(unsigned), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to read k from device: %s\n", cudaGetErrorString(err));
        return;
    }

    if (new_k == 0) {
        updatePartnerKernel<<<GRID, BLOCK_SIZE>>>(d_lattice_curr, d_lattice_partner, (unsigned)total_cells);
        cudaDeviceSynchronize();
    }

    if (new_k < ENCOUNTER) {
        size_t size = (size_t)total_cells * sizeof(::CellDevice);
        err = cudaMemcpy(d_lattice_draft, d_lattice_partner, size, cudaMemcpyDeviceToDevice);
        if (err != cudaSuccess) {
            fprintf(stderr, "Partner copy for rotation failed: %s\n", cudaGetErrorString(err));
            return;
        }
        rotatePartnersKernel<<<GRID, BLOCK_SIZE>>>(d_lattice_draft, d_lattice_partner, (unsigned)total_cells);
        cudaDeviceSynchronize();
    }
}

void cudaUpdateVoxelsLayer(unsigned selectedW)
{
    // TODO: implement GPU-accelerated voxel coloring (optional)
}

uint32_t* getMappedVoxels()
{
    return nullptr;
}

// ===================================================================
// NAMESPACE WRAPPER FUNCTIONS
// ===================================================================
namespace automaton 
{
    ::CellDevice convertToDevice(const Cell& src) {
        ::CellDevice dst;
        dst.ch = src.ch;
        dst.pB = src.pB ? 1 : 0;
        dst.sB = src.sB ? 1 : 0;
        dst.a = src.a;
        for (int i = 0; i < 4; ++i) dst.x[i] = src.x[i];
        dst.r2 = src.r2;
        dst.r = src.r;
        dst.u = src.u;
        dst.v = src.v;
        dst.active = src.active ? 1u : 0u;
        dst.phiB = src.phiB ? 1 : 0;
        dst.t = src.t;
        dst.f = src.f;
        for (int i = 0; i < 3; ++i) dst.c[i] = src.c[i];
        dst.k = src.k;
        dst.s2B = src.s2B ? 1 : 0;
        dst.kB = src.kB ? 1 : 0;
        dst.bB = src.bB ? 1 : 0;
        dst.homB = src.homB ? 1 : 0;
        dst.cB = src.cB ? 1 : 0;
        dst.gB = src.gB ? 1 : 0;
        for (int i = 0; i < 3; ++i) dst.g[i] = static_cast<int32_t>(src.g[i]);

        dst.kind = static_cast<uint8_t>(src.kind);
        dst.parent = src.parent;
        dst.spin_target = static_cast<int32_t>(src.spin_target);
        dst.pair_idx = src.pair_idx;
        dst.leader_w = src.leader_w;
        dst.pair_count = static_cast<uint32_t>(src.pair_count);
        for (int i = 0; i < 3; ++i) dst.m[i] = static_cast<int32_t>(src.m[i]);
        for (int i = 0; i < 3; ++i) dst.reloc[i] = static_cast<int32_t>(src.reloc[i]);
        return dst;
    }

    void convertToHost(const ::CellDevice& src, Cell& dst) {
        dst.ch = src.ch;
        dst.pB = src.pB != 0;
        dst.sB = src.sB != 0;
        dst.a = src.a;
        for (int i = 0; i < 4; ++i) dst.x[i] = src.x[i];
        dst.r2 = src.r2;
        dst.r = src.r;
        dst.u = src.u;
        dst.v = src.v;
        dst.active = src.active != 0;
        dst.phiB = src.phiB != 0;
        dst.t = src.t;
        dst.f = src.f;
        for (int i = 0; i < 3; ++i) dst.c[i] = src.c[i];
        dst.k = src.k;
        dst.s2B = src.s2B != 0;
        dst.kB = src.kB != 0;
        dst.bB = src.bB != 0;
        dst.homB = src.homB != 0;
        dst.cB = src.cB != 0;
        dst.gB = src.gB != 0;
        for (int i = 0; i < 3; ++i) dst.g[i] = static_cast<int>(src.g[i]);

        dst.kind = static_cast<SourceKind>(src.kind);
        dst.parent = src.parent;
        dst.spin_target = static_cast<int8_t>(src.spin_target);
        dst.pair_idx = src.pair_idx;
        dst.leader_w = src.leader_w;
        dst.pair_count = static_cast<uint8_t>(src.pair_count);
        for (int i = 0; i < 3; ++i) dst.m[i] = static_cast<int>(src.m[i]);
        for (int i = 0; i < 3; ++i) dst.reloc[i] = static_cast<int>(src.reloc[i]);
    }

    bool swap_lattices_gpu() { return true; }

    void free_cuda_memory() { ::free_cuda_memory(); }
}