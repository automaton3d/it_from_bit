// Experimental P-body contact payloads; included inside namespace automaton.
static std::vector<uint64_t> inertiaDirections, inertiaDirectionsNext;
static std::vector<unsigned> inertiaMate, inertiaMateNext;
static bool inertiaBody(const Cell& a) { return a.kind==SourceKind::K || a.kind==SourceKind::D; }
static void inertiaReset(size_t words) {
  inertiaDirections.assign(size_t(BLOCK)*6*words,0);
  inertiaDirectionsNext.resize(inertiaDirections.size());
  inertiaMate.assign(size_t(BLOCK)*W_USED,W_USED);
  inertiaMateNext.resize(inertiaMate.size());
}
static void inertiaRecord(size_t i,const Cell& a,const Cell& b,size_t words) {
  const Cell& p=a.kind==SourceKind::P?a:b;
  const Cell& body=a.kind==SourceKind::P?b:a;
  if(!inertiaBody(body) || p.kind!=SourceKind::P || p.a!=body.a ||
     p.pair_idx>=W_USED || p.pair_idx==p.w || !p.pair_count ||
     (!p.m[0] && !p.m[1] && !p.m[2])) return;
  colorContacts[i*words+b.w/64] |= uint64_t(1)<<(b.w%64);
  if(a.kind!=SourceKind::P) inertiaMate[i*W_USED+b.w]=p.pair_idx;
  for(int axis=0;axis<3;++axis) if(p.m[axis]) {
    const unsigned channel=2*axis+(p.m[axis]<0);
    inertiaDirections[(i*6+channel)*words+b.w/64] |= uint64_t(1)<<(b.w%64);
  }
}
static int inertiaDirection(size_t i,unsigned w,int axis) {
  const size_t words=(W_USED+63u)/64u;
  const uint64_t bit=uint64_t(1)<<(w%64);
  return int((inertiaDirections[(i*6+2*axis)*words+w/64]&bit)!=0)
       - int((inertiaDirections[(i*6+2*axis+1)*words+w/64]&bit)!=0);
}
static bool inertiaEdge(size_t i,unsigned w) {
  return inertiaDirection(i,w,0) || inertiaDirection(i,w,1) || inertiaDirection(i,w,2);
}
