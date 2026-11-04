#pragma once
struct GameGUISmoke {
  bool Enabled=false,Passed=false;int Frame=0;
  void Input(){
    if(!Enabled)return;
    ++Frame;
    if(Frame==1){g_Game=StudioGameSettings();g_Game.StartupScript=-1;GameWidget w;w.Id="test_button";w.Type=1;w.Text="Click me";w.X=.4f;w.Y=.4f;g_Game.Widgets.push_back(w);StartSimulation();ExecuteLuaScript("function on_gui_click(id) gui_text(id,'Clicked') end");}
    if(Frame>=4){auto& io=ImGui::GetIO();io.ConfigInputTrickleEventQueue=false;io.AddMousePosEvent(g_Studio.SceneMin.x+.5f*g_Studio.SceneSize.x,g_Studio.SceneMin.y+.44f*g_Studio.SceneSize.y);io.AddMouseButtonEvent(0,Frame<7);}
  }
  void Check(bool& done){
    if(!Enabled||Frame<12)return;
    Passed=g_Game.Widgets.size()==1&&g_Game.Widgets[0].Text=="Clicked";std::cout<<(Passed?"PASS: GUI button mouse input to Lua callback in editor Play\n":"FAIL: GUI mouse callback\n");done=true;
  }
};
