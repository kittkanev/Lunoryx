#pragma once
#include <array>
#include <set>
#include <map>
struct BoundsSpatialIndex {
  using Key=std::array<int64_t,3>;
  std::map<Key,std::set<size_t>> Cells;std::vector<std::vector<Key>> Membership;
  std::set<size_t> Large,Active;
  explicit BoundsSpatialIndex(size_t count):Membership(count){}
  static bool Keys(const Bounds& b,std::vector<Key>& keys){int64_t lo[3],hi[3];const float mn[3]={b.Min.x,b.Min.y,b.Min.z},mx[3]={b.Max.x,b.Max.y,b.Max.z};uint64_t count=1;for(int k=0;k<3;++k){if(!std::isfinite(mn[k])||!std::isfinite(mx[k])||std::abs(double(mn[k]))>1e15||std::abs(double(mx[k]))>1e15)return false;lo[k]=int64_t(std::floor(double(mn[k])/4));hi[k]=int64_t(std::floor(double(mx[k])/4));uint64_t span=uint64_t(hi[k]-lo[k]+1);if(hi[k]<lo[k]||!span||span>512||count>512/span)return false;count*=span;}for(auto x=lo[0];x<=hi[0];++x)for(auto y=lo[1];y<=hi[1];++y)for(auto z=lo[2];z<=hi[2];++z)keys.push_back({{x,y,z}});return true;}
  void Update(size_t id,const Bounds& b){for(const auto& key:Membership[id]){auto it=Cells.find(key);if(it!=Cells.end()){it->second.erase(id);if(it->second.empty())Cells.erase(it);}}Membership[id].clear();Large.erase(id);Active.insert(id);auto& keys=Membership[id];if(!Keys(b,keys)){Large.insert(id);return;}for(const auto& key:keys)Cells[key].insert(id);}
  std::set<size_t> Query(const Bounds& b)const{std::vector<Key> keys;if(!Keys(b,keys))return Active;auto result=Large;for(const auto& key:keys){auto it=Cells.find(key);if(it!=Cells.end())result.insert(it->second.begin(),it->second.end());}return result;}
};
inline Bounds UnionBounds(const Bounds& a,const Bounds& b){return {{std::min(a.Min.x,b.Min.x),std::min(a.Min.y,b.Min.y),std::min(a.Min.z,b.Min.z)},{std::max(a.Max.x,b.Max.x),std::max(a.Max.y,b.Max.y),std::max(a.Max.z,b.Max.z)}};}
struct PhysicsMetrics {size_t CandidatePairs=0,Contacts=0,ContinuousHits=0;};
inline PhysicsMetrics& LastPhysicsMetrics(){static thread_local PhysicsMetrics metrics;return metrics;}
