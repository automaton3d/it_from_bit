#pragma once
#include <cstdint>

namespace automaton::polarization {
// Shared by the reference election and its isolated first-election probe.
inline uint32_t electionPayload(uint32_t key, uint32_t code) {
  uint32_t x = key * 0x9E3779B9u ^ (code * 0x85EBCA6Bu);
  x ^= x >> 13;
  x *= 0xC2B2AE35u;
  x ^= x >> 16;
  return ((x & 0xFFu) << 24) | (code & 0x00FFFFFFu);
}
}
