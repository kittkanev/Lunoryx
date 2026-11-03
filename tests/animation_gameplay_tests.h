#pragma once
static void CheckAnimationGameplay(){
  g_Objects.clear();g_Game.StartupScript=-1;g_Sounds.clear();
  CharacterConfig character;CreateCharacter(g_Objects,"Flat character",character);assert(g_Objects[0].Normals.empty());
  assert(SaveStudioProject("tests/flat-character.lunoryx"));assert(LoadStudioProject("tests/flat-character.lunoryx")&&g_Objects[0].Normals.empty());g_Objects.clear();
  GameObject animated("Animated");animated.Position[0]=2;assert(CaptureAnimationKey(animated,0,0));animated.Position[0]=8;assert(CaptureAnimationKey(animated,2,1));
  animated.AnimationAutoplay=true;g_Objects.push_back(animated);auto id=animated.Id;
  ResetAnimationPlayers();AdvanceAnimations(1);assert(g_Objects[0].Position[0]==5);
  assert(!CaptureAnimationKey(g_Objects[0],-1,0));
  assert(SaveStudioProject("tests/animated.lunoryx"));auto valid=StudioProjectJson("tests/animated.lunoryx");
  assert(LoadStudioProject("tests/animated.lunoryx")&&g_Objects[0].Id==id&&g_Objects[0].AnimationKeys.size()==2);
  auto invalid=valid;invalid["objects"][0]["animation"]["keys"][1]["time"]=0;
  auto data=invalid.dump();WriteAssetFile("tests/invalid-animation.lunoryx",data.data(),data.size());auto hash=CompleteDocumentHash();assert(!LoadStudioProject("tests/invalid-animation.lunoryx")&&CompleteDocumentHash()==hash);
  invalid=valid;invalid["objects"][0].erase("animation");invalid["objects"][0].erase("solid");invalid["objects"][0].erase("trigger");data=invalid.dump();WriteAssetFile("tests/legacy-animation.lunoryx",data.data(),data.size());assert(LoadStudioProject("tests/legacy-animation.lunoryx")&&g_Objects[0].AnimationKeys.empty()&&g_Objects[0].Solid);
  g_Objects.clear();GameObject player("Actor");player.Dynamic=true;player.Character=true;player.Position[0]=-3;player.Position[1]=1;g_Objects.push_back(player);
  GameObject zone("Killzone");zone.Trigger=1;zone.Position[1]=1;g_Objects.push_back(zone);ResetGameplay();
  // A complete crossing in one frame must trigger even without final overlap.
  g_Objects[0].Position[0]=3;UpdateGameplay();assert(g_DeathCount==1&&g_Objects[0].Position[0]==-3&&g_Objects[0].Velocity[0]==0);
  g_Objects[1].Trigger=2;g_Objects[0].Position[0]=0;UpdateGameplay();assert(g_CheckpointCount==1);
  g_Objects[0].Position[0]=10;assert(RespawnActor(g_Objects[0])&&g_Objects[0].Position[0]==0);
  g_Objects[1].Trigger=1;g_Objects[0].Position[0]=0;g_Objects[0].Position[1]=1;g_Objects[0].Velocity[0]=0;
  PhysicsState physics;physics.Gravity=0;physics.Ground=false;PhysicsStep(g_Objects,.01f,physics);assert(g_Objects[0].Position[0]==0); // trigger never pushes an actor
  g_Objects[0].Solid=false;g_Objects[1].Trigger=0;PhysicsStep(g_Objects,.01f,physics);assert(g_Objects[0].Position[0]==0);
  g_Objects.clear();CheckLua("local i=add_object('LuaClip'); assert(animation_config(i,2,true,1,true)); assert(animation_key(i,0,0)); set_position(i,4,0,0); assert(animation_key(i,2,1)); assert(not animation_config(i,1,true)); assert(set_trigger(i,1)); assert(not set_trigger(i,3)); assert(set_solid(i,false)); assert(animation_play(i)); assert(animation_stop(i))");
  ExecuteLuaScript("local function nested() error('trace regression') end; function on_update() nested() end");g_SimulationRunning=true;UpdateLuaScript(.01f);assert(g_LastLuaError.find("stack traceback")!=std::string::npos&&g_LastLuaError.find("trace regression")!=std::string::npos);
  g_Game.StartupScript=-1;g_PhysicsRunning=false;auto initial=ObjectPose(g_Objects[0]);StartSimulation();AdvanceAnimations(.5f);assert(g_Objects[0].Position[0]!=initial.Position[0]);StopSimulation();assert(g_Objects[0].Position[0]==initial.Position[0]);
  g_ObjectHistory.Clear();g_ObjectHistory.Capture(g_Objects);g_Objects[0].Position[0]+=10;g_ObjectHistory.Capture(g_Objects);
  g_PreviewObject=g_Objects[0].Id;g_PreviewPose=ObjectPose(g_Objects[0]);g_AnimationPreview=true;g_Objects[0].Position[0]=999;
  assert(g_ObjectHistory.Undo(g_Objects)&&!g_AnimationPreview&&g_Objects[0].Position[0]!=999);
  assert(g_ObjectHistory.Redo(g_Objects)&&g_Objects[0].Position[0]==g_PreviewPose.Position[0]);
  std::cout<<"PASS: animation/project compatibility, Lua API, swept killzones, checkpoint respawn, non-solid triggers, traceback and Play restore\n";
}
