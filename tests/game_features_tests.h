#pragma once
static void CheckGameBuildGUITriggers(){
  g_PhysicsRunning=false;g_SimulationRunning=false;g_DebugPaused=false;g_Objects.clear();g_Sounds.clear();g_Game=StudioGameSettings();
  GameWidget button;button.Id="start";button.Type=1;button.Text="Играть";g_Game.Widgets.push_back(button);
  g_Game.Title="Custom game / Игра";g_Game.WindowMode=1;g_Game.Width=1600;g_Game.Height=900;
  auto oldHash=CompleteDocumentHash();g_Game.Width=800;assert(CompleteDocumentHash()!=oldHash);g_Game.Width=1600;
  g_ObjectHistory.Clear();g_ObjectHistory.Capture(g_Objects);g_Game.Widgets[0].X=.6f;g_ObjectHistory.Capture(g_Objects);assert(g_ObjectHistory.Undo(g_Objects)&&g_Game.Widgets[0].X==.05f);assert(g_ObjectHistory.Redo(g_Objects)&&g_Game.Widgets[0].X==.6f);
  assert(SaveStudioProject("tests/game-features.lunoryx"));assert(LoadStudioProject("tests/game-features.lunoryx"));assert(g_Game.Title=="Custom game / Игра"&&g_Game.WindowMode==1&&g_Game.Width==1600&&g_Game.Widgets[0].Text=="Играть");
  auto document=StudioProjectJson("tests/game-features.lunoryx");
  for(int mode=0;mode<4;++mode){auto bad=document;if(mode==0)bad["game"]["width"]=2.5;else if(mode==1)bad["game"]["gui"][0]["value"]=2;else if(mode==2)bad["game"]["gui"].push_back(bad["game"]["gui"][0]);else bad["game"]["window_mode"]=3;
    auto text=bad.dump();WriteAssetFile("tests/bad-game.lunoryx",text.data(),text.size());auto hash=CompleteDocumentHash();assert(!LoadStudioProject("tests/bad-game.lunoryx")&&CompleteDocumentHash()==hash);}
  CheckLua("assert(gui_text('start','Play')); assert(gui_visible('start',true)); assert(gui_value('start',.8)); assert(not gui_text('missing','x')); assert(not pcall(gui_value,'start',0/0))");
  auto before=g_Game.Widgets[0].Text;StartSimulation();CheckLua("gui_text('start','runtime')");StopSimulation();assert(g_Game.Widgets[0].Text==before);
  g_Objects.clear();GameObject actor("actor");actor.Dynamic=true;actor.Position[0]=-3;g_Objects.push_back(actor);GameObject zone("sensor");zone.Trigger=3;g_Objects.push_back(zone);ResetGameplay();
  ExecuteLuaScript("events={}; function on_trigger(e,z,a) table.insert(events,e); assert(z==1 and a==0) end; function on_gui_click(id) assert(id=='start'); gui_text(id,'Clicked') end");g_SimulationRunning=true;
  g_Objects[0].Position[0]=0;UpdateGameplay();DispatchGameEvents();CheckLua("assert(events[1]=='enter'); assert(trigger_contains(1,0))");
  UpdateGameplay();DispatchGameEvents();CheckLua("assert(events[2]=='stay')");g_Objects[0].Position[0]=3;UpdateGameplay();DispatchGameEvents();CheckLua("assert(events[3]=='exit')");
  g_Objects[0].Position[0]=-3;ResetGameplay();g_Objects[0].Position[0]=3;UpdateGameplay();DispatchGameEvents();CheckLua("assert(events[4]=='enter' and events[5]=='exit')");
  g_GuiClicks.push_back("start");DispatchGameEvents();assert(g_Game.Widgets[0].Text=="Clicked"&&lua_gettop(g_LuaState)==0);
  ExecuteLuaScript("function on_trigger() error('trigger stack') end");g_SimulationRunning=true;g_Objects[0].Position[0]=0;UpdateGameplay();DispatchGameEvents();assert(g_DebugPaused&&!g_SimulationRunning&&g_LastLuaError.find("stack traceback")!=std::string::npos);
  g_DebugPaused=false;g_SimulationRunning=true;g_Objects.emplace_back("second sensor");g_Objects.back().Trigger=3;
  auto first=g_Objects[1].Id,second=g_Objects[2].Id,actorId=g_Objects[0].Id;
  g_TriggerEvents.clear();g_TriggerEvents.push_back({"enter",first,actorId});g_TriggerEvents.push_back({"enter",second,actorId});
  ExecuteLuaScript("hits=0; function on_trigger(e,z,a) hits=hits+1; assert(a==0); remove_object(z) end");DispatchGameEvents();CheckLua("assert(hits==2)");assert(g_Objects.size()==1);
  g_DebugPaused=false;g_Objects.clear();g_Game=StudioGameSettings();g_GuiClicks.clear();g_TriggerEvents.clear();g_TriggerOverlaps.clear();
  std::remove("tests/game-features.lunoryx");std::remove("tests/bad-game.lunoryx");
  std::cout<<"PASS: build settings and GUI persistence/undo, transactional validation, Lua GUI, swept trigger enter/stay/exit and callback traceback\n";
}
