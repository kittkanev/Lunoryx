#pragma once
// Runtime-only state uses stable IDs. Editor Stop restores its existing snapshot.
struct ActorSpawn { Vec3 Position, Previous; uint64_t Checkpoint; ActorSpawn():Position{0,0,0},Previous{0,0,0},Checkpoint(0){} ActorSpawn(Vec3 p,Vec3 previous,uint64_t checkpoint):Position(p),Previous(previous),Checkpoint(checkpoint){} };
static std::map<uint64_t,ActorSpawn> g_ActorSpawns;
static unsigned g_DeathCount=0,g_CheckpointCount=0;
static void ResetGameplay() {
  g_ActorSpawns.clear();g_DeathCount=g_CheckpointCount=0;
  for(const auto& o:g_Objects)if(o.Dynamic){Vec3 p={o.Position[0],o.Position[1],o.Position[2]};g_ActorSpawns[o.Id]={p,p,0};}
}
static bool RespawnActor(GameObject& o) {
  auto it=g_ActorSpawns.find(o.Id);if(it==g_ActorSpawns.end())return false;
  const auto p=it->second.Position;o.Position[0]=p.x;o.Position[1]=p.y;o.Position[2]=p.z;
  std::fill(o.Velocity,o.Velocity+3,0);o.Grounded=false;it->second.Previous=p;return true;
}
// Sweep the actor's AABB center against an expanded trigger, avoiding tunnelling.
inline bool SweptTrigger(const GameObject& actor,Vec3 previous,const Bounds& trigger) {
  Bounds b;if(!WorldBounds(actor,b))return false;
  const double p[3]={(b.Min.x+b.Max.x)*.5,(b.Min.y+b.Max.y)*.5,(b.Min.z+b.Max.z)*.5};
  const double half[3]={(b.Max.x-b.Min.x)*.5,(b.Max.y-b.Min.y)*.5,(b.Max.z-b.Min.z)*.5};
  const double old[3]={previous.x,previous.y,previous.z},lo[3]={trigger.Min.x,trigger.Min.y,trigger.Min.z},hi[3]={trigger.Max.x,trigger.Max.y,trigger.Max.z};
  double enter=0,exit=1;
  for(int k=0;k<3;++k){double start=p[k]+old[k]-actor.Position[k],delta=p[k]-start;
    if(std::abs(delta)<1e-9){if(start<lo[k]-half[k]||start>hi[k]+half[k])return false;}
    else {double a=(lo[k]-half[k]-start)/delta,c=(hi[k]+half[k]-start)/delta;if(a>c)std::swap(a,c);enter=std::max(enter,a);exit=std::min(exit,c);if(enter>exit)return false;}}
  return true;
}
static void UpdateGameplay() {
  for(auto& actor:g_Objects)if(actor.Dynamic&&!actor.Trigger){
    auto it=g_ActorSpawns.find(actor.Id);
    if(it==g_ActorSpawns.end()){Vec3 p={actor.Position[0],actor.Position[1],actor.Position[2]};it=g_ActorSpawns.emplace(actor.Id,ActorSpawn{p,p,0}).first;}
    bool killed=false;
    // Kill takes priority over checkpoints when volumes overlap.
    for(const auto& zone:g_Objects)if(zone.Trigger==1){Bounds b;if(WorldBounds(zone,b)&&SweptTrigger(actor,it->second.Previous,b)){killed=true;break;}}
    if(actor.Position[1]<-50)killed=true;
    if(killed){RespawnActor(actor);++g_DeathCount;continue;}
    if(actor.Character)for(const auto& zone:g_Objects)if(zone.Trigger==2){Bounds b;if(WorldBounds(zone,b)&&SweptTrigger(actor,it->second.Previous,b)&&it->second.Checkpoint!=zone.Id){
      Bounds a;WorldBounds(actor,a);it->second.Position={zone.Position[0],b.Max.y+(actor.Position[1]-a.Min.y)+.05f,zone.Position[2]};it->second.Checkpoint=zone.Id;++g_CheckpointCount;}}
    it->second.Previous={actor.Position[0],actor.Position[1],actor.Position[2]};
  }
}
