#pragma once
#include "model/simulation.h"
namespace automaton {
// Eligibility does not depend on affinity or the seed's charge-family index.
inline bool canElectChief(const Cell& a,const Cell& b) {
  return a.w!=b.w && a.kind!=SourceKind::P && b.kind!=SourceKind::P && a.ch==b.ch;
}
// Island identity is its chief. A singleton or pair is not assigned to
// an island merely because it shares affinity or a seeded leader field.
inline WIndex islandChief(const Cell& s) {
  if(s.kind==SourceKind::K && s.w<W_USED) return s.w;
  if(s.kind==SourceKind::D && s.parent<W_USED) return s.parent;
  return NO_PARENT;
}
inline bool shareChief(const Cell& a,const Cell& b) {
  const WIndex chief=islandChief(a);
  return chief!=NO_PARENT && chief==islandChief(b);
}
}
