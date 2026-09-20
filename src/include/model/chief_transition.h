#pragma once
#include "model/island_identity.h"
namespace automaton {
#ifdef PHASE_DISTINCT_FSM
  // Candidate (see experiments/EMERGENCE_SEARCH.md): identity is the pair
  // (charge word, breathing phase).  The reference already refuses merges
  // between different charge words; this refuses them also between sources that
  // agree in word AND phase, because such bubbles are indistinguishable in the
  // model's own sense.  Reads only ch and t: no ISLAND_SIZE, no L.
  inline bool phaseConflict(const Cell& a, const Cell& b) {
    return a.ch == b.ch && a.t == b.t;
  }
#endif
inline bool promotesDelegate(const Cell& main,const Cell& mirror) {
  // Active/radius contact gates belong to encounter. No affinity, parent,
  // spin, separation, or population condition is imposed here.
#ifdef PARENT_SELECTIVE_FSM
  // Candidate (2026-09-19): D x D no longer promotes.  The population cap of 2 came
  // from this transition ejecting the surplus member of a group; with it disabled, two
  // delegates of the SAME island simply stay delegates: identity preserved, no step and no
  // record -- their contact is a no-op, NOT a cohesion step (the equal-charge D x D early return
  // inside `encounter()` runs before the `internal` recording, so a same-charge D x D pair never
  // reaches the cohesion loop; corrected 19 Sep 2026) -- while two delegates of DIFFERENT
  // islands repel by one step at the frame edge
  // (interaction.cpp: encounter()/resolveParentRepulsion()).  The discriminator is the
  // DYNAMICAL parent identity -- never the seed family, never ISLAND_SIZE, never L.
  (void)main; (void)mirror;
  return false;
#endif
#ifdef DD_INTRA_ISLAND_FIX
  // Candidate (WP3.3): a D x D contact promotes only across DISTINCT islands.
  // Within one island (same chief / parent) the two delegates must stay
  // delegates, so a 3-constituent family keeps 1 K + 2 D instead of cascading
  // to 2 K + 1 D.  Macro-guarded; OFF in the reference build.
  if (main.parent < W_USED && main.parent == mirror.parent) return false;
#endif
  return main.kind==SourceKind::D && mirror.kind==SourceKind::D &&
         main.ch==mirror.ch && main.w<mirror.w;
}
inline void makeChief(Cell& draft) {
  draft.kind=SourceKind::K;draft.parent=NO_PARENT;draft.leader_w=draft.w;
}
inline bool demotesChief(const Cell& main,const Cell& mirror) {
  return main.kind==SourceKind::K && mirror.kind==SourceKind::K &&
         main.ch==mirror.ch && main.w>mirror.w;
}
inline void makeDelegate(Cell& draft,WIndex chief) {
  draft.kind=SourceKind::D;draft.parent=chief;draft.leader_w=chief;
}
inline void chiefContact(const Cell& main,const Cell& mirror,Cell& draft) {
  if(main.w==mirror.w || main.ch!=mirror.ch) return;
#ifdef PHASE_DISTINCT_FSM
  // Candidate: the reference refuses merges between different words; this also
  // refuses them between sources that agree in word AND breathing phase, i.e.
  // between indistinguishable bubbles.  Covers (T1), (T3) and (T4) here and
  // (T2) through promotesDelegate below.
  if(phaseConflict(main,mirror)) return;
#endif
#ifdef DD_INTRA_ISLAND_FIX
  // Candidate (WP3.3): within one seed FAMILY (w / ISLAND_SIZE) the chief is the
  // family-minimum address, so a 3-constituent family elects exactly ONE chief
  // (1 K + 2 D) instead of a pairwise cascade to 2 K + 1 D.  Only same-family
  // S x S election is redirected; other contacts keep the minimum-address rule.
  // Macro-guarded; OFF in the reference build.
  if (main.kind==SourceKind::S && mirror.kind==SourceKind::S &&
      ISLAND_SIZE > 0 && main.w / ISLAND_SIZE == mirror.w / ISLAND_SIZE) {
    const WIndex famMin = (main.w / ISLAND_SIZE) * ISLAND_SIZE;
    if (main.w == famMin) makeChief(draft);
    else { draft.kind = SourceKind::D; draft.parent = famMin; draft.leader_w = famMin; }
    return;
  }
#endif
  if(promotesDelegate(main,mirror)) {makeChief(draft);return;}
  if(demotesChief(main,mirror)) {
    const WIndex chief=draft.kind==SourceKind::D?std::min(draft.parent,mirror.w):mirror.w;
    makeDelegate(draft,chief);return;
  }
  // Existing K/D identities are not overwritten by generic minimum merging.
  if(main.kind!=SourceKind::S || draft.kind!=SourceKind::S) return;
  WIndex chief=NO_PARENT;
  if(mirror.kind==SourceKind::K) chief=mirror.w;
  else if(mirror.kind==SourceKind::D) chief=mirror.parent;
  else if(mirror.kind==SourceKind::S) chief=main.w<mirror.w?main.w:mirror.w;
  if(chief>=W_USED) return;
  if(chief==main.w) makeChief(draft);
  else {draft.kind=SourceKind::D;draft.parent=chief;draft.leader_w=chief;}
}
#ifdef CASCADE_LOG
  // ---------------------------------------------------------------------------
  // A2 probe (read-only; OFF in the reference -- with the macro undefined the
  // preprocessor removes this whole block, so the reference build is unchanged).
  // The question it answers: where in the light frame does the membership
  // cascade happen, and which encounter leaves exactly one delegate per charge
  // word?  Nothing here writes to the lattice.
  //   cascadeRecord()  called by encounter() when chiefContact() changed the
  //                    current source's role: tick, both addresses, both kinds,
  //                    the charge word, and the parent before/after.
  //   cascadeDump(f)   called by the probe after each light frame: the global
  //                    role-change tally, then one line per charge word with the
  //                    tick window of its cascade and its LAST transition -- the
  //                    encounter that ends that word's cascade.
  // ---------------------------------------------------------------------------
  struct CascadeNote {
    unsigned      tick;
    WIndex        w, partner;
    unsigned char fromKind, toKind, ch;
    WIndex        fromParent, toParent;
  };
  inline std::vector<CascadeNote>& cascadeLedger()
  {
    static std::vector<CascadeNote> v;
    return v;
  }
  inline void cascadeRecord(unsigned tick, WIndex w, WIndex partner,
                            unsigned char fromKind, unsigned char toKind,
                            unsigned char ch, WIndex fromParent, WIndex toParent)
  {
    cascadeLedger().push_back({tick, w, partner, fromKind, toKind, ch, fromParent, toParent});
  }
  inline const char* cascadeKind(unsigned char k)
  {
    switch ((SourceKind)k) {
      case SourceKind::S: return "S";
      case SourceKind::K: return "K";
      case SourceKind::D: return "D";
      default:            return "P";
    }
  }
  inline void cascadeDump(unsigned frame, unsigned tick)
  {
    const std::vector<CascadeNote>& L = cascadeLedger();
    unsigned sk = 0, sd = 0, dk = 0, kd = 0, other = 0;
    for (const CascadeNote& n : L) {
      const bool s = n.fromKind == (unsigned char)SourceKind::S;
      const bool d = n.fromKind == (unsigned char)SourceKind::D;
      const bool k = n.fromKind == (unsigned char)SourceKind::K;
      if      (s && n.toKind == (unsigned char)SourceKind::K) ++sk;
      else if (s && n.toKind == (unsigned char)SourceKind::D) ++sd;
      else if (d && n.toKind == (unsigned char)SourceKind::K) ++dk;
      else if (k && n.toKind == (unsigned char)SourceKind::D) ++kd;
      else ++other;
    }
    printf("[cascade] frame=%u tick=%u notes=%u S->K=%u S->D=%u D->K=%u K->D=%u other=%u\n",
           frame, tick, (unsigned)L.size(), sk, sd, dk, kd, other);
    for (unsigned ch = 0; ch < 256u; ++ch) {
      unsigned n = 0, firstT = 0, lastT = 0;
      const CascadeNote* last = nullptr;
      for (const CascadeNote& x : L) {
        if (x.ch != (unsigned char)ch) continue;
        if (n == 0) firstT = x.tick;
        lastT = x.tick; last = &x; ++n;
      }
      if (n == 0) continue;
      printf("[cascade]   ch=0x%02X notes=%u first_tick=%u last_tick=%u"
             " last=%s(w=%u)->%s(partner=%u) parent %u->%u\n",
             ch, n, firstT, lastT,
             cascadeKind(last->fromKind), (unsigned)last->w,
             cascadeKind(last->toKind), (unsigned)last->partner,
             (unsigned)last->fromParent, (unsigned)last->toParent);
    }
    fflush(stdout);
  }
#endif

}
