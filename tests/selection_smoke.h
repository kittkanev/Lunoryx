#pragma once
struct SelectionSmoke {
  bool Enabled=false,Passed=false,Done=false;int Phase=0,Frame=0;
  ImVec2 A,B;
  void Initialize(){if(!Enabled)return;g_PhysicsRunning=false;g_Objects.clear();for(int i=0;i<2;++i){g_Objects.emplace_back("Selection "+std::to_string(i));g_Objects.back().Position[0]=i==0?-2.f:2.f;g_Objects.back().Position[1]=.5f;}g_SelectedObjectIndex=-1;g_Modeling.Edit=false;g_Selection.Tool=1;g_Preferences.RenderQuality=0;g_SimulationRunning=false;}
  void Input(float distance,float pitch,float yaw,const float target[3]){
    if(!Enabled||Done||g_Studio.SceneSize.x<10)return;
    auto& io=ImGui::GetIO();io.ConfigInputTrickleEventQueue=false;
    ++Frame;StudioMatrix view,projection;StudioCameraMatrices(distance,pitch,yaw,target,g_Studio.SceneSize.x/g_Studio.SceneSize.y,g_Preferences.Fov,g_Preferences.FarClip,view,projection);auto vp=StudioMultiply(projection,view);
    if(Phase==0||Phase==1){int i=Phase;auto& o=g_Objects[size_t(i)];auto p=StudioProjectPoint(vp,{o.Position[0],o.Position[1],o.Position[2]},g_Studio.SceneMin.x,g_Studio.SceneMin.y,g_Studio.SceneSize.x,g_Studio.SceneSize.y);A=ImVec2(p.X-22,p.Y-22);B=ImVec2(p.X+22,p.Y+22);g_Selection.Tool=Phase==0?1:2;io.AddKeyEvent(ImGuiMod_Shift,Phase==1);if(Phase==1)A=B=ImVec2(p.X,p.Y);}
    else {g_Modeling.Edit=true;g_Modeling.Object=g_Objects[0].Id;g_SelectedObjectIndex=0;g_Modeling.Mode=Phase-2;g_Selection.Tool=1;g_Selection.XRay=true;A=ImVec2(g_Studio.SceneMin.x+2,g_Studio.SceneMin.y+2);B=ImVec2(g_Studio.SceneMin.x+g_Studio.SceneSize.x-2,g_Studio.SceneMin.y+g_Studio.SceneSize.y-2);io.AddKeyEvent(ImGuiMod_Shift,false);}
    io.AddMousePosEvent(Frame<4?A.x:B.x,Frame<4?A.y:B.y);io.AddMouseButtonEvent(0,Frame>=3&&Frame<7);
  }
  void Check(){if(!Enabled||Done||Frame<10)return;
    bool ok=false;
    if(Phase==0)ok=g_Objects[0].Selected&&!g_Objects[1].Selected;
    else if(Phase==1){ok=g_Objects[0].Selected&&g_Objects[1].Selected;auto mesh=Model::Editable(g_Objects[0]);Model::Polygonize(mesh);Model::SetEditable(g_Objects[0],std::move(mesh));}
    else ok=g_Modeling.Selected.size()==size_t(Phase==2?8:(Phase==3?12:6));
    if(!ok){Done=true;std::cerr<<"FAIL: production selection gesture phase="<<Phase<<" elements="<<g_Modeling.Selected.size()<<"\n";return;}
    ++Phase;Frame=0;if(Phase==5){Done=Passed=true;std::cout<<"PASS: production box selection, Shift circle-add and Edit vertex/edge/face box selection\n";}
  }
};
