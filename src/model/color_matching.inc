// Included inside namespace automaton after the contact-set declarations.
struct ColorProposal {
  unsigned owner, choice;
#ifdef INERTIA_MATCHED_FSM
  unsigned mate; SourceKind kind; int direction[3];
#endif
};
#ifdef INERTIA_MATCHED_FSM
static std::vector<unsigned char> inertiaVotes;
#endif
static std::vector<ColorProposal> matchMessages, matchNext;
static std::vector<unsigned> matchChoice, matchAccepted;

static bool colorBound(size_t i,unsigned w) {
  const size_t words=(W_USED+63u)/64u;
  return (boundContacts[i*words+w/64]>>(w%64))&1u;
}
static int colorAttraction(size_t i,unsigned w,int axis) {
  const size_t words=(W_USED+63u)/64u;
  const uint64_t bit=uint64_t(1)<<(w%64);
  return int((cohesionContacts[(i*6+2*axis)*words+w/64]&bit)!=0)
       - int((cohesionContacts[(i*6+2*axis+1)*words+w/64]&bit)!=0);
}
static int colorMatchedImpulse(size_t i,unsigned w,int axis) {
  if(matchAccepted[i]!=w) return 0;
#ifdef INERTIA_MATCHED_FSM
  if(inertiaEdge(i,w)) {
    const int kick=inertiaDirection(i,w,axis);
    if(lattice_curr[i].kind!=SourceKind::P) return kick;
    // After an agreed kick, reemit the pair one step toward the contacted
    // constituent's destination. This recycles the P near the body instead
    // of granting it perpetual forward flight. Reference geometry remains
    // explicit; the pair direction m itself is unchanged.
    const unsigned edge=axis==0?ELX:(axis==1?ELY:ELZ);
    int delta=int(lcenters[w][axis])+kick-int(lcenters[lattice_curr[i].w][axis]);
    if(delta>int(edge/2)) delta-=int(edge);
    if(delta<-int(edge/2)) delta+=int(edge);
    return (delta>0)-(delta<0);
  }
#endif
  if(colorBound(i,w)) return colorAttraction(i,w,axis);
  const Cell& a=lattice_curr[i];
  const uint64_t gap=(uint64_t(w)+W_USED-a.w)%W_USED;
  if(!gap || 2*gap==W_USED) return 0;
  return (2*gap<W_USED?1:-1)*(1-2*int((a.ch>>axis)&1));
}
int colorMatchedStep(unsigned w,unsigned partner,int axis) {
  const size_t i=&getCell(lattice_curr,0,0,0,w)-lattice_curr.data();
  return colorMatchedImpulse(i,partner,axis);
}
#ifdef INERTIA_MATCHED_FSM
bool colorInertiaGrant(unsigned w,unsigned partner) {
  const size_t i=&getCell(lattice_curr,0,0,0,w)-lattice_curr.data();
  return matchAccepted[i]==partner && inertiaEdge(i,partner);
}
#endif
static void colorMatchingTick(unsigned k,unsigned start) {
  const size_t words=(W_USED+63u)/64u;
  if(k==start) {
    matchChoice.assign(BLOCK,W_USED);matchAccepted.assign(BLOCK,W_USED);
    matchMessages.resize(BLOCK);matchNext.resize(BLOCK);
#ifdef INERTIA_MATCHED_FSM
    inertiaVotes.assign(BLOCK,0);
#endif
    for(size_t i=0;i<BLOCK;++i) {
      const unsigned a=lattice_curr[i].w;
      unsigned bestGap=W_USED,bestLow=W_USED,bestHigh=W_USED;
      for(unsigned b=0;b<W_USED;++b) {
        if(!((colorContacts[i*words+b/64]>>(b%64))&1u)) continue;
        unsigned rankA=a;
#ifdef INERTIA_MATCHED_FSM
        if(inertiaEdge(i,b)) {
          if(lattice_curr[i].kind==SourceKind::P) rankA=std::min(a,unsigned(lattice_curr[i].pair_idx));
          else {
            const unsigned mate=inertiaMate[i*W_USED+b];
            if(b>=mate || mate>=W_USED || inertiaMate[i*W_USED+mate]!=b || !inertiaEdge(i,mate)) continue;
          }
        }
#endif
        const unsigned gap=std::min((b+W_USED-rankA)%W_USED,(rankA+W_USED-b)%W_USED);
        if(!gap || 2*gap==W_USED) continue;
        if(colorBound(i,b) && !colorAttraction(i,b,0) &&
           !colorAttraction(i,b,1) && !colorAttraction(i,b,2)) continue;
        const unsigned low=std::min(rankA,b);
        const unsigned high=std::max(rankA,b);
        // An unordered edge has the same priority at both endpoints.
        // The W address is an explicit tie-break, not a scan-order effect.
        if(gap<bestGap || (gap==bestGap && (low<bestLow || (low==bestLow && high<bestHigh)))) {
          bestGap=gap;bestLow=low;bestHigh=high;matchChoice[i]=b;
        }
      }
      matchMessages[i]={a,matchChoice[i]};
#ifdef INERTIA_MATCHED_FSM
      auto& message=matchMessages[i];const unsigned choice=matchChoice[i];
      message.kind=lattice_curr[i].kind;message.mate=W_USED;
      if(choice<W_USED && inertiaEdge(i,choice)) {
        message.mate=message.kind==SourceKind::P?lattice_curr[i].pair_idx:inertiaMate[i*W_USED+choice];
        for(int axis=0;axis<3;++axis) message.direction[axis]=inertiaDirection(i,choice,axis);
      }
#endif
    }
  }
  if(k<start || k>=start+W_USED) return;
  for(size_t i=0;i<BLOCK;++i) {
    const Cell& a=lattice_curr[i];const auto& msg=matchMessages[i];
    bool ordinary=true;
#ifdef INERTIA_MATCHED_FSM
    const unsigned choice=matchChoice[i];
    if(choice<W_USED && inertiaEdge(i,choice)) {
      ordinary=false;
      const unsigned root=a.kind==SourceKind::P?std::min(unsigned(a.w),unsigned(a.pair_idx)):choice;
      const unsigned half=a.kind==SourceKind::P?std::max(unsigned(a.w),unsigned(a.pair_idx)):inertiaMate[i*W_USED+choice];
      const unsigned target=a.kind==SourceKind::P?choice:a.w;
      bool aligned=true;
      for(int axis=0;axis<3;++axis) aligned &= msg.direction[axis]==inertiaDirection(i,choice,axis);
      if(aligned) {
        if(msg.owner==root && msg.kind==SourceKind::P && msg.mate==half && msg.choice==target) inertiaVotes[i]|=1;
        if(msg.owner==half && msg.kind==SourceKind::P && msg.mate==root && msg.choice==target) inertiaVotes[i]|=2;
        if(msg.owner==target && msg.kind!=SourceKind::P && msg.mate==half && msg.choice==root) inertiaVotes[i]|=4;
      }
      if(inertiaVotes[i]==7) matchAccepted[i]=choice;
    }
#endif
    if(ordinary && msg.owner==matchChoice[i] && msg.choice==a.w) matchAccepted[i]=msg.owner;
    const size_t j=&getCell(lattice_curr,a.x[0],a.x[1],a.x[2],(a.w+1)%W_USED)-lattice_curr.data();
    matchNext[i]=matchMessages[j];
  }
  matchMessages.swap(matchNext);
}
