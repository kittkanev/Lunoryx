#pragma once
// Frozen bounds/physics from Lunoryx 1.0.1 (216242f), used only for differential tests.
inline bool ReferenceWorldBounds(const GameObject& o, Bounds& b) {
    Vec3 lo={-.5f,-.5f,-.5f}, hi={.5f,.5f,.5f};
    if (o.Type==MeshType::Plane && o.Vertices.empty()) lo.y=hi.y=0;
    if (!o.Vertices.empty()) {
        lo=hi={o.Vertices[0],o.Vertices[1],o.Vertices[2]};
        for (size_t i=3;i<o.Vertices.size();i+=3) {
            lo.x=std::min(lo.x,o.Vertices[i]); hi.x=std::max(hi.x,o.Vertices[i]);
            lo.y=std::min(lo.y,o.Vertices[i+1]); hi.y=std::max(hi.y,o.Vertices[i+1]);
            lo.z=std::min(lo.z,o.Vertices[i+2]); hi.z=std::max(hi.z,o.Vertices[i+2]);
        }
    }
    if(o.Type==MeshType::Plane && hi.y-lo.y<.01f){lo.y-=.005f;hi.y+=.005f;}
    for(int corner=0;corner<8;++corner) {
        Vec3 p;
        if(!TransformPoint(o,{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z},p)) return false;
        if(corner==0) b.Min=b.Max=p;
        else {
            b.Min.x=std::min(b.Min.x,p.x); b.Max.x=std::max(b.Max.x,p.x);
            b.Min.y=std::min(b.Min.y,p.y); b.Max.y=std::max(b.Max.y,p.y);
            b.Min.z=std::min(b.Min.z,p.z); b.Max.z=std::max(b.Max.z,p.z);
        }
    }
    return true;
}

inline void ReferencePhysicsStep(std::vector<GameObject>& objects,float dt,const PhysicsState& state) {
    for(auto& o:objects) if(o.Dynamic) {
        o.Grounded=false;
        o.Velocity[1]+=state.Gravity*dt;
        for(int k=0;k<3;++k) o.Position[k]=float(std::max(-1000000.0,std::min(1000000.0,double(o.Position[k])+double(o.Velocity[k])*dt)));
        Bounds b;if(!ReferenceWorldBounds(o,b))continue;
        if(state.Ground&&b.Min.y<0) {
            o.Position[1]-=b.Min.y;
            if(o.Velocity[1]<0)o.Velocity[1]=-o.Velocity[1]*o.Restitution;
            o.Grounded=true;
            float drag=std::max(0.0f,1-o.Friction*dt);o.Velocity[0]*=drag;o.Velocity[2]*=drag;
        }
    }
    for(size_t i=0;i<objects.size();++i)for(size_t j=i+1;j<objects.size();++j) {
        auto& a=objects[i];auto& b=objects[j];if(!a.Dynamic&&!b.Dynamic)continue;
        Bounds ba,bb;if(!ReferenceWorldBounds(a,ba)||!ReferenceWorldBounds(b,bb))continue;
        float overlap[3]={std::min(ba.Max.x,bb.Max.x)-std::max(ba.Min.x,bb.Min.x),std::min(ba.Max.y,bb.Max.y)-std::max(ba.Min.y,bb.Min.y),std::min(ba.Max.z,bb.Max.z)-std::max(ba.Min.z,bb.Min.z)};
        if(overlap[0]<=0||overlap[1]<=0||overlap[2]<=0)continue;
        int axis=overlap[1]<overlap[0]?1:0;if(overlap[2]<overlap[axis])axis=2;
        const float centersA[3]={ba.Min.x*.5f+ba.Max.x*.5f,ba.Min.y*.5f+ba.Max.y*.5f,ba.Min.z*.5f+ba.Max.z*.5f};
        const float centersB[3]={bb.Min.x*.5f+bb.Max.x*.5f,bb.Min.y*.5f+bb.Max.y*.5f,bb.Min.z*.5f+bb.Max.z*.5f};
        float sign=centersA[axis]<centersB[axis]?-1.0f:1.0f;
        float ia=a.Dynamic?1/std::max(.01f,a.Mass):0,ib=b.Dynamic?1/std::max(.01f,b.Mass):0,total=ia+ib;
        a.Position[axis]+=sign*overlap[axis]*ia/total;b.Position[axis]-=sign*overlap[axis]*ib/total;
        float relative=(a.Velocity[axis]-b.Velocity[axis])*sign;
        if(relative<0) {
            float impulse=-(1+std::min(a.Restitution,b.Restitution))*relative/total;
            a.Velocity[axis]+=impulse*sign*ia;b.Velocity[axis]-=impulse*sign*ib;
        }
        if(axis==1){if(a.Dynamic&&sign>0)a.Grounded=true;if(b.Dynamic&&sign<0)b.Grounded=true;}
    }
}
