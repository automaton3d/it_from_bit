#pragma once
#include <cstdint>

// Source-kind constants matching automaton::SourceKind (K/S/D/P)
constexpr uint8_t SRC_K = 0;
constexpr uint8_t SRC_S = 1;
constexpr uint8_t SRC_D = 2;
constexpr uint8_t SRC_P = 3;
constexpr uint32_t DEV_NO_PARENT = 0xFFFFFFFFu;
constexpr uint32_t DEV_NO_PAIR    = 0xFFFFFFFFu;
constexpr uint32_t DEV_NO_LEADER_W = 0xFFFFFFFFu;

struct CellDevice
{
    uint8_t ch;
    uint8_t pB, sB;       // pB=(u>0) electric channel; sB=(v>0) magnetic channel
    uint32_t a;

    uint32_t x[4];
    uint32_t r2;
    int32_t  r;
    int32_t  u, v;        // radial polarisation pair (u in-phase, v quadrature)
    uint32_t bstamp;      // broadcast arrival tick of the elected momentum (0 = never reached)
    int32_t  pol_u, pol_v;// reconstructed transverse polarisation (approximates u^2+v^2=R^4 via isqrt)
    uint32_t active;      // 1 when cell is on the pulsating wavefront
    uint32_t phiB;        // active wavefront marker (phiB == active)
    uint32_t t;
    uint32_t f;

    uint32_t c[4];
    uint32_t k;
    uint32_t s2B;

    uint32_t kB, bB, homB, cB;

    uint32_t gB;
    int32_t  g[3];

    // Spin-rev source model
    uint8_t  kind;         // SRC_K, SRC_S, SRC_D, SRC_P
    uint32_t parent;       // Parent source index
    int32_t  spin_target;  // +1 outward, -1 inward, 0 neutral
    uint32_t pair_idx;     // Pair partner index
    uint32_t leader_w;     // Auxiliary W identity copied from the core
    uint32_t pair_count;   // Number of overlapping pairs (frequency = 2 * pair_count)
    int32_t  m[3];         // Momentum direction vector
    int32_t  reloc[3];     // Consumable relocation offset / impulse
};