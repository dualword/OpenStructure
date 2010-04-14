//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2010 by the OpenStructure authors
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3.0 of the License, or (at your option)
// any later version.
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//------------------------------------------------------------------------------

/*
  Author: Ansgar Philippsen
*/

#include <map>
#include <vector>
#include <limits>

#include <ost/geom/geom.hh>
#include <ost/gfx/vertex_array.hh>

#include "calc_ambient.hh"

using namespace ost;
using namespace ost::gfx;

namespace {

  typedef IndexedVertexArray::EntryList EntryList;
  typedef IndexedVertexArray::IndexList IndexList;
  
  geom::Vec3 abs(const geom::Vec3& v) {
    geom::Vec3 nrvo(std::abs(v[0]),std::abs(v[1]),std::abs(v[2]));
    return nrvo;
  }

  std::pair<geom::Vec3, geom::Vec3> calc_limits(const EntryList& elist)
  {
    geom::Vec3 minc(std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max());
    geom::Vec3 maxc(-std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max());

    for(EntryList::const_iterator it=elist.begin();it!=elist.end();++it) {
#if OST_DOUBLE_PRECISION
      minc[0]=std::min(static_cast<double>(it->v[0]),minc[0]);
      minc[1]=std::min(static_cast<double>(it->v[1]),minc[1]);
      minc[2]=std::min(static_cast<double>(it->v[2]),minc[2]);
      maxc[0]=std::max(static_cast<double>(it->v[0]),maxc[0]);
      maxc[1]=std::max(static_cast<double>(it->v[1]),maxc[1]);
      maxc[2]=std::maxstatic_cast<double>((it->v[2]),maxc[2]);
#else
      minc[0]=std::min(it->v[0],minc[0]);
      minc[1]=std::min(it->v[1],minc[1]);
      minc[2]=std::min(it->v[2],minc[2]);
      maxc[0]=std::max(it->v[0],maxc[0]);
      maxc[1]=std::max(it->v[1],maxc[1]);
      maxc[2]=std::max(it->v[2],maxc[2]);
#endif
    }
    return std::make_pair(minc,maxc);
  }

  struct CIndex { 
    CIndex(): u(0),v(0),w(0) {}
    CIndex(long uu, long vv, long ww): u(uu), v(vv), w(ww) {}
    long u,v,w;
    bool operator<(const CIndex& rhs) const {
      return u==rhs.u ? (v==rhs.v ? w<rhs.w : v<rhs.v) : u<rhs.u;
    }
  };

  struct CEntry {
    CEntry(): id(0), type(0) {}
    CEntry(unsigned int ii, const geom::Vec3& vv1, const geom::Vec3& vv2, const geom::Vec3& vv3, const geom::Vec4& cc):
      id(ii), type(3), 
      v0(1.0f/3.0f*(vv1+vv2+vv3)), 
      v1(vv1), v2(vv2), v3(vv3), v4(), 
      n(geom::Normalize(geom::Cross(v1-v2,v1-v3))),
      c(cc) {}
    CEntry(unsigned int ii, const geom::Vec3& vv1, const geom::Vec3& vv2, const geom::Vec3& vv3, const geom::Vec3& vv4, const geom::Vec4& cc):
      id(ii), type(3), 
      v0(1.0f/4.0f*(vv1+vv2+vv3+vv4)), 
      v1(vv1), v2(vv2), v3(vv3), v4(vv4),
      n(geom::Normalize(geom::Cross(v1-v2,v1-v3))),
      c(cc) {}

    unsigned int id;   // original entry id
    unsigned int type; // type 
    geom::Vec3 v0;     // center position
    geom::Vec3 v1;     // edge positions
    geom::Vec3 v2;     // edge positions
    geom::Vec3 v3;     // edge positions
    geom::Vec3 v4;     // edge positions
    geom::Vec3 n;      // face normal
    geom::Vec4 c;      // average color
  };

  struct RayEntry {
    RayEntry(): v(), d2(0.0), c() {}
    RayEntry(const geom::Vec3& vv, float dd): v(vv), d2(dd), c() {}
    geom::Vec3 v;
    float d2;
    geom::Vec4 c;
    bool hit;
    bool back;
  };

