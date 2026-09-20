#pragma once
namespace automaton::polarization {
// Experimental lexicographic ranking of signed polarization components.
// Zero carries no orientation; equal maxima do not elect a winner.
struct PolarizationCandidate {
  int u=0,v=0;
  bool present=false,tied=false;
  bool consider(int pu,int pv) {
    if(pu==0 && pv==0) return false;
    if(!present || pu>u || (pu==u && pv>v)) {
      u=pu;v=pv;present=true;tied=false;return true;
    }
    if(pu==u && pv==v) tied=true;
    return false;
  }
  bool unique() const { return present && !tied; }
};
}
