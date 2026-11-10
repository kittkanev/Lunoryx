#pragma once
// Runtime-only state uses stable IDs. Editor Stop restores its existing snapshot.
struct ActorSpawn { Vec3 Position, Previous; uint64_t Checkpoint; ActorSpawn():Position{0,0,0},Previous{0,0,0},Checkpoint(0){} ActorSpawn(Vec3 p,Vec3 previous,uint64_t checkpoint):Position(p),Previous(previous),Checkpoint(checkpoint){} };
static std::map<uint64_t,ActorSpawn> g_ActorSpawns;
static unsigned g_DeathCount=0,g_CheckpointCount=0;
struct TriggerEvent { std::string Event;uint64_t Zone,Actor; };
static std::set<std::pair<uint64_t,uint64_t>> g_TriggerOverlaps;
static std::vector<TriggerEvent> g_TriggerEvents;
static bool TriggerContains(const GameObject& zone,const GameObject& actor) {
  Bounds a,b;if(!zone.Trigger||!WorldBounds(zone,a)||!WorldBounds(actor,b))return false;
  return a.Min.x<=b.Max.x&&a.Max.x>=b.Min.x&&a.Min.y<=b.Max.y&&a.Max.y>=b.Min.y&&a.Min.z<=b.Max.z&&a.Max.z>=b.Min.z;
}
inline bool SweptTrigger(const GameObject&,Vec3,const Bounds&);
static bool g_TriggerBudgetExceeded=false;
static size_t g_TriggerPairBudget=32768;
static bool ValidTriggerBudget(){size_t actors=0,zones=0;for(const auto& object:g_Objects){actors+=object.Dynamic&&!object.Trigger;zones+=object.Trigger!=0;}return !actors||zones<=g_TriggerPairBudget/actors;}
static bool CollectTriggerEvents() {
  g_TriggerEvents.clear();if(!ValidTriggerBudget()){g_TriggerBudgetExceeded=true;return false;}g_TriggerBudgetExceeded=false;
  std::set<std::pair<uint64_t,uint64_t>> current;BoundsSpatialIndex zones(g_Objects.size());for(size_t i=0;i<g_Objects.size();++i)if(g_Objects[i].Trigger){Bounds b;if(WorldBounds(g_Objects[i],b))zones.Update(i,b);}
  auto emit=[](const char* event,uint64_t zone,uint64_t actor){g_TriggerEvents.push_back({event,zone,actor});};
  for(const auto& actor:g_Objects)if(actor.Dynamic&&!actor.Trigger){auto it=g_ActorSpawns.find(actor.Id);Vec3 previous=it==g_ActorSpawns.end()?Vec3{actor.Position[0],actor.Position[1],actor.Position[2]}:it->second.Previous;Bounds now;if(!WorldBounds(actor,now))continue;Bounds old=now;old.Min.x+=previous.x-actor.Position[0];old.Max.x+=previous.x-actor.Position[0];old.Min.y+=previous.y-actor.Position[1];old.Max.y+=previous.y-actor.Position[1];old.Min.z+=previous.z-actor.Position[2];old.Max.z+=previous.z-actor.Position[2];
    for(auto index:zones.Query(UnionBounds(old,now))){const auto& zone=g_Objects[index];auto pair=std::make_pair(zone.Id,actor.Id);bool was=g_TriggerOverlaps.count(pair)!=0,inside=TriggerContains(zone,actor);if(inside){current.insert(pair);emit(was?"stay":"enter",zone.Id,actor.Id);}else if(!was){Bounds b;if(WorldBounds(zone,b)&&SweptTrigger(actor,previous,b)){emit("enter",zone.Id,actor.Id);emit("exit",zone.Id,actor.Id);}}}
  }
  for(const auto& pair:g_TriggerOverlaps)if(!current.count(pair))emit("exit",pair.first,pair.second);
  g_TriggerOverlaps.swap(current);return true;
}
static void ResetGameplay() {
  g_TriggerOverlaps.clear();g_TriggerEvents.clear();
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
  if(!CollectTriggerEvents())return;
  std::map<uint64_t,size_t> zoneIndices;for(size_t i=0;i<g_Objects.size();++i)if(g_Objects[i].Trigger)zoneIndices[g_Objects[i].Id]=i;
  std::map<uint64_t,std::set<size_t>> touched;for(const auto& event:g_TriggerEvents){auto found=zoneIndices.find(event.Zone);if(found!=zoneIndices.end())touched[event.Actor].insert(found->second);}
  for(auto& actor:g_Objects)if(actor.Dynamic&&!actor.Trigger){
    auto it=g_ActorSpawns.find(actor.Id);
    if(it==g_ActorSpawns.end()){Vec3 p={actor.Position[0],actor.Position[1],actor.Position[2]};it=g_ActorSpawns.emplace(actor.Id,ActorSpawn{p,p,0}).first;}
    bool killed=false;
    // Kill takes priority over checkpoints when volumes overlap.
    for(auto index:touched[actor.Id])if(g_Objects[index].Trigger==1){const auto& zone=g_Objects[index];Bounds b;if(WorldBounds(zone,b)&&SweptTrigger(actor,it->second.Previous,b)){killed=true;break;}}
    if(actor.Position[1]<-50)killed=true;
    if(killed){RespawnActor(actor);++g_DeathCount;continue;}
    if(actor.Character)for(auto index:touched[actor.Id])if(g_Objects[index].Trigger==2){const auto& zone=g_Objects[index];Bounds b;if(WorldBounds(zone,b)&&SweptTrigger(actor,it->second.Previous,b)&&it->second.Checkpoint!=zone.Id){
      Bounds a;WorldBounds(actor,a);it->second.Position={zone.Position[0],b.Max.y+(actor.Position[1]-a.Min.y)+.05f,zone.Position[2]};it->second.Checkpoint=zone.Id;++g_CheckpointCount;}}
    it->second.Previous={actor.Position[0],actor.Position[1],actor.Position[2]};
  }
}
