#pragma once
static void CheckEasingIntegration(){
  StopSimulation();g_ProjectDirectory.clear();ScriptAssetRoot().clear();g_OpenedProject.clear();g_Objects.clear();g_Sounds.clear();g_Game=StudioGameSettings();
  for(auto& script:StudioScripts())script=ScriptDocument();
  InitLuaState();g_Objects.emplace_back("Curve cube");auto& object=g_Objects[0];object.AnimationDuration=2;
  assert(CaptureAnimationKey(object,0,LNX_EASE_SINE_IN_OUT));object.Position[0]=8;assert(CaptureAnimationKey(object,2,LNX_EASE_LINEAR));
  SampleObjectAnimation(object,.5,false);assert(std::abs(object.Position[0]-float(8*.14644660940672624))<1e-6f);
  CheckLua("assert(animation_key(0,0,9));assert(not animation_key(0,0,10));assert(not animation_key(0,0,-1));assert(not animation_key(0,0,9223372036854775807));assert(not pcall(animation_key,0,0/0,9))");
  g_ObjectHistory.Clear();g_ObjectHistory.Capture(g_Objects);g_Objects[0].AnimationKeys[0].Easing=LNX_EASE_CUBIC_IN;g_ObjectHistory.Capture(g_Objects);
  assert(g_ObjectHistory.Undo(g_Objects)&&g_Objects[0].AnimationKeys[0].Easing==LNX_EASE_SINE_IN_OUT);
  assert(g_ObjectHistory.Redo(g_Objects)&&g_Objects[0].AnimationKeys[0].Easing==LNX_EASE_CUBIC_IN);
  auto folder=ProjectFS::temp_directory_path()/ProjectFS::path("lunoryx-curves-"+std::to_string(std::random_device{}()));ProjectFS::create_directory(folder);
  auto path=(folder/"curves.lunoryx").u8string();assert(SaveStudioProject(path));g_Objects.clear();assert(LoadStudioProject(path));assert(g_Objects[0].AnimationKeys[0].Easing==LNX_EASE_CUBIC_IN);
  auto invalid=StudioProjectJson(path);invalid["schema"]=3;invalid["format_version"]=3;auto text=invalid.dump();auto old=(folder/"old.lunoryx").u8string();WriteAssetFileAtomic(old,text.data(),text.size());
  auto hash=CompleteDocumentHash();assert(!LoadStudioProject(old)&&CompleteDocumentHash()==hash);
  for(auto& key:invalid["objects"][0]["animation"]["keys"])key["easing"]=1;
  text=invalid.dump();WriteAssetFileAtomic(old,text.data(),text.size());assert(LoadStudioProject(old)&&g_Objects[0].AnimationKeys[0].Easing==1);
  auto& restored=g_Objects[0];for(int curve=0;curve<LNX_EASE_COUNT;++curve){assert(CaptureAnimationKey(restored,0,curve));assert(ValidAnimation(restored));}
  assert(!CaptureAnimationKey(restored,0,LNX_EASE_COUNT));
  ShutdownLuaState();g_OpenedProject.clear();g_ProjectDirectory.clear();ScriptAssetRoot().clear();
  {std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());ConfinedAssetPolicies().clear();AssetRootAnchors().clear();}
  ProjectFS::remove_all(folder);
  std::cout<<"PASS: easing Lua bounds, schema 4 persistence, legacy schema validation and undo/redo\n";
}
