#include "../scene_tools.h"
#include "../mesh_modeling.h"
#include <cassert>
#include <iostream>
static std::vector<uint32_t> Reference(const std::vector<float>& p,const std::vector<uint32_t>& selected,float t){std::map<uint32_t,uint32_t> map;std::vector<uint32_t> out;for(auto v:selected){map[v]=v;for(auto r:map)if(v!=r.first){Vec3 d=Subtract({p[v*3],p[v*3+1],p[v*3+2]},{p[r.first*3],p[r.first*3+1],p[r.first*3+2]});if(Dot(d,d)<=double(t)*t){map[v]=r.second;break;}}out.push_back(map[v]);}return out;}
int main(){
 static_assert(sizeof(LunoryxWeldCell)==32&&alignof(LunoryxWeldCell)==8,"Rust ABI layout");
 uint32_t seed=73;
 for(int run=0;run<80;++run){std::vector<float> p;std::vector<uint32_t> selection;for(uint32_t i=0;i<600;++i){for(int k=0;k<3;++k){seed=seed*1664525u+1013904223u;p.push_back(float(int(seed%200)-100)*.01f);}selection.push_back(i);}float t=run%2?.011f:.03f;auto expected=Reference(p,selection,t);std::vector<uint32_t> out(600);std::vector<LunoryxWeldCell> workspace(600);assert(!lunoryx_weld_map(p.data(),600,selection.data(),600,t,workspace.data(),out.data()));assert(expected==out);}
 // Full C++ operation, material/topology preservation, large and small paths.
 for(int count:{12,1024}){StudioMesh m;std::set<uint32_t> selected;for(int i=0;i<count;++i){m.Positions.insert(m.Positions.end(),{float(i%512),float(i/512)*.0001f,0});selected.insert(uint32_t(i));}for(int i=0;i<510;i+=3){if(i+2<count)m.Faces.push_back({uint32_t(i),uint32_t(i+1),uint32_t(i+2)});}m.FaceMaterials.assign(m.Faces.size(),2);Model::Triangulate(m);Model::Weld(m,selected,.001f);Model::Check(m);for(auto slot:m.FaceMaterials)assert(slot==2);assert(m.Positions.size()/3<=512);}
 uint32_t out[2]={99,99},selection[2]={0,1};LunoryxWeldCell cells[2];float p[6]={0,0,0,1,0,0};assert(lunoryx_weld_map(p,2,selection,2,std::numeric_limits<float>::min(),cells,out)==2&&out[0]==99);assert(lunoryx_weld_map(nullptr,2,selection,2,1,cells,out)==1);
 std::cout<<"PASS: Rust/C ABI differential weld mapping, boundaries, numeric fallback and full mesh operation\n";
}
