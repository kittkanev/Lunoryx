#pragma once
static void RenderDebugPanel() {
  if(!g_ShowDebug)return;
  ImGui::SetNextWindowSize(ImVec2(560,420),ImGuiCond_FirstUseEver);
  if(ImGui::Begin("Debugger",&g_ShowDebug)){
    ImGui::Text("Runtime: %s",g_PhysicsRunning?(g_DebugPaused?"Paused":"Running"):"Stopped");
    ImGui::BeginDisabled(!g_PhysicsRunning);
    if(ImGui::Button(g_DebugPaused?"Resume":"Pause")){g_DebugPaused=!g_DebugPaused;if(!g_DebugPaused)g_SimulationRunning=true;}
    ImGui::SameLine();if(ImGui::Button("Step 1/60 s")){g_DebugPaused=true;g_DebugStep=true;}
    ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Clear errors"))g_LastLuaError.clear();
    ImGui::Checkbox("Collision / trigger bounds",&g_DebugBounds);
    ImGui::Text("Frame %.2f ms | %.0f FPS",1000.f/ImGui::GetIO().Framerate,ImGui::GetIO().Framerate);
    ImGui::Text("Draws %zu | triangles %zu | culled %zu",g_StudioRenderStats.DrawCalls,g_StudioRenderStats.Triangles,g_StudioRenderStats.CulledObjects);
    ImGui::Text("Cache %.2f MiB | deaths %u | checkpoints %u",g_StudioRenderStats.CacheBytes/1048576.,g_DeathCount,g_CheckpointCount);
    if(g_SelectedObjectIndex>=0&&size_t(g_SelectedObjectIndex)<g_Objects.size()){
      const auto& o=g_Objects[size_t(g_SelectedObjectIndex)];
      ImGui::SeparatorText("Selected object");ImGui::Text("%s | stable ID %llu",o.Name.c_str(),(unsigned long long)o.Id);
      ImGui::Text("Position %.3f %.3f %.3f",o.Position[0],o.Position[1],o.Position[2]);
      ImGui::Text("Velocity %.3f %.3f %.3f | grounded %s",o.Velocity[0],o.Velocity[1],o.Velocity[2],o.Grounded?"yes":"no");
    }
    ImGui::SeparatorText("Last Lua traceback");ImGui::BeginChild("Trace",ImVec2(0,0),true);ImGui::TextWrapped("%s",g_LastLuaError.empty()?"No runtime errors.":g_LastLuaError.c_str());ImGui::EndChild();
  }ImGui::End();
}
static void RenderDebugBounds() {
  if(!g_DebugBounds||g_PlayerMode)return;
  glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT|GL_CURRENT_BIT);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glLineWidth(1.5f);
  for(const auto& o:g_Objects){if(!o.Solid&&!o.Trigger)continue;Bounds b;if(!WorldBounds(o,b))continue;
    glColor3f(o.Trigger==1?1.f:.2f,o.Trigger==2?1.f:.3f,o.Trigger?.2f:1.f);
    const float lo[3]={b.Min.x,b.Min.y,b.Min.z},hi[3]={b.Max.x,b.Max.y,b.Max.z};
    glBegin(GL_LINES);for(int v=0;v<8;++v)for(int axis=0;axis<3;++axis)if(!(v&(1<<axis))){int w=v|(1<<axis);glVertex3f(v&1?hi[0]:lo[0],v&2?hi[1]:lo[1],v&4?hi[2]:lo[2]);glVertex3f(w&1?hi[0]:lo[0],w&2?hi[1]:lo[1],w&4?hi[2]:lo[2]);}glEnd();
  }glPopAttrib();
}
