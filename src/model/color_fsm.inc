// Included inside namespace automaton, after phase_step(). Experimental FSM.
// The ledger has one bit per encountered W identity at each voxel. Repeated
// contacts add no weight. OR diffusion is synchronous and idempotent.
static std::vector<uint64_t> colorContacts, colorContactsNext;
static std::vector<unsigned char> colorReemit;
#ifdef KK_CLASH_FSM
static std::vector<WIndex> clashChief, clashChiefNext;
#endif
#ifdef DD_PROMOTION_FSM
static std::vector<unsigned char> delegatePromotion, delegatePromotionNext;
#endif
#ifdef COAGULATION_RANDOM_FSM
// Explicit experimental intervention, defined by the standalone harness.
// This is not part of the automaton's proposed physical rule.
void coagulationIntervene();
#endif
#ifdef LIGHT_MATTER_FSM
// Provisional light-matter decoupling stand-in (pB/sB channel).  The real
// polarisation broadcast that would set pB/sB is deferred; the harness
// assigns each source a deterministic random light-matter label instead
// (same SplitMix64 pattern as the coagulation experiment).  At a bound,
// coincident, equal-charge contact whose labels differ, the matched
// impulse (zero at coincidence) is overridden by one antisymmetric
// separation step, so superposed constituents decouple.
void lightMatterInit();
int lightMatterKick(unsigned a, unsigned w, int axis);
#endif
#ifdef EXCLUSION_FSM
// Pauli-like identity hard core (experimental candidate).  Equal-charge
// sources from DIFFERENT seed families (w/3, the L/3 identical copies of a
// W-island) cannot share a chief: their contact is not recorded for
// election, cohesion or colour kicks, so one family island can never absorb
// another into a charge-word tribe.  Copies inside the same family interact
// normally (they must elect their own 1K+nD island).  This is a candidate
// identity rule, not the current manuscript election rule.
static unsigned long long exclusionContacts = 0;
static std::vector<uint64_t> colorExcl, colorExclNext;  // cross-family sets
unsigned long long colorExclusionTotal() { return exclusionContacts; }
#endif
#ifdef INERTIA_MATCHED_FSM
#include "inertia_contact.inc"
#endif
#ifdef COLOR_COHESION_FSM
// Six sets encode each partner's signed axis contributions, counted once.
static std::vector<uint64_t> cohesionContacts, cohesionNext;
#ifdef COLOR_MATCHED_FSM
static std::vector<uint64_t> boundContacts, boundNext;
#endif
int colorCohesionStep(unsigned w,unsigned partner,int axis) {
  const size_t words=(W_USED+63u)/64u;
  const size_t i=&getCell(lattice_curr,0,0,0,w)-lattice_curr.data();
  const size_t base=i*6*words;
  const uint64_t bit=uint64_t(1)<<(partner%64);
  return int((cohesionContacts[base+2*axis*words+partner/64]&bit)!=0)
       - int((cohesionContacts[base+(2*axis+1)*words+partner/64]&bit)!=0);
}
#endif
#ifdef COLOR_CHIEF_FSM
// One candidate per voxel. Unlike the seed's leader field, an S proposes
// its own address. Candidates travel through encounter and neighbor diffusion.
static std::vector<WIndex> colorChief, colorChiefNext;
static WIndex colorChiefCandidate(const Cell& c) {
  if(c.kind==SourceKind::D && c.parent<W_USED) return c.parent;
  return c.w;
}
#endif
// Matching uses a second synchronous W circulation after spatial diffusion.
// These messages contain proposals only; no remote draft is accessed.
#ifdef COLOR_MATCHED_FSM
#include "color_matching.inc"
#endif
// Read-only diagnostics, sampled only after the relevant FSM stages.
bool colorFsmContact(unsigned w,unsigned partner) {
  const size_t words=(W_USED+63u)/64u;
  const size_t i=&getCell(lattice_curr,0,0,0,w)-lattice_curr.data();
  return (colorContacts[i*words+partner/64]>>(partner%64))&1u;
}
bool colorFsmReemitted(unsigned w) { return colorReemit[w]!=0; }
static void colorFsmTick() {
  const unsigned k=lattice_curr.front().k;
  const size_t words=(W_USED+63u)/64u;
  const unsigned edges[3]={ELX,ELY,ELZ};
  const unsigned diameter=ELX/2+ELY/2+ELZ/2;
  if(k==0) {
    colorContacts.assign(size_t(BLOCK)*words,0);
    colorContactsNext.resize(colorContacts.size());
    colorReemit.assign(W_USED,0);
#ifdef EXCLUSION_FSM
    colorExcl.assign(size_t(BLOCK)*words,0);
    colorExclNext.resize(colorExcl.size());
#endif
#ifdef KK_CLASH_FSM
    clashChief.assign(BLOCK,NO_PARENT);clashChiefNext.resize(BLOCK);
#endif
#ifdef DD_PROMOTION_FSM
    delegatePromotion.assign(BLOCK,0);delegatePromotionNext.resize(BLOCK);
#endif
#ifdef INERTIA_MATCHED_FSM
    inertiaReset(words);
#endif
#ifdef COLOR_COHESION_FSM
    cohesionContacts.assign(size_t(BLOCK)*6*words,0);
    cohesionNext.resize(cohesionContacts.size());
#ifdef COLOR_MATCHED_FSM
    boundContacts.assign(size_t(BLOCK)*words,0);boundNext.resize(boundContacts.size());
    if(ENCOUNTER+diameter+W_USED>=DIFFUSION-1)
      throw std::runtime_error("insufficient FSM slots for proposal circulation");
#endif
#endif
#ifdef COLOR_CHIEF_FSM
    colorChief.resize(BLOCK);colorChiefNext.resize(BLOCK);
    for(size_t i=0;i<BLOCK;++i) colorChief[i]=colorChiefCandidate(lattice_curr[i]);
#endif
  }
  update_pulsating_wavefront();
  ++pulse_tick;
  phase_step();
  lattice_curr.swap(lattice_draft);
  if(k==0) lattice_partner=lattice_curr; // current encounter snapshot
  lattice_draft=lattice_curr;

  if(k<ENCOUNTER) {
    for(size_t i=0;i<BLOCK;++i) {
      const Cell& a=lattice_curr[i];const Cell& b=lattice_partner[i];
      if(!a.active || !b.active || a.r<=0 || b.r<=0 || a.w==b.w) continue;
#ifdef INERTIA_MATCHED_FSM
      if(a.kind==SourceKind::P || b.kind==SourceKind::P) {
        inertiaRecord(i,a,b,words);continue;
      }
#endif
      if(a.ch!=b.ch) continue;
#ifdef EXCLUSION_FSM
      // Hard-core identity exclusion between different seed families: the
      // contact is kept only for the exclusion push, never for election,
      // cohesion or colour kicks, so families cannot merge into tribes.
      if (a.w / 3u != b.w / 3u) {
        ++exclusionContacts;
        colorExcl[i*words+b.w/64] |= uint64_t(1)<<(b.w%64);
        continue;
      }
#endif
#ifdef KK_CLASH_FSM
      if(demotesChief(a,b)) clashChief[i]=std::min(clashChief[i],b.w);
#endif
      colorContacts[i*words+b.w/64] |= uint64_t(1)<<(b.w%64);
#ifdef DD_PROMOTION_FSM
      delegatePromotion[i]=static_cast<unsigned char>(delegatePromotion[i] || promotesDelegate(a,b));
#endif
#ifdef COLOR_COHESION_FSM
      auto chief=[](const Cell& s) {
        return s.kind==SourceKind::K?s.w:(s.kind==SourceKind::D?s.parent:NO_PARENT);
      };
      if(chief(a)!=NO_PARENT && chief(a)==chief(b)) {
#ifdef COLOR_MATCHED_FSM
        boundContacts[i*words+b.w/64] |= uint64_t(1)<<(b.w%64);
#endif
        // Provisional reference geometry: no remote draft writes. Each
        // unilateral contact records its own signed step; reciprocal visits
        // must supply the reverse independently.
        for(int axis=0;axis<3;++axis) {
          int delta=int(lcenters[b.w][axis])-int(lcenters[a.w][axis]);
          if(delta>int(edges[axis]/2)) delta-=edges[axis];
          if(delta<-int(edges[axis]/2)) delta+=edges[axis];
          if(std::abs(delta)<=1 || 2*std::abs(delta)==int(edges[axis])) continue;
          const int channel=2*axis+(delta<0);
          cohesionContacts[(i*6+channel)*words+b.w/64] |= uint64_t(1)<<(b.w%64);
        }
      }
#endif
#ifdef COLOR_CHIEF_FSM
      // Unilateral: only main's candidate changes. Mirror remains read-only.
      colorChief[i]=std::min(colorChief[i],colorChiefCandidate(b));
#endif
    }
  } else if(k<ENCOUNTER+diameter) {
    colorContactsNext=colorContacts;
#ifdef EXCLUSION_FSM
    colorExclNext=colorExcl;
#endif
#ifdef KK_CLASH_FSM
    clashChiefNext=clashChief;
#endif
#ifdef DD_PROMOTION_FSM
    delegatePromotionNext=delegatePromotion;
#endif
#ifdef INERTIA_MATCHED_FSM
    inertiaDirectionsNext=inertiaDirections;inertiaMateNext=inertiaMate;
#endif
#ifdef COLOR_COHESION_FSM
    cohesionNext=cohesionContacts;
#ifdef COLOR_MATCHED_FSM
    boundNext=boundContacts;
#endif
#endif
#ifdef COLOR_CHIEF_FSM
    colorChiefNext=colorChief;
#endif
    for(size_t i=0;i<BLOCK;++i) {
      Cell& c=lattice_curr[i];
      for(int dir=0;dir<6;++dir) {
        size_t j=&c.getNeighbor(dir)-lattice_curr.data();
#ifdef KK_CLASH_FSM
        clashChiefNext[i]=std::min(clashChiefNext[i],clashChief[j]);
#endif
#ifdef DD_PROMOTION_FSM
        delegatePromotionNext[i] |= delegatePromotion[j];
#endif
#ifdef INERTIA_MATCHED_FSM
        for(size_t b=0;b<6*words;++b)
          inertiaDirectionsNext[i*6*words+b] |= inertiaDirections[j*6*words+b];
        for(unsigned b=0;b<W_USED;++b)
          inertiaMateNext[i*W_USED+b]=std::min(inertiaMateNext[i*W_USED+b],inertiaMate[j*W_USED+b]);
#endif
#ifdef COLOR_COHESION_FSM
        for(size_t b=0;b<6*words;++b)
          cohesionNext[i*6*words+b] |= cohesionContacts[j*6*words+b];
#ifdef COLOR_MATCHED_FSM
        for(size_t b=0;b<words;++b) boundNext[i*words+b] |= boundContacts[j*words+b];
#endif
#endif
#ifdef COLOR_CHIEF_FSM
        colorChiefNext[i]=std::min(colorChiefNext[i],colorChief[j]);
#endif
        for(size_t b=0;b<words;++b)
          colorContactsNext[i*words+b] |= colorContacts[j*words+b];
#ifdef EXCLUSION_FSM
        for(size_t b=0;b<words;++b)
          colorExclNext[i*words+b] |= colorExcl[j*words+b];
#endif
      }
    }
    colorContacts.swap(colorContactsNext);
#ifdef EXCLUSION_FSM
    colorExcl.swap(colorExclNext);
#endif
#ifdef KK_CLASH_FSM
    clashChief.swap(clashChiefNext);
#endif
#ifdef DD_PROMOTION_FSM
    delegatePromotion.swap(delegatePromotionNext);
#endif
#ifdef INERTIA_MATCHED_FSM
    inertiaDirections.swap(inertiaDirectionsNext);inertiaMate.swap(inertiaMateNext);
#endif
#ifdef COLOR_COHESION_FSM
    cohesionContacts.swap(cohesionNext);
#ifdef COLOR_MATCHED_FSM
    boundContacts.swap(boundNext);
#endif
#endif
#ifdef COLOR_CHIEF_FSM
    colorChief.swap(colorChiefNext);
#endif
  }
#ifdef COLOR_MATCHED_FSM
  colorMatchingTick(k,ENCOUNTER+diameter);
#endif
  if(k==DIFFUSION-1) {
    // Every voxel independently reconstructs its c from the locally
    // received set. No source-level contact list or remote source write.
    for(size_t i=0;i<BLOCK;++i) {
      const Cell& a=lattice_curr[i];Cell& d=lattice_draft[i];
      for(int axis=0;axis<3;++axis) d.c[axis]=0;
#ifdef COLOR_CHIEF_FSM
      bool met=false;
      for(size_t word=0;word<words;++word) met |= colorContacts[i*words+word]!=0;
      if(met && a.kind!=SourceKind::P
#ifdef DD_PROMOTION_FSM
         && a.kind==SourceKind::S
#endif
      ) {
        const WIndex chief=colorChief[i];
        d.kind=d.w==chief?SourceKind::K:SourceKind::D;
        d.parent=d.w==chief?NO_PARENT:chief;
        d.leader_w=chief;
      }
#ifdef DD_PROMOTION_FSM
      if(delegatePromotion[i]) makeChief(d);
#endif
#ifdef KK_CLASH_FSM
      if(a.kind==SourceKind::K && clashChief[i]<W_USED) makeDelegate(d,clashChief[i]);
#endif
#endif
      for(unsigned w=0;w<W_USED;++w) if(colorContacts[i*words+w/64] & (uint64_t(1)<<(w%64))) {
        uint64_t gap=(uint64_t(w)+W_USED-a.w)%W_USED;
        if(!gap || 2*gap==W_USED) continue;
        int sign=2*gap<W_USED?1:-1;
        for(int axis=0;axis<3;++axis) {
          int step=sign*(1-2*int((a.ch>>axis)&1));
#ifdef COLOR_COHESION_FSM
          const uint64_t bit=uint64_t(1)<<(w%64);
          step+=int((cohesionContacts[(i*6+2*axis)*words+w/64]&bit)!=0);
          step-=int((cohesionContacts[(i*6+2*axis+1)*words+w/64]&bit)!=0);
#endif
#ifdef COLOR_MATCHED_FSM
          step=colorMatchedImpulse(i,w,axis);
#endif
#ifdef LIGHT_MATTER_FSM
          { const int lm=lightMatterKick(a.w,w,axis); if(lm) step=lm; }
#endif
          d.c[axis]=(int(d.c[axis])+step+int(edges[axis]))%edges[axis];
        }
      }
#ifdef EXCLUSION_FSM
      // Exclusion push: one antisymmetric separation step for each
      // equal-charge cross-family contact, along the axis of largest
      // shortest-torus separation (W-derived axis when coincident).
      for(unsigned w=0;w<W_USED;++w)
        if(colorExcl[i*words+w/64] & (uint64_t(1)<<(w%64))) {
          int sep[3]={0,0,0}; int best=0,bestAbs=-1;
          for(int axis=0;axis<3;++axis){
            int dlt=int(lcenters[w][axis])-int(lcenters[a.w][axis]);
            if(dlt>int(edges[axis]/2)) dlt-=edges[axis];
            if(dlt<-int(edges[axis]/2)) dlt+=edges[axis];
            sep[axis]=dlt;
            int ad=dlt<0?-dlt:dlt;
            if(ad>bestAbs){bestAbs=ad;best=axis;}
          }
          int axis, sgn;
          if(bestAbs>0){ axis=best; sgn= sep[axis]>0 ? -1 : 1; }
          else { axis=int((uint64_t(a.w)+w)%3u); sgn= a.w<w ? -1 : 1; }
          d.c[axis]=(int(d.c[axis])+sgn+int(edges[axis]))%edges[axis];
        }
#endif
    }
#ifdef COAGULATION_RANDOM_FSM
    coagulationIntervene();
#endif
    // Verification only: transport requires a consistent layer field.
    for(size_t i=0;i<BLOCK;++i) {
      const Cell& a=lattice_draft[i];
      const Cell& ref=getCell(lattice_draft,0,0,0,a.w);
#ifdef COLOR_CHIEF_FSM
      if(a.kind!=ref.kind || a.parent!=ref.parent || a.leader_w!=ref.leader_w)
        throw std::runtime_error("chief diffusion incomplete");
#endif
      for(int axis=0;axis<3;++axis)
        if(a.c[axis]!=ref.c[axis]) throw std::runtime_error("color contact diffusion incomplete");
    }
  }
  if(k>=DIFFUSION && k<RELOC) {
    int axis=k<SLOT6?0:(k<SLOT7?1:2);
    for(size_t i=0;i<BLOCK;++i) {
      const Cell& a=lattice_curr[i];
      if(!a.c[axis]) continue;
      unsigned p[3]={a.x[0],a.x[1],a.x[2]};
      int direction=1;
#ifdef INERTIA_MATCHED_FSM
      // Matched impulses are restricted to -1, 0, +1. Consume the negative
      // residue with one opposite neighbor pull, not a full torus circuit.
      if(a.c[axis]==edges[axis]-1) direction=-1;
#endif
      p[axis]=(int(p[axis])+int(edges[axis])-direction)%edges[axis];
      Cell& d=lattice_draft[i];
      d=getCell(lattice_curr,p[0],p[1],p[2],a.w);
      if(direction<0) d.c[axis]=0;else --d.c[axis];
      for(int j=0;j<4;++j) d.x[j]=a.x[j];
      if(a.r2==0) colorReemit[a.w]=1;
    }
    // Refresh the reference geometry cache from actual transported cells.
    // This is not a position override. The existing geometry still reads
    // this cache; the experiment does not certify its spatial locality.
    std::vector<unsigned> sources(W_USED,0);
    for(const Cell& c:lattice_draft) if(c.r2==0) {
      ++sources[c.w];lcenters[c.w]={c.x[0],c.x[1],c.x[2]};
    }
    for(unsigned n:sources) if(n!=1) throw std::runtime_error("source lost or duplicated during translation");
  }
  for(size_t i=0;i<BLOCK;++i) {
    Cell& d=lattice_draft[i];
    if(k==RELOC && colorReemit[d.w]) {
      const bool source=d.r2==0;
      d.t=0;d.f=0;d.active=0;d.phiB=false;
      d.r2=source?0u:INF_R2;d.r=source?0:-1;
      d.u=source?2048:0;d.v=0;
    }
    d.k=(k+1)%FRAME;
    if(d.k==0) { d.t=(d.t+1)%(2*RMAX);d.f=effective_t(d.t); }
  }
}
