#include "../scene_tools.h"
#include "physics_reference.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>

static void Same(const std::vector<GameObject>& a,const std::vector<GameObject>& b) {
    assert(a.size()==b.size());
    for(size_t i=0;i<a.size();++i) {
        assert(!std::memcmp(a[i].Position,b[i].Position,sizeof(a[i].Position)));
        assert(!std::memcmp(a[i].Velocity,b[i].Velocity,sizeof(a[i].Velocity)));
        assert(a[i].Grounded==b[i].Grounded);
    }
}
int main() {
    std::mt19937 random(0x1aab123u);
    auto number=[&](float low,float high){return low+(high-low)*(random()%100001)/100000.f;};
    size_t steps=0;
    for(int fixture=0;fixture<120;++fixture) {
        std::vector<GameObject> reference;
        for(int i=0;i<18;++i) {
            reference.emplace_back("Fixture",MeshType(i%5));auto& o=reference.back();
            for(int k=0;k<3;++k) {
                o.Position[k]=number(-2,2);o.Rotation[k]=number(-180,180);
                o.Scale[k]=number(.1f,2)*(random()%2?-1:1);o.Velocity[k]=number(-3,3);
            }
            o.Dynamic=random()%4!=0;o.Grounded=true;o.Mass=number(.1f,10);
            o.Restitution=number(0,1);o.Friction=number(0,10);
            if(i%3==0) {
                std::vector<uint32_t> indices;ObjectMesh(o,o.Vertices,indices);o.Indices=indices;
            }
        }
        if(fixture%4==0)for(auto& o:reference)o.Dynamic=false;
        auto optimized=reference;
        PhysicsState state;state.Gravity=number(-15,0);state.Ground=fixture%3!=0;
        for(int step=0;step<32;++step) {
            if(step==10) {
                // Cache scope is one step: edits, teleportation and dynamic flag
                // changes between steps must be visible immediately.
                for(auto* scene:{&reference,&optimized}) {
                    (*scene)[1].Position[0]+=.3f;(*scene)[2].Dynamic=!(*scene)[2].Dynamic;
                    (*scene)[3].Scale[1]*=2;(*scene)[4].Rotation[0]+=42;
                    if(!(*scene)[0].Vertices.empty())(*scene)[0].Vertices[0]+=.2f;
                }
            }
            ReferencePhysicsStep(reference,1.f/120,state);
            PhysicsStep(optimized,1.f/120,state);Same(reference,optimized);++steps;
        }
    }
    for(int i=0;i<2000;++i) {
        GameObject o("Bounds",MeshType(i%5));
        for(int k=0;k<3;++k){o.Position[k]=number(-100,100);o.Rotation[k]=number(-720,720);o.Scale[k]=number(-5,5);}
        if(i%2==0){std::vector<uint32_t> indices;ObjectMesh(o,o.Vertices,indices);o.Indices=indices;}
        Bounds reference,optimized;
        assert(ReferenceWorldBounds(o,reference)==WorldBounds(o,optimized));
        assert(!std::memcmp(&reference,&optimized,sizeof(Bounds)));
    }
    for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        GameObject o("Invalid");o.Rotation[0]=bad;Bounds a,b;
        assert(!ReferenceWorldBounds(o,a)&&!WorldBounds(o,b));
        o.Rotation[0]=0;o.Scale[2]=bad;
        assert(!ReferenceWorldBounds(o,a)&&!WorldBounds(o,b));
    }
    for(float value:{-0.f,0.f,std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::max()}) {
        GameObject o("Numeric bounds");o.Vertices={-value,0,0,value,0,0};
        for(float scale:{-2.f,0.f,1.f,2.f}) {
            o.Scale[0]=scale;Bounds reference,optimized;
            bool expected=ReferenceWorldBounds(o,reference),actual=WorldBounds(o,optimized);
            assert(expected==actual);
            if(expected)assert(!std::memcmp(&reference,&optimized,sizeof(Bounds)));
        }
    }
    for(size_t size:{size_t(1),size_t(2),size_t(4),MAX_EDIT_VERTICES*3+3}) {
        GameObject invalid("Malformed geometry");invalid.Vertices.assign(size,0);Bounds bounds;
        assert(!WorldBounds(invalid,bounds));
        invalid.Dynamic=true;std::vector<GameObject> scene={invalid};PhysicsState state;
        PhysicsStep(scene,1.f/120,state); // Must reject malformed bounds without reading past the buffer.
    }
    std::cout<<"PASS: "<<steps<<" physics steps and 2000 bounds fixtures match 1.0.1 bit-for-bit\n";
}
