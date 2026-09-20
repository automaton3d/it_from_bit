#include "model/attractor.h"
#include "model/island_identity.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <limits>

namespace automaton { namespace attractor {
namespace {
constexpr uint32_t ORPHAN = UINT32_MAX;
struct Member { uint32_t label=ORPHAN, category=0; };
Observable mode_=Observable::ChiefConstituents;
std::vector<Member> prev_;
std::vector<uint64_t> pop_,cap_,esc_,flux_;
std::vector<int64_t> conversion_; // signed matter-minus-antimatter changes within membership
std::vector<Census> census_;
unsigned nBuckets_=0,frames_=0;
int64_t dIslOrb0_=0,dIslUmb0_=0;
unsigned category(const Cell& c) {
  const unsigned weight=(c.ch&1u)+((c.ch>>1)&1u)+((c.ch>>2)&1u);
  return ((c.ch>>5)&1u)*2u + (weight<2?0u:1u); // M, anti, M, anti
}
int sign(unsigned c) { return c%2?-1:1; }
std::vector<Member> snapshot(Census& census) {
  std::vector<Member> result;
  if(mode_==Observable::AffinityCells) {
    result.reserve(lattice_curr.size());
    for(const Cell& c:lattice_curr)
      result.push_back({c.a<W_USED?c.a:ORPHAN,category(c)});
    return result;
  }
  std::vector<const Cell*> sources(W_USED,nullptr);
  std::set<std::array<unsigned,3>> centers;
  for(const Cell& c:lattice_curr) if(c.r2==0) {
    if(c.w>=W_USED || sources[c.w]) throw std::runtime_error("census: invalid or duplicate source");
    sources[c.w]=&c;
    centers.insert({c.x[0],c.x[1],c.x[2]});
  }
  result.resize(W_USED);
  std::vector<unsigned> populations(W_USED,0);
  for(unsigned w=0;w<W_USED;++w)
    if(!sources[w]) throw std::runtime_error("census: missing source");
  for(unsigned w=0;w<W_USED;++w) {
    const Cell& c=*sources[w];
    uint32_t chief=ORPHAN;
    switch(c.kind) {
      case SourceKind::K: ++census.chiefs;chief=w;break;
      case SourceKind::D:
        ++census.delegates;
        if(c.parent<W_USED && sources[c.parent]->kind==SourceKind::K) chief=c.parent;
        else ++census.unresolved;
        break;
      case SourceKind::S: ++census.singletons;break;
      case SourceKind::P: ++census.pairs;break; // P halves, not reciprocal pair count
    }
    result[w]={chief,category(c)};
    if(chief!=ORPHAN) ++populations[chief];
  }
  census.occupiedCenters=static_cast<unsigned>(centers.size());
  if(EL>=3 && EL%3==0)
    for(unsigned n:populations) census.groupsOfTarget+=n==EL/3;
  return result;
}
bool ols(const std::vector<double>& x,const std::vector<double>& y,
         size_t begin,size_t end,double& slope,double& intercept,double& r2,double& se) {
  slope=intercept=r2=se=0;
  const size_t n=end-begin;if(n<8)return false;
  double sx=0,sy=0;
  for(size_t i=begin;i<end;++i){sx+=x[i];sy+=y[i];}
  const double mx=sx/n,my=sy/n;
  double xx=0,xy=0,yy=0;
  for(size_t i=begin;i<end;++i){double dx=x[i]-mx,dy=y[i]-my;xx+=dx*dx;xy+=dx*dy;yy+=dy*dy;}
  if(xx<=0)return false;
  slope=xy/xx;intercept=my-slope*mx;
  const double sse=std::max(0.0,yy-slope*xy);
  r2=yy>0?std::clamp(1-sse/yy,0.0,1.0):0;
  se=std::sqrt(sse/(n-2)/xx);return true;
}
}
void begin(Observable observable) {
  if(observable!=Observable::ChiefConstituents && observable!=Observable::AffinityCells) throw std::invalid_argument("unknown census observable");
  mode_=observable;nBuckets_=W_USED;frames_=0;
  pop_.clear();cap_.clear();esc_.clear();flux_.clear();conversion_.clear();census_.clear();
  Census c;prev_=snapshot(c);dIslOrb0_=dIslUmb0_=0;
  for(const Member& m:prev_) if(m.label!=ORPHAN)
    (m.category<2?dIslOrb0_:dIslUmb0_)+=sign(m.category);
}
void resyncPrev() {
  Census c;auto current=snapshot(c);
  if(frames_) {
    if(current.size()!=prev_.size())throw std::runtime_error("census checkpoint mismatch");
    for(size_t i=0;i<current.size();++i)
      if(current[i].label!=prev_[i].label || current[i].category!=prev_[i].category)
        throw std::runtime_error("census checkpoint mismatch; start a new series");
  } else {
    dIslOrb0_=dIslUmb0_=0;
    for(const Member& m:current)if(m.label!=ORPHAN)
      (m.category<2?dIslOrb0_:dIslUmb0_)+=sign(m.category);
  }
  prev_=std::move(current);
}
unsigned framesSampled(){return frames_;}
void sampleFrame(unsigned frame) {
  if(!frame)frame=frames_+1;
  if(frame!=frames_+1 || nBuckets_!=W_USED) throw std::runtime_error("census: nonsequential frame or changed topology");
  Census c;auto current=snapshot(c);
  if(current.size()!=prev_.size())throw std::runtime_error("census: changed snapshot size");
  const size_t off=size_t(frames_)*nBuckets_,foff=size_t(frames_)*8;
  pop_.resize(off+nBuckets_,0);cap_.resize(off+nBuckets_,0);esc_.resize(off+nBuckets_,0);
  flux_.resize(foff+8,0);conversion_.resize(size_t(frame)*2,0);
  for(size_t i=0;i<current.size();++i) {
    const Member a=prev_[i],b=current[i];
    if(b.label!=ORPHAN)++pop_[off+b.label];
    if(a.label!=b.label) {
      if(a.label!=ORPHAN){++esc_[off+a.label];++flux_[foff+4+a.category];}
      if(b.label!=ORPHAN){++cap_[off+b.label];++flux_[foff+b.category];}
    } else if(b.label!=ORPHAN && a.category!=b.category) {
      conversion_[size_t(frames_)*2+a.category/2]-=sign(a.category);
      conversion_[size_t(frames_)*2+b.category/2]+=sign(b.category);
    }
  }
  prev_=std::move(current);census_.push_back(c);frames_=frame;
}
// v4 uses an explicit observable, topology, 64-bit baseline, previous snapshot,
// and conversion series. Old affinity/flicker series must not be mixed with it.
bool saveSeries(const std::string& path) {
  FILE* f=fopen(path.c_str(),"wb");if(!f)return false;
  uint32_t h[]={0x41545452,4,uint32_t(mode_),W_USED,ELX,ELY,ELZ,frames_,nBuckets_};
  int64_t baseline[]={dIslOrb0_,dIslUmb0_};
  bool ok=fwrite(h,sizeof(h),1,f)==1 && fwrite(baseline,sizeof(baseline),1,f)==1;
  auto put=[&](const auto& v){if(!v.empty())ok=(fwrite(v.data(),sizeof(v[0]),v.size(),f)==v.size())&&ok;};
  put(prev_);put(pop_);put(cap_);put(esc_);put(flux_);put(conversion_);put(census_);
  return fclose(f)==0 && ok;
}
bool loadSeries(const std::string& path) {
  FILE* f=fopen(path.c_str(),"rb");if(!f)return false;
  uint32_t h[9]{};int64_t baseline[2]{};
  bool ok=fread(h,sizeof(h),1,f)==1 && h[0]==0x41545452 && h[1]==4 &&
    h[2]==uint32_t(mode_) && h[3]==W_USED && h[4]==ELX && h[5]==ELY && h[6]==ELZ && h[8]==nBuckets_;
  if(!ok){fclose(f);return false;}
  // Check the exact byte length before trusting the recorded frame count.
  const uint64_t count=uint64_t(h[7])*nBuckets_;
  const uint64_t expected=sizeof(h)+sizeof(baseline)+prev_.size()*sizeof(Member)+
    count*3*sizeof(uint64_t)+uint64_t(h[7])*(8*sizeof(uint64_t)+2*sizeof(int64_t)+sizeof(Census));
  _fseeki64(f,0,SEEK_END);const auto length=_ftelli64(f);
  if(length<0 || uint64_t(length)!=expected){fclose(f);return false;}
  _fseeki64(f,sizeof(h),SEEK_SET);
  auto previous=prev_;std::vector<uint64_t> p(count),c(count),e(count),flux(size_t(h[7])*8);
  std::vector<int64_t> conversion(size_t(h[7])*2);std::vector<Census> census(h[7]);
  ok=fread(baseline,sizeof(baseline),1,f)==1;
  auto get=[&](auto& v){if(!v.empty())ok=(fread(v.data(),sizeof(v[0]),v.size(),f)==v.size())&&ok;};
  get(previous);get(p);get(c);get(e);get(flux);get(conversion);get(census);fclose(f);
  for(const Member& m:previous)ok=ok && (m.label==ORPHAN || m.label<W_USED) && m.category<4;
  if(!ok)return false;
  prev_=std::move(previous);pop_=std::move(p);cap_=std::move(c);esc_=std::move(e);flux_=std::move(flux);
  conversion_=std::move(conversion);census_=std::move(census);frames_=h[7];dIslOrb0_=baseline[0];dIslUmb0_=baseline[1];return true;
}
bool writeCensusCSV(const std::string& path) {
  if(mode_!=Observable::ChiefConstituents)return false;
  FILE* f=fopen(path.c_str(),"w");if(!f)return false;
  fprintf(f,"frame,K,D,S,P_halves,unresolved,occupied_centers,groups_of_L_over_3\n");
  for(size_t i=0;i<census_.size();++i){const auto& c=census_[i];fprintf(f,"%zu,%u,%u,%u,%u,%u,%u,%u\n",i+1,c.chiefs,c.delegates,c.singletons,c.pairs,c.unresolved,c.occupiedCenters,c.groupsOfTarget);}
  const bool ok=!ferror(f);return fclose(f)==0 && ok;
}
    Report summarize()
    {
      Report rep;
      rep.observable=mode_;
      if(!census_.empty())rep.census=census_.back();
      rep.frames   = frames_;
      rep.nBuckets = nBuckets_;
      rep.nIslands = rep.census.chiefs;
      rep.islands.resize(nBuckets_);

      std::vector<double> xs, ys;   // pooled (N_t, dN_t) points

      double totPop = 0.0;
      unsigned totFrames = 0;

      for (unsigned g = 0; g < nBuckets_; ++g)
      {
        IslandStats& st = rep.islands[g];
        st.island = g;

        const size_t F = frames_;
        if (F == 0) continue;

        std::vector<double> N(F), dN(F - 1);
        double s = 0, s2 = 0;
        double mn = 1e300, mx = -1e300;
        double caps = 0, escs = 0;

        for (size_t t = 0; t < F; ++t)
        {
          const double v = static_cast<double>(pop_[t * nBuckets_ + g]);
          N[t] = v;
          s += v; s2 += v * v;
          mn = std::min(mn, v); mx = std::max(mx, v);
          caps += static_cast<double>(cap_[t * nBuckets_ + g]);
          escs += static_cast<double>(esc_[t * nBuckets_ + g]);
        }

        st.meanN   = s / F;
        st.stdN    = std::sqrt(std::max(0.0, s2 / F - st.meanN * st.meanN));
        st.minN    = mn;
        st.maxN    = mx;
        st.capRate = caps / F;
        st.escRate = escs / F;
        st.samples = static_cast<long>(F);

        for (size_t t = 0; t + 1 < F; ++t)
        {
          dN[t] = N[t + 1] - N[t];
          if(N[t]>0){xs.push_back(N[t]);ys.push_back(dN[t]);}
        }

        st.hasFit = ols(N, dN, 0, dN.size(), st.slope, st.intercept, st.r2, st.slopeSE);
        if (st.hasFit && st.slope < 0.0)
          st.nStar = -st.intercept / st.slope;

        totPop += s;
        totFrames = static_cast<unsigned>(F);
      }

      rep.meanTotalPop = (totFrames > 0) ? totPop / totFrames : 0.0;

      if (frames_ > 0)
      {
        double c = 0, e = 0;
        for (size_t t = 0; t < frames_; ++t)
          for (unsigned g = 0; g < nBuckets_; ++g)
          {
            c += static_cast<double>(cap_[t * nBuckets_ + g]);
            e += static_cast<double>(esc_[t * nBuckets_ + g]);
          }
        rep.meanCaptures = c / frames_;
        rep.meanEscapes  = e / frames_;
      }

      // Pooled regression across all islands: descriptive only; temporal dependence is not modeled.
      rep.pooledPoints = static_cast<long>(xs.size());
      rep.pooledHasFit = ols(xs, ys, 0, xs.size(),
          rep.pooledSlope, rep.pooledIntercept, rep.pooledR2, rep.pooledSlopeSE);

      if (rep.pooledHasFit && rep.pooledSlope < 0.0)
        rep.pooledNStar = -rep.pooledIntercept / rep.pooledSlope;

      return rep;
    }

