#pragma once
static void PaintGameWidget(ImDrawList* draw,const GameWidget& w,ImVec2 origin,ImVec2 size) {
  ImVec2 a(origin.x+w.X*size.x,origin.y+w.Y*size.y),b(a.x+w.Width*size.x,a.y+w.Height*size.y);
  auto color=ImGui::ColorConvertFloat4ToU32(ImVec4(w.Color[0],w.Color[1],w.Color[2],w.Color[3]));
  auto text=ImGui::ColorConvertFloat4ToU32(ImVec4(w.TextColor[0],w.TextColor[1],w.TextColor[2],w.TextColor[3]));
  if(w.Type!=0)draw->AddRectFilled(a,b,color,4);
  if(w.Type==3)draw->AddRectFilled(a,ImVec2(a.x+(b.x-a.x)*w.Value,b.y),text,4);
  draw->PushClipRect(a,b,true);
  if(w.Type!=3){auto textSize=ImGui::CalcTextSize(w.Text.c_str());draw->AddText(ImVec2(a.x+6,a.y+std::max(0.f,(b.y-a.y-textSize.y)*.5f)),text,w.Text.c_str());}
  draw->PopClipRect();
}
static void DrawGameGUI() {
  if(!g_PhysicsRunning)return;
  auto origin=g_PlayerMode?ImVec2(0,0):g_Studio.SceneMin;
  auto size=g_PlayerMode?ImGui::GetIO().DisplaySize:g_Studio.SceneSize;
  auto* draw=ImGui::GetForegroundDrawList();draw->PushClipRect(origin,ImVec2(origin.x+size.x,origin.y+size.y),true);
  for(const auto& w:g_Game.Widgets)if(w.Visible)PaintGameWidget(draw,w,origin,size);
  if(!g_DebugPaused&&(g_PlayerMode||g_Studio.SceneHovered)&&(ImGui::IsMouseClicked(0)||ImGui::IsMouseReleased(0))) {
    auto mouse=ImGui::GetIO().MousePos;
    for(auto it=g_Game.Widgets.rbegin();it!=g_Game.Widgets.rend();++it)if(it->Visible&&it->Type==1){
      float x=(mouse.x-origin.x)/std::max(size.x,1.f),y=(mouse.y-origin.y)/std::max(size.y,1.f);
      if(x>=it->X&&y>=it->Y&&x<=it->X+it->Width&&y<=it->Y+it->Height){if(ImGui::IsMouseClicked(0))g_GuiPressed=it->Id;else if(g_GuiPressed==it->Id&&g_GuiClicks.size()<128)g_GuiClicks.push_back(it->Id);break;}
    }
  }
  if(ImGui::IsMouseReleased(0))g_GuiPressed.clear();
  draw->PopClipRect();
}