  typedef std::vector<CEntry> CList;
  typedef std::map<CIndex,CList > CMap;

  class AmbientOcclusionBuilder {
  public:
    AmbientOcclusionBuilder(IndexedVertexArray& va):
      va_(va), cmap_(), bsize_(5.0), weight_(0.5), cutoff_(bsize_), rays_(), density_(20) 
    {
      float delta=2.0*M_PI/static_cast<float>(density_);
      for(unsigned int s=0;s<density_/2;++s) {
        float a=s*delta;
        float z=std::cos(a);
        float x=std::sin(a);
        unsigned int tmp_subdiv=static_cast<unsigned int>(round(2.0*M_PI*std::abs(x)/delta));
        if(tmp_subdiv==0) {
          rays_.push_back(RayEntry(geom::Vec3(0.0,0.0,z/std::abs(z)),cutoff_*cutoff_));
        } else {
          float tmp_delta=2.0*M_PI/static_cast<float>(tmp_subdiv);
          for(unsigned int t=0;t<tmp_subdiv;++t) {
            float b=t*tmp_delta;
            rays_.push_back(RayEntry(geom::Normalize(geom::Vec3(x*std::cos(b),x*std::sin(b),z)),cutoff_*cutoff_));
          }
        }
      }
    }

    CIndex coord_to_index(const geom::Vec3& v) 
    {
      return CIndex(static_cast<int>(floor(v[0]/bsize_)),
                    static_cast<int>(floor(v[1]/bsize_)),
                    static_cast<int>(floor(v[2]/bsize_)));
    }
    
    void build_cmap() {
      const EntryList& elist = va_.GetEntries();
      const IndexList& tlist = va_.GetTriIndices();
      const IndexList& qlist = va_.GetQuadIndices();

      cmap_.clear();
      float inv3 = 1.0f/3.0;
      for(unsigned int c=0;c<tlist.size();c+=3) {
        geom::Vec4 c0(elist[tlist[c+0]].c);
        geom::Vec4 c1(elist[tlist[c+1]].c);
        geom::Vec4 c2(elist[tlist[c+2]].c);
        CEntry ce = add_to_cmap(c,geom::Vec3(elist[tlist[c+0]].v),geom::Vec3(elist[tlist[c+1]].v),geom::Vec3(elist[tlist[c+2]].v),inv3*(c0+c1+c2));
        //std::cerr << c << " " << ce.n << " ";
        //std::cerr << geom::Vec3(elist[tlist[c+0]].n) << " ";
        //std::cerr << geom::Vec3(elist[tlist[c+1]].n) << " ";
        //std::cerr << geom::Vec3(elist[tlist[c+2]].n) << std::endl;
      }
      float inv4 = 1.0f/4.0;
      for(unsigned int c=0;c<qlist.size();c+=4) {
        geom::Vec4 c0(elist[qlist[c+0]].c);
        geom::Vec4 c1(elist[qlist[c+1]].c);
        geom::Vec4 c2(elist[qlist[c+2]].c);
        geom::Vec4 c3(elist[qlist[c+3]].c);
        add_to_cmap(c,geom::Vec3(elist[qlist[c+0]].v),geom::Vec3(elist[qlist[c+1]].v),geom::Vec3(elist[qlist[c+2]].v),geom::Vec3(elist[qlist[c+3]].v),inv4*(c0+c1+c2+c3));
      }
    }
    
    CEntry add_to_cmap(unsigned int id, const geom::Vec3& v1, const geom::Vec3& v2, const geom::Vec3& v3, const geom::Vec4& c) 
    {
      CEntry ce(id,v1,v2,v3,c);
      CIndex cindex=coord_to_index(ce.v0);
      CMap::iterator it=cmap_.find(cindex);
      if(it==cmap_.end()) {
        std::vector<CEntry> tmplist(1);
        tmplist[0]=ce;
        cmap_[cindex]=tmplist;
      } else {
        it->second.push_back(ce);
      }
      return ce;
    }

