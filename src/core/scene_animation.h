#pragma once
#include "src/core/scene_tools.h"
inline LnxPose ObjectPose(const GameObject& o) {
  LnxPose p;
  std::copy(o.Position,o.Position+3,p.Position);std::copy(o.Rotation,o.Rotation+3,p.Rotation);
  std::copy(o.Scale,o.Scale+3,p.Scale);std::copy(o.Color,o.Color+3,p.Color);return p;
}
inline void ApplyPose(GameObject& o,const LnxPose& p) {
  std::copy(p.Position,p.Position+3,o.Position);std::copy(p.Rotation,p.Rotation+3,o.Rotation);
  std::copy(p.Scale,p.Scale+3,o.Scale);std::copy(p.Color,p.Color+3,o.Color);
}
inline bool ValidAnimation(const GameObject& o) {
  if(!std::isfinite(o.AnimationDuration)||o.AnimationDuration<.01f||o.AnimationDuration>3600||
     !std::isfinite(o.AnimationSpeed)||o.AnimationSpeed<.01f||o.AnimationSpeed>100||o.AnimationKeys.size()>256)return false;
  float previous=-1;
  for(const auto& key:o.AnimationKeys){
    if(!std::isfinite(key.Time)||key.Time<0||key.Time>o.AnimationDuration||key.Time<=previous||key.Easing<0||key.Easing>2)return false;
    previous=key.Time;
    for(int k=0;k<3;++k)if(!std::isfinite(key.Pose.Position[k])||std::abs(key.Pose.Position[k])>1000000||
      !std::isfinite(key.Pose.Rotation[k])||std::abs(key.Pose.Rotation[k])>36000||
      !std::isfinite(key.Pose.Scale[k])||key.Pose.Scale[k]<.001f||key.Pose.Scale[k]>10000||
      !std::isfinite(key.Pose.Color[k])||key.Pose.Color[k]<0||key.Pose.Color[k]>1)return false;
  }return true;
}
inline bool CaptureAnimationKey(GameObject& o,float time,int easing) {
  if(!std::isfinite(time)||time<0||time>o.AnimationDuration||easing<0||easing>2)return false;
  LnxAnimationKey key={time,ObjectPose(o),easing};
  auto it=std::lower_bound(o.AnimationKeys.begin(),o.AnimationKeys.end(),time,[](const LnxAnimationKey& k,float t){return k.Time<t;});
  auto copy=o.AnimationKeys;
  size_t at=size_t(it-o.AnimationKeys.begin());
  if(it!=o.AnimationKeys.end()&&std::abs(it->Time-time)<.0001f)o.AnimationKeys[at]=key;
  else if(o.AnimationKeys.size()<256)o.AnimationKeys.insert(it,key);else return false;
  if(!ValidAnimation(o)){o.AnimationKeys.swap(copy);return false;}return true;
}
inline void SampleObjectAnimation(GameObject& o,double time,bool wrap=true) {
  LnxPose p;double t=wrap?lnx_animation_time(time,o.AnimationDuration,o.AnimationLoop):time;
  if(lnx_animation_sample(o.AnimationKeys.data(),o.AnimationKeys.size(),t,&p))ApplyPose(o,p);
}
struct AnimationPlayer { double Time=0; bool Playing=false; };
static std::map<uint64_t,AnimationPlayer> g_AnimationPlayers;
static void ResetAnimationPlayers(){g_AnimationPlayers.clear();}
static void AdvanceAnimations(float dt) {
  for(auto& o:g_Objects){
    auto it=g_AnimationPlayers.find(o.Id);
    if(it==g_AnimationPlayers.end()&&!o.AnimationAutoplay)continue;
    if(it==g_AnimationPlayers.end()){AnimationPlayer p;p.Playing=true;it=g_AnimationPlayers.emplace(o.Id,p).first;}
    auto& p=it->second;if(!p.Playing||o.AnimationKeys.empty())continue;
    p.Time+=double(dt)*o.AnimationSpeed;SampleObjectAnimation(o,p.Time);
    if(!o.AnimationLoop&&p.Time>=o.AnimationDuration)p.Playing=false;
  }
}
