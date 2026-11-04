#pragma once
static bool g_ShowGUI=false;
static int g_SelectedWidget=-1;
static void RenderGUIEditor() {
  if(!g_ShowGUI)return;
  ImGui::SetNextWindowSize(ImVec2(820,580),ImGuiCond_FirstUseEver);
  if(ImGui::Begin("GUI Designer",&g_ShowGUI)) {
    ImGui::BeginDisabled(g_PhysicsRunning);
    const char* types[]={"Label","Button","Panel","Progress"};
    for(int type=0;type<4;++type){if(type)ImGui::SameLine();ImGui::PushID(type);
      if(ImGui::Button(types[type])&&g_Game.Widgets.size()<128){GameWidget w;w.Type=type;w.Text=type==2?"":types[type];int n=1;do{w.Id="widget"+std::to_string(n++);}while(FindGameWidget(w.Id));g_Game.Widgets.push_back(w);g_SelectedWidget=int(g_Game.Widgets.size())-1;}
      ImGui::PopID();}
    ImGui::TextWrapped("Drag widgets on the canvas. Coordinates/sizes are fractions of the game viewport. Last widget is drawn on top.");
    ImGui::BeginChild("Widget list",ImVec2(155,0),true);
    for(size_t i=0;i<g_Game.Widgets.size();++i){ImGui::PushID(int(i));if(ImGui::Selectable(g_Game.Widgets[i].Id.c_str(),g_SelectedWidget==int(i)))g_SelectedWidget=int(i);ImGui::PopID();}ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginGroup();auto origin=ImGui::GetCursorScreenPos();float aspect=float(g_Game.Width)/std::max(1,g_Game.Height);float width=std::max(100.f,std::min(ImGui::GetContentRegionAvail().x,280.f*aspect));ImVec2 size(width,width/aspect);
    ImGui::InvisibleButton("Canvas",size);bool hover=ImGui::IsItemHovered(),active=ImGui::IsItemActive();auto mouse=ImGui::GetIO().MousePos;
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(origin,ImVec2(origin.x+size.x,origin.y+size.y),IM_COL32(14,24,35,255));draw->PushClipRect(origin,ImVec2(origin.x+size.x,origin.y+size.y),true);
    if(hover&&ImGui::IsMouseClicked(0)){g_SelectedWidget=-1;for(int i=int(g_Game.Widgets.size())-1;i>=0;--i){const auto& w=g_Game.Widgets[size_t(i)];if(mouse.x>=origin.x+w.X*size.x&&mouse.x<=origin.x+(w.X+w.Width)*size.x&&mouse.y>=origin.y+w.Y*size.y&&mouse.y<=origin.y+(w.Y+w.Height)*size.y){g_SelectedWidget=i;break;}}}
    for(size_t i=0;i<g_Game.Widgets.size();++i){const auto& w=g_Game.Widgets[i];PaintGameWidget(draw,w,origin,size);if(int(i)==g_SelectedWidget)draw->AddRect(ImVec2(origin.x+w.X*size.x,origin.y+w.Y*size.y),ImVec2(origin.x+(w.X+w.Width)*size.x,origin.y+(w.Y+w.Height)*size.y),IM_COL32(80,240,210,255),0.f,2.f);}
    draw->PopClipRect();
    if(g_SelectedWidget>=0&&size_t(g_SelectedWidget)<g_Game.Widgets.size()){
      auto& w=g_Game.Widgets[size_t(g_SelectedWidget)];
      if(active&&!g_PhysicsRunning&&ImGui::IsMouseDragging(0)){auto delta=ImGui::GetIO().MouseDelta;w.X=std::max(0.f,std::min(1.f-w.Width,w.X+delta.x/size.x));w.Y=std::max(0.f,std::min(1.f-w.Height,w.Y+delta.y/size.y));}
      char id[64],text[1025];std::snprintf(id,sizeof(id),"%s",w.Id.c_str());std::snprintf(text,sizeof(text),"%s",w.Text.c_str());
      if(ImGui::InputText("ID",id,sizeof(id))&&id[0]&&(!FindGameWidget(id)||w.Id==id))w.Id=id;
      if(ImGui::InputTextMultiline("Text",text,sizeof(text),ImVec2(-1,50)))w.Text=text;
      ImGui::Checkbox("Visible",&w.Visible);ImGui::SameLine();ImGui::TextDisabled("Lua: gui_text / gui_visible / gui_value");
      ImGui::SliderFloat("X",&w.X,0,1);ImGui::SliderFloat("Y",&w.Y,0,1);ImGui::SliderFloat("Width",&w.Width,.01f,1);ImGui::SliderFloat("Height",&w.Height,.01f,1);
      if(w.Type==3)ImGui::SliderFloat("Value",&w.Value,0,1);
      ImGui::ColorEdit4("Background",w.Color);ImGui::ColorEdit4("Text / fill",w.TextColor);
      if(ImGui::Button("Move layer up")&&g_SelectedWidget+1<int(g_Game.Widgets.size())){std::swap(w,g_Game.Widgets[size_t(g_SelectedWidget+1)]);++g_SelectedWidget;}
      ImGui::SameLine();if(ImGui::Button("Delete widget")){g_Game.Widgets.erase(g_Game.Widgets.begin()+g_SelectedWidget);g_SelectedWidget=-1;}
    }
    ImGui::EndGroup();ImGui::EndDisabled();
  }ImGui::End();
}