    CEntry add_to_cmap(unsigned int id, const geom::Vec3& v1, const geom::Vec3& v2, const geom::Vec3& v3, const geom::Vec3& v4, const geom::Vec4& c) 
    {
      CEntry ce(id,v1,v2,v3,v4,c);
      CIndex cindex=coord_to_index(ce.v0);
      CMap::iterator it=cmap_.find(cindex);
      if(it==cmap_.end()) {
        std::vector<CEntry> tmplist(1);
        tmplist[0]=ce;
        cmap_[cindex]=tmplist;
      } else {
        it->second.push_back(ce);
      }
      return ce;
    }

    void accumulate(const CIndex& cindex, const CEntry& ce)
    {
      float cutoff2=cutoff_*cutoff_;
      CMap::iterator mit=cmap_.find(cindex);
      if(mit==cmap_.end()) return;
      for(std::vector<CEntry>::const_iterator eit=mit->second.begin();eit!=mit->second.end();++eit) {
        //std::cerr << " comparing with v=" << eit->v0 << std::endl;
        geom::Vec3 dir0=(eit->v0-ce.v0); // vector from reference entry to current entry
        float l2=geom::Length2(dir0);
        if(l2>cutoff2 || l2<0.01) {
          //std::cerr << "  MISS: l2 failed: " << l2 << std::endl;
          continue;
        }
        dir0=geom::Normalize(dir0);

        //std::cerr << "  dir0=" << dir0 << " dot=" << geom::Dot(dir0,ce.n) << std::endl;

        if(geom::Dot(dir0,ce.n)<1e-6) { 
          //std::cerr << "  MISS: dir0 points to back" << std::endl;
          continue;
        }

        geom::Vec3 dir1=geom::Normalize(eit->v1-ce.v0); // vector from reference entry to corner 1
        geom::Vec3 dir2=geom::Normalize(eit->v2-ce.v0); // vector from reference entry to corner 2
        geom::Vec3 dir3=geom::Normalize(eit->v3-ce.v0); // vector from reference entry to corner 3
        geom::Vec3 dir4=geom::Normalize(eit->v4-ce.v0); // vector from reference entry to corner 4

        //std::cerr << "  v1=" << eit->v1 << std::endl;
        //std::cerr << "  v2=" << eit->v2 << std::endl;
        //std::cerr << "  v3=" << eit->v3 << std::endl;
        //std::cerr << "  v4=" << eit->v4 << std::endl;

        // largest opening angle from reference entry to corners
        float a0=1.0;
        if(eit->type==3) {
          a0 = (geom::Dot(dir0,dir1)+geom::Dot(dir0,dir2)+geom::Dot(dir0,dir3))/3.0;
        } else if(eit->type==4) {
          a0 = (geom::Dot(dir0,dir1)+geom::Dot(dir0,dir2)+geom::Dot(dir0,dir3)+geom::Dot(dir0,dir4))/4.0;
        }
        //std::cerr << "  a0=" << a0 << std::endl;
        for(std::vector<RayEntry>::iterator rit=rays_.begin();rit!=rays_.end();++rit) {
          //std::cerr << "   ray " << rit->v << std::endl;
          if(rit->back) {
            //std::cerr << "    MISS: points to back" << std::endl;
            continue;
          }
          if(geom::Dot(ce.n,rit->v)<1e-6) {
            rit->back=true;
            //std::cerr << "    MISS: points to back" << std::endl;
            continue;
          }
          //std::cerr << "    dot(dir0,ray)=" << geom::Dot(dir0,rit->v) << std::endl;
          if(geom::Dot(dir0,rit->v)<a0) {
            //std::cerr << "    MISS: outside" << std::endl;
            continue;
          }
          //std::cerr << "    HIT" << std::endl;
          if(rit->d2<l2) continue;
          rit->d2=l2;
          rit->c=eit->c;
          rit->hit=true;
        }
      }
    }

