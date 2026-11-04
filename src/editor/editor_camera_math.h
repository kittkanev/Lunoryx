#pragma once
#include "src/core/scene_tools.h"
inline Vec3 StudioCameraVector(Vec3 v,float pitch,float yaw){
  const float toRad=-PI/180;float a=pitch*toRad,c=std::cos(a),s=std::sin(a),y=c*v.y-s*v.z;
  v.z=s*v.y+c*v.z;v.y=y;a=yaw*toRad;c=std::cos(a);s=std::sin(a);float x=c*v.x+s*v.z;v.z=-s*v.x+c*v.z;v.x=x;return v;
}
inline Vec3 StudioCameraEye(float distance,float pitch,float yaw,const float target[3]){
  auto p=StudioCameraVector({0,0,distance},pitch,yaw);return {p.x+target[0],p.y+target[1],p.z+target[2]};
}
inline void StudioFreeLook(float distance,float& pitch,float& yaw,float target[3],float dx,float dy){
  Vec3 eye=StudioCameraEye(distance,pitch,yaw,target);
  pitch=std::max(-89.f,std::min(89.f,pitch+dy));yaw=WrapYaw(yaw+dx);
  Vec3 offset=StudioCameraVector({0,0,distance},pitch,yaw);target[0]=eye.x-offset.x;target[1]=eye.y-offset.y;target[2]=eye.z-offset.z;
}
inline void StudioFly(float target[3],float pitch,float yaw,Vec3 input,float speed,float dt){
  if(!FiniteVec(input)||!std::isfinite(speed)||speed<=0||!std::isfinite(dt)||dt<0)return;
  auto direction=StudioCameraVector({input.x,0,input.z},pitch,yaw);direction.y+=input.y;
  float n=float(std::sqrt(Dot(direction,direction)));if(n<.0001f)return;
  direction.x/=n;direction.y/=n;direction.z/=n;
  float step=speed*std::min(dt,.1f);float p[3]={target[0]+direction.x*step,target[1]+direction.y*step,target[2]+direction.z*step};
  for(float v:p)if(!std::isfinite(v)||std::abs(v)>1000000)return;
  std::copy(p,p+3,target);
}