    // Accumulated sector flux over all sampled frames.
    struct SectorTotals
    {
      uint64_t capM_Orb = 0, capA_Orb = 0, capM_Umb = 0, capA_Umb = 0;
      uint64_t escM_Orb = 0, escA_Orb = 0, escM_Umb = 0, escA_Umb = 0;
    };

    SectorTotals fluxTotals()
    {
      SectorTotals t;
      for (unsigned fr = 0; fr < frames_; ++fr)
      {
        const size_t foff = static_cast<size_t>(fr) * 8;
        t.capM_Orb += flux_[foff + 0];
        t.capA_Orb += flux_[foff + 1];
        t.capM_Umb += flux_[foff + 2];
        t.capA_Umb += flux_[foff + 3];
        t.escM_Orb += flux_[foff + 4];
        t.escA_Orb += flux_[foff + 5];
        t.escM_Umb += flux_[foff + 6];
        t.escA_Umb += flux_[foff + 7];
      }
      return t;
    }

    void printSectorFlux(const SectorTotals& t, unsigned frames)
    {
      const double f = (frames > 0) ? (double)frames : 1.0;
      // Net balance of D = mat - anti flowing INTO islands per frame, by sector.
      //   netD_orb = (capM-capA)_orb - (escM-esA)_orb
      //   netD_umb = (capM-capA)_umb - (escM-esA)_umb
      const double netD_orb =
          ((double)t.capM_Orb - (double)t.capA_Orb) - ((double)t.escM_Orb - (double)t.escA_Orb);
      const double netD_umb =
          ((double)t.capM_Umb - (double)t.capA_Umb) - ((double)t.escM_Umb - (double)t.escA_Umb);

      printf("----------------------------------------------------------------\n");
      printf("SECTOR FLUX  (aggregate %u frames, per-frame rates)\n", frames);
      // Measured balance = initial balance + net membership flux + conversion.
      // Chief constituents and affinity cells have different units.
      const long long cumOrb = (long long)t.capM_Orb - (long long)t.capA_Orb -
                               (long long)t.escM_Orb + (long long)t.escA_Orb;
      const long long cumUmb = (long long)t.capM_Umb - (long long)t.capA_Umb -
                               (long long)t.escM_Umb + (long long)t.escA_Umb;
      int64_t convOrb=0,convUmb=0;
      for(unsigned i=0;i<frames_;++i){convOrb+=conversion_[size_t(i)*2];convUmb+=conversion_[size_t(i)*2+1];}
      printf("Within-membership charge conversion: Orbis=%lld Umbra=%lld\n",(long long)convOrb,(long long)convUmb);
      printf("                capM   capA   |   escM   escA   |  netD/fr  D_members(pred)\n");
      printf("  Orbis (w1=0) %5.1f %6.1f | %6.1f %6.1f | %+8.2f  %+9lld\n",
             (double)t.capM_Orb / f, (double)t.capA_Orb / f,
             (double)t.escM_Orb / f, (double)t.escA_Orb / f, netD_orb / f,
             (long long)(dIslOrb0_ + cumOrb + convOrb));
      printf("  Umbra (w1=1) %5.1f %6.1f | %6.1f %6.1f | %+8.2f  %+9lld\n",
             (double)t.capM_Umb / f, (double)t.capA_Umb / f,
             (double)t.escM_Umb / f, (double)t.escA_Umb / f, netD_umb / f,
             (long long)(dIslUmb0_ + cumUmb + convUmb));

      // The headline question: does Umbra shed anti faster than Orbis sheds
      // matter while Orbis keeps netting matter?
      const double escA_Umb = (double)t.escA_Umb / f;
      const double escM_Orb = (double)t.escM_Orb / f;
      const double netM_Orb = ((double)t.capM_Orb - (double)t.escM_Orb) / f;
      const double netA_Umb = ((double)t.capA_Umb - (double)t.escA_Umb) / f;
      printf("  Umbra anti escape rate    %8.2f units/frame\n", escA_Umb);
      printf("  Orbis matter escape rate  %8.2f units/frame\n", escM_Orb);
      printf("  Umbra net anti membership flux %+8.2f units/frame (neg = shedding)\n", netA_Umb);
      printf("  Orbis net matter membership flux %+8.2f units/frame (pos = gaining)\n", netM_Orb);
      printf("----------------------------------------------------------------\n");
    }

