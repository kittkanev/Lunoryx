#include "../scene_tools.h"
#include "../mesh_modeling.h"
#include "../tests/reference_weld.h"
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
static volatile size_t sink=0;
static StudioMesh Fixture(unsigned side) {
    StudioMesh mesh;
    for(unsigned layer=0;layer<2;++layer) {
        unsigned base=unsigned(mesh.Positions.size()/3);
        for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x) {
            mesh.Positions.insert(mesh.Positions.end(),{float(x),0,float(y)});
            mesh.Colors.insert(mesh.Colors.end(),{float(x)/side,float(y)/side,.5f});
            mesh.UV.insert(mesh.UV.end(),{float(x)/(side-1),float(y)/(side-1)});
        }
        for(unsigned y=0;y+1<side;++y)for(unsigned x=0;x+1<side;++x) {
            unsigned a=base+y*side+x;
            mesh.Faces.push_back({a,a+side,a+side+1,a+1});
            mesh.FaceMaterials.push_back((x+y)%2);
        }
    }
    Model::Triangulate(mesh);Model::Check(mesh);return mesh;
}
static void Same(const StudioMesh& a,const StudioMesh& b) {
    assert(a.Positions==b.Positions&&a.Indices==b.Indices&&a.Faces==b.Faces);
    assert(a.Colors==b.Colors&&a.UV==b.UV&&a.Normals==b.Normals);
    assert(a.FaceMaterials==b.FaceMaterials&&a.Seams==b.Seams);
}
static double Measure(const StudioMesh& source,const std::set<uint32_t>& selected,bool fast) {
    std::vector<double> samples;
    for(int sample=0;sample<5;++sample) {
        auto start=std::chrono::steady_clock::now();
        // Copy and all allocation/FFI/Compact/Triangulate work are inside time.
        auto mesh=source;
        if(fast)Model::Weld(mesh,selected,.00001f);
        else Model::ReferenceWeld(mesh,selected,.00001f);
        sink=mesh.Positions.size();
        auto end=std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double,std::milli>(end-start).count());
    }
    std::sort(samples.begin(),samples.end());return samples[2];
}
int main() {
    std::cout<<"scenario,implementation,selected_vertices,median_ms\n";
    for(unsigned side:{16u,32u,64u}) {
        auto source=Fixture(side);std::set<uint32_t> selected;
        for(uint32_t v=0;v<source.Positions.size()/3;++v)selected.insert(v);
        auto reference=source,optimized=source;
        Model::ReferenceWeld(reference,selected,.00001f);Model::Weld(optimized,selected,.00001f);Same(reference,optimized);
        assert(optimized.Positions.size()/3==side*side);
        for(bool fast:{false,true})std::cout<<"two_layers_"<<side<<"x"<<side<<","<<(fast?"adaptive_weld":"cpp_1.0.1")<<","<<selected.size()<<","<<std::fixed<<std::setprecision(6)<<Measure(source,selected,fast)<<"\n";
    }
}