    void calc_all() {
      const EntryList& elist = va_.GetEntries();
      const IndexList& tlist = va_.GetTriIndices();
      const IndexList& qlist = va_.GetQuadIndices();
      float cutoff2=cutoff_*cutoff_;

      std::vector<std::pair<geom::Vec4, int> > entry_accum(elist.size());
      
      for(CMap::iterator mit=cmap_.begin();mit!=cmap_.end();++mit) {
        for(CList::iterator lit=mit->second.begin();lit!=mit->second.end();++lit) {
          //std::cerr << "working on v=" << lit->v0 << " n=" << lit->n << std::endl;
          //std::cerr << " v1=" << lit->v1 << std::endl;
          //std::cerr << " v2=" << lit->v2 << std::endl;
          //std::cerr << " v3=" << lit->v3 << std::endl;
          //std::cerr << " v4=" << lit->v4 << std::endl;
          // reset rays
          for(std::vector<RayEntry>::iterator rit=rays_.begin();rit!=rays_.end();++rit) {
            rit->d2=cutoff2;
            rit->hit=false;
            rit->back=false;
          }
          // visit all 27 quadrants
          for(int w=-1;w<=1;++w) {
            for(int v=-1;v<=1;++v) {
              for(int u=-1;u<=1;++u) {
                accumulate(CIndex(mit->first.u+u,mit->first.v+v,mit->first.w+w),*lit);
              }
            }
          }

          float kA=0.0;
          float kS=0.0;
          geom::Vec3 col(0.0,0.0,0.0);
          unsigned int miss_count=0;
          unsigned int back_count=0;
          unsigned int hit_count=0;
          for(std::vector<RayEntry>::iterator rit=rays_.begin();rit!=rays_.end();++rit) {
            if(!rit->back) {
              float ca=geom::Dot(rit->v,lit->n);
              kS += ca;
              if(!rit->hit) {
                kA += ca;
                col[0] += rit->c[0];
                col[1] += rit->c[1];
                col[2] += rit->c[2];
                ++miss_count;
              }
            }
            if(rit->back) ++back_count;
            if(rit->hit) ++hit_count;
          }
          float amb=kA/kS;
          if(miss_count>0) {
            col*=1.0f/static_cast<float>(miss_count);
          }
          //std::cerr << " hits=" << hit_count << ", miss=" << miss_count << ", back=" << back_count << " (" << rays_.size() << "), kA= " << kA << ", kS=" << kS << ", amb=" << amb << std::endl;

          if(lit->type==3) {
            // please provide a Vec explicit float ctor in double-precision mode
            // instead of typecasting here
            entry_accum[tlist[lit->id+0]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            entry_accum[tlist[lit->id+1]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            entry_accum[tlist[lit->id+2]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            ++entry_accum[tlist[lit->id+0]].second;
            ++entry_accum[tlist[lit->id+1]].second;
            ++entry_accum[tlist[lit->id+2]].second;
          } else if(lit->type==4) {
            entry_accum[qlist[lit->id+0]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            entry_accum[qlist[lit->id+1]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            entry_accum[qlist[lit->id+2]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            entry_accum[qlist[lit->id+3]].first+=geom::Vec4(col[0],col[1],col[2],amb);
            ++entry_accum[qlist[lit->id+0]].second;
            ++entry_accum[qlist[lit->id+1]].second;
            ++entry_accum[qlist[lit->id+2]].second;
            ++entry_accum[qlist[lit->id+3]].second;
          }
        }
      }
      for(unsigned int i=0;i<elist.size();++i) {
        float fact = entry_accum[i].second==0 ? 1.0f : 1.0f/static_cast<float>(entry_accum[i].second);
        va_.SetAmbientColor(i,Color(entry_accum[i].first[0]*fact,
                                    entry_accum[i].first[1]*fact,
                                    entry_accum[i].first[2]*fact,
                                    entry_accum[i].first[3]*fact));
      }
    }
    
  private:
    IndexedVertexArray& va_;
    CMap cmap_;
    float bsize_;
    float weight_;
    float cutoff_;
    std::vector<RayEntry> rays_;
    unsigned int density_;
  };
  
} // ns



void ost::gfx::CalcAmbientTerms(IndexedVertexArray& va)
{
  AmbientOcclusionBuilder aob(va);
  LOGN_VERBOSE("building component map");
  aob.build_cmap();
  LOGN_VERBOSE("calculating ambient terms");
  aob.calc_all();
}
