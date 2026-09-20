#pragma once
#include <array>
#include <cstdint>
namespace automaton {
// Proposed rule. Bit 0/1/2 corresponds to x/y/z. Distinct equal-charge
// participants in an odd W ring receive opposite contributions.
inline std::array<int,3> colorContact(unsigned wa,unsigned wb,unsigned W,
                                    unsigned ca,unsigned cb,int ra,int rb) {
  std::array<int,3> d{0,0,0};
  if(W==0 || wa>=W || wb>=W || wa==wb || ca!=cb || ra<=0 || rb<=0) return d;
  const uint64_t gap=(uint64_t(wb)+W-wa)%W;
  if(2*gap==W) return d; // Even-W antipode has no reciprocal orientation.
  int sign=2*gap<W?1:-1;
  for(int axis=0;axis<3;++axis) d[axis]=sign*(1-2*int((ca>>axis)&1));
  return d;
}
}