    void printReport(const Report& rep)
    {
      printf("\n==============================================================\n");
      printf("%s\n",mode_==Observable::ChiefConstituents?"CHIEF CONSTITUENT CENSUS":"AFFINITY CELL OCCUPANCY (NOT ISLAND POPULATION)");
      printf("==============================================================\n");
      printf("lattice: EL=%u W_USED=%u RMAX=%u ISLAND_SIZE=%u buckets=%u\n",
             EL, W_USED, RMAX, ISLAND_SIZE, rep.nBuckets);
      printf("frames sampled: %u\n", rep.frames);
      printf("mean total measured population: %.1f units\n", rep.meanTotalPop);
      if(mode_==Observable::ChiefConstituents)printf("Latest: K=%u D=%u S=%u P_halves=%u unresolved=%u centers=%u groups_of_L/3=%u\n",rep.census.chiefs,rep.census.delegates,rep.census.singletons,rep.census.pairs,rep.census.unresolved,rep.census.occupiedCenters,rep.census.groupsOfTarget);
      printf("gross turnover: captures/frame=%.2f  escapes/frame=%.2f\n",
             rep.meanCaptures, rep.meanEscapes);
      printSectorFlux(fluxTotals(), frames_);

      printf("\nPooled dN ~ N regression (%ld points):\n", rep.pooledPoints);
      if (rep.pooledHasFit)
      {
        printf("  slope     = %+.6g  (+- %.2g)\n", rep.pooledSlope, rep.pooledSlopeSE);
        printf("  intercept = %+.6g\n", rep.pooledIntercept);
        printf("  r2        = %.4f\n", rep.pooledR2);
        if (rep.pooledSlope < 0.0)
          printf("  ==> negative descriptive slope; fitted zero ~= %.2f\n", rep.pooledNStar);
        else
          printf("  ==> nonnegative descriptive slope\n");
      }
      else
        printf("  insufficient data\n");

      printf("Negative slope alone does not establish stability; OLS errors assume independent residuals.\n");
      // Descriptive ranking only.
      std::vector<const IslandStats*> ranked;
      for (const auto& st : rep.islands)
        if (st.samples > 0)
          ranked.push_back(&st);

      std::sort(ranked.begin(), ranked.end(),
                [](const IslandStats* a, const IslandStats* b)
      {
        const double ta = (a->slopeSE > 0) ? std::fabs(a->slope / a->slopeSE) : 0.0;
        const double tb = (b->slopeSE > 0) ? std::fabs(b->slope / b->slopeSE) : 0.0;
        return ta > tb;
      });

      printf("\naddress buckets ranked by descriptive slope/SE:\n");
      printf("%6s %10s %9s %9s %9s %+11s %8s %9s %8s\n",
             "address", "meanN", "std", "min", "max", "slope", "r2", "N*", "cap/fr");
      int shown = 0;
      for (const IslandStats* st : ranked)
      {
        if (shown++ >= 12) break;
        printf("%6u %10.2f %9.2f %9.0f %9.0f %+11.3g %8.3f ",
               st->island, st->meanN, st->stdN, st->minN, st->maxN,
               st->slope, st->r2);
        if (st->hasFit && st->slope < 0.0)
          printf("%9.2f ", st->nStar);
        else
          printf("%9s ", "--");
        printf("%8.2f\n", st->capRate);
      }
      printf("==============================================================\n");
    }

