#pragma once
static void UpdateSelectionGesture(){
  auto& s=g_Selection;auto& io=ImGui::GetIO();s.Paint=s.Replace=false;
  if(s.Tool==0||g_PhysicsRunning||g_AnimationPreview||g_ModelBusy||ImGui::IsMouseDown(1)){s.Dragging=false;return;}
  if(g_Studio.SceneHovered&&ImGui::IsMouseClicked(0)){s.Start=s.End=io.MousePos;s.Dragging=true;s.Replace=!io.KeyShift&&!io.KeyCtrl;}
  if(s.Dragging){s.End=ImVec2(std::max(g_Studio.SceneMin.x,std::min(io.MousePos.x,g_Studio.SceneMin.x+g_Studio.SceneSize.x)),std::max(g_Studio.SceneMin.y,std::min(io.MousePos.y,g_Studio.SceneMin.y+g_Studio.SceneSize.y)));s.Remove=io.KeyCtrl;
    if(s.Tool==2){s.Paint=ImGui::IsMouseDown(0)&&SceneContains(io.MousePos.x,io.MousePos.y);}
    else if(ImGui::IsMouseReleased(0)){s.Paint=true;s.Replace=!io.KeyShift&&!io.KeyCtrl;}
    if(ImGui::IsMouseReleased(0))s.Dragging=false;
  }
  auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(g_Studio.SceneMin,ImVec2(g_Studio.SceneMin.x+g_Studio.SceneSize.x,g_Studio.SceneMin.y+g_Studio.SceneSize.y),true);
  if(s.Tool==2&&g_Studio.SceneHovered)draw->AddCircle(io.MousePos,s.Radius,IM_COL32(90,225,235,240),48,1.5f);
  if(s.Tool==1&&s.Dragging){ImVec2 a(std::min(s.Start.x,s.End.x),std::min(s.Start.y,s.End.y)),b(std::max(s.Start.x,s.End.x),std::max(s.Start.y,s.End.y));draw->AddRectFilled(a,b,IM_COL32(60,190,220,35));draw->AddRect(a,b,IM_COL32(80,220,235,240));}
  draw->PopClipRect();
}
static void SelectObjectsGesture(float distance,float pitch,float yaw,const float target[3]){
  if(!g_Selection.Paint||g_Modeling.Edit)return;
  StudioMatrix view,projection;StudioCameraMatrices(distance,pitch,yaw,target,g_Studio.SceneSize.x/g_Studio.SceneSize.y,g_Preferences.Fov,g_Preferences.FarClip,view,projection);
  auto vp=StudioMultiply(projection,view);
  if(g_Selection.Replace){for(auto& o:g_Objects)o.Selected=false;g_SelectedObjectIndex=-1;}
  for(size_t i=0;i<g_Objects.size();++i){auto& o=g_Objects[i];if(!o.Visible)continue;
    Bounds b;if(!WorldBounds(o,b))continue;auto p=StudioProjectPoint(vp,{(b.Min.x+b.Max.x)*.5f,(b.Min.y+b.Max.y)*.5f,(b.Min.z+b.Max.z)*.5f},g_Studio.SceneMin.x,g_Studio.SceneMin.y,g_Studio.SceneSize.x,g_Studio.SceneSize.y);
    if(p.Valid&&StudioSelectionContains(p.X,p.Y,g_Selection.Start.x,g_Selection.Start.y,g_Selection.End.x,g_Selection.End.y,g_Selection.Tool==2,g_Selection.Radius)){
      o.Selected=!g_Selection.Remove;if(o.Selected)g_SelectedObjectIndex=int(i);
    }
  }
  if(g_SelectedObjectIndex>=0&&!g_Objects[size_t(g_SelectedObjectIndex)].Selected){g_SelectedObjectIndex=-1;for(size_t i=0;i<g_Objects.size();++i)if(g_Objects[i].Selected)g_SelectedObjectIndex=int(i);}
}
