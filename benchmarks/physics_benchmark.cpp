#include "../scene_tools.h"
#include "../tests/physics_reference.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
static volatile float sink=0;
static void Geometry(GameObject& object,size_t vertices) {
    for(size_t i=0;i<vertices;++i)object.Vertices.insert(object.Vertices.end(),{
        float(int(i%23)-11)/22,float(int(i%17)-8)/16,float(int(i%31)-15)/30});
}
static double Measure(const std::vector<GameObject>& source,bool optimized) {
    std::vector<double> times;
    PhysicsState state;
    for(int sample=0;sample<5;++sample) {
        auto objects=source;
        auto start=std::chrono::steady_clock::now();
        for(int step=0;step<5;++step) {
            if(optimized)PhysicsStep(objects,1.f/120,state);
            else ReferencePhysicsStep(objects,1.f/120,state);
            sink=objects.back().Position[1];
        }
        auto end=std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double,std::milli>(end-start).count()/5);
    }
    std::sort(times.begin(),times.end());return times[2];
}
static void Scenario(const char* name,std::vector<GameObject> objects) {
    auto reference=objects,optimized=objects;PhysicsState state;
    for(int step=0;step<5;++step){ReferencePhysicsStep(reference,1.f/120,state);PhysicsStep(optimized,1.f/120,state);}
    for(size_t i=0;i<objects.size();++i){assert(!std::memcmp(reference[i].Position,optimized[i].Position,sizeof(reference[i].Position)));assert(!std::memcmp(reference[i].Velocity,optimized[i].Velocity,sizeof(reference[i].Velocity)));assert(reference[i].Grounded==optimized[i].Grounded);}
    for(bool fast:{false,true})std::cout<<name<<","<<(fast?"cpp_cached":"cpp_1.0.1")<<","<<objects.size()<<","<<std::fixed<<std::setprecision(6)<<Measure(objects,fast)<<"\n";
}
int main() {
    std::cout<<"scenario,implementation,objects,median_ms\n";
    std::vector<GameObject> props;
    for(int i=0;i<3000;++i){props.emplace_back("Prop");props.back().Position[0]=float(i%100)*2;props.back().Position[2]=float(i/100)*2;}
    props.emplace_back("Player");props.back().Dynamic=true;props.back().Position[1]=4;
    Scenario("3000_static_one_player",std::move(props));
    std::vector<GameObject> meshes;
    for(int i=0;i<100;++i){meshes.emplace_back("Mesh");Geometry(meshes.back(),5000);meshes.back().Position[0]=float(i)*3;meshes.back().Position[1]=4;meshes.back().Dynamic=i%25==0;}
    Scenario("100_meshes_5000_vertices",std::move(meshes));
    std::vector<GameObject> heavy;
    for(int i=0;i<48;++i){heavy.emplace_back("Platform");heavy.back().Position[0]=float(i)*2;}
    heavy.emplace_back("Dense player");Geometry(heavy.back(),200000);heavy.back().Dynamic=true;heavy.back().Position[1]=4;
    Scenario("200k_vertex_player_48_platforms",std::move(heavy));
    std::vector<GameObject> dense;
    for(int i=0;i<64;++i){dense.emplace_back("Body");dense.back().Dynamic=true;dense.back().Position[0]=float(i%4)*.9f;dense.back().Position[1]=float(i/16)+.5f;dense.back().Position[2]=float(i/4%4)*.9f;}
    Scenario("64_touching_dynamic_boxes",std::move(dense));
    std::vector<GameObject> sample;
    for(int i=0;i<12;++i){sample.emplace_back("Example");sample.back().Position[0]=float(i)*2;sample.back().Dynamic=i==11;sample.back().Position[1]=4;}
    Scenario("12_object_small_level",std::move(sample));
}