    bool sectorCSVToFile(const std::string& path)
    {
      FILE* f = fopen(path.c_str(), "w");
      if (!f) return false;
      fprintf(f, "frame,capM_Orb,capA_Orb,capM_Umb,capA_Umb,escM_Orb,escA_Orb,escM_Umb,escA_Umb,conversionD_Orb,conversionD_Umb\n");
      for (unsigned fr = 0; fr < frames_; ++fr)
      {
        const size_t foff = static_cast<size_t>(fr) * 8;
        fprintf(f, "%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lld,%lld\n",
                fr + 1,
                (unsigned long long)flux_[foff + 0],
                (unsigned long long)flux_[foff + 1],
                (unsigned long long)flux_[foff + 2],
                (unsigned long long)flux_[foff + 3],
                (unsigned long long)flux_[foff + 4],
                (unsigned long long)flux_[foff + 5],
                (unsigned long long)flux_[foff + 6],
                (unsigned long long)flux_[foff + 7],
                (long long)conversion_[size_t(fr)*2], (long long)conversion_[size_t(fr)*2+1]);
      }
      const bool ok = !ferror(f);
      return fclose(f) == 0 && ok;
    }

    bool writeCSV(const std::string& path, const Report& rep)
    {
      (void)rep;
      FILE* f = fopen(path.c_str(), "w");
      if (!f) return false;
      fprintf(f, mode_==Observable::ChiefConstituents?"frame,chief_w,constituents,captures,escapes\n":"frame,affinity,occupied_cells,captures,escapes\n");
      for (unsigned t = 0; t < frames_; ++t)
        for (unsigned g = 0; g < nBuckets_; ++g)
          fprintf(f, "%u,%u,%llu,%llu,%llu\n",
                  t + 1, g,
                  (unsigned long long)pop_[t * nBuckets_ + g],
                  (unsigned long long)cap_[t * nBuckets_ + g],
                  (unsigned long long)esc_[t * nBuckets_ + g]);
      const bool ok = !ferror(f);
      return fclose(f) == 0 && ok;
    }

    // Public wrapper: persist the 8-sector-flux series to its own CSV.
    bool writeSectorCSV(const std::string& path)
    {
      return sectorCSVToFile(path);
    }

} } // namespace automaton::attractor

