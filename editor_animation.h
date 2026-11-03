#pragma once
static bool g_ShowAnimation=false;
static uint64_t g_PreviewObject=0;
static LnxPose g_PreviewPose;
static float g_TimelineTime=0;
static void StopAnimationPreview() {
  if(g_AnimationPreview)for(auto& o:g_Objects)if(o.Id==g_PreviewObject)ApplyPose(o,g_PreviewPose);
  g_AnimationPreview=false;g_PreviewPlaying=false;
}
static void RenderAnimationEditor(float dt) {
  if(!g_ShowAnimation){StopAnimationPreview();return;}
  ImGui::SetNextWindowSize(ImVec2(650,410),ImGuiCond_FirstUseEver);
  if(ImGui::Begin("Animation",&g_ShowAnimation)){
    if(g_SelectedObjectIndex<0||size_t(g_SelectedObjectIndex)>=g_Objects.size()){StopAnimationPreview();ImGui::TextWrapped("Select an object to author a transform / color clip.");}
    else{
      auto& o=g_Objects[size_t(g_SelectedObjectIndex)];
      if(g_AnimationPreview&&(g_PreviewObject!=o.Id||g_PhysicsRunning))StopAnimationPreview();
      ImGui::Text("%s | %zu keys",o.Name.c_str(),o.AnimationKeys.size());
      ImGui::TextDisabled("Position / Rotation / Scale / Color   |   seconds");
      ImGui::BeginDisabled(g_PhysicsRunning||g_AnimationPreview);
      float duration=o.AnimationDuration;
      if(ImGui::DragFloat("Duration",&duration,.05f,.01f,3600))o.AnimationDuration=std::max(duration,o.AnimationKeys.empty()?.01f:o.AnimationKeys.back().Time);
      ImGui::DragFloat("Speed",&o.AnimationSpeed,.05f,.01f,100,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
      ImGui::Checkbox("Loop",&o.AnimationLoop);ImGui::SameLine();ImGui::Checkbox("Autoplay in game",&o.AnimationAutoplay);
      ImGui::EndDisabled();
      g_TimelineTime=std::max(0.f,std::min(g_TimelineTime,o.AnimationDuration));
      ImGui::BeginDisabled(g_PhysicsRunning);
      if(ImGui::SliderFloat("Time",&g_TimelineTime,0,o.AnimationDuration,"%.3f s")){
        if(!g_AnimationPreview){g_PreviewPose=ObjectPose(o);g_PreviewObject=o.Id;g_AnimationPreview=true;}
        g_PreviewPlaying=false;SampleObjectAnimation(o,g_TimelineTime,false);
      }
      if(ImGui::Button(g_AnimationPreview?"Stop / restore pose":"Preview")){
        if(g_AnimationPreview)StopAnimationPreview();else {g_PreviewPose=ObjectPose(o);g_PreviewObject=o.Id;g_AnimationPreview=true;g_PreviewPlaying=true;g_TimelineTime=0;}
      }
      ImGui::SameLine();ImGui::TextDisabled("Stop preview before editing keys.");
      ImGui::BeginDisabled(g_AnimationPreview);
      static int easing=1;const char* curves[]={"Linear","Smooth","Step"};ImGui::Combo("Next segment",&easing,curves,3);
      if(ImGui::Button("Capture current pose")){g_ObjectHistory.Capture(g_Objects);if(!CaptureAnimationKey(o,g_TimelineTime,easing))AppendConsole("Cannot capture key: check pose ranges / 256-key limit.\n");g_ObjectHistory.Capture(g_Objects);}
      ImGui::EndDisabled();
      if(ImGui::BeginTable("Keyframes",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH)){
        ImGui::TableSetupColumn("Time");ImGui::TableSetupColumn("Interpolation");ImGui::TableSetupColumn("Actions");ImGui::TableHeadersRow();
        for(size_t i=0;i<o.AnimationKeys.size();){auto& key=o.AnimationKeys[i];ImGui::PushID(int(i));ImGui::TableNextRow();ImGui::TableNextColumn();
          if(ImGui::Selectable((std::to_string(key.Time)+" s").c_str()))g_TimelineTime=key.Time;
          ImGui::TableNextColumn();ImGui::BeginDisabled(g_AnimationPreview);ImGui::SetNextItemWidth(100);ImGui::Combo("##curve",&key.Easing,curves,3);
          ImGui::TableNextColumn();bool remove=ImGui::SmallButton("Delete");ImGui::EndDisabled();ImGui::PopID();
          if(remove)o.AnimationKeys.erase(o.AnimationKeys.begin()+i);else ++i;
        }ImGui::EndTable();
      }
      ImGui::EndDisabled();
      if(g_AnimationPreview&&g_PreviewPlaying){g_TimelineTime=float(lnx_animation_time(g_TimelineTime+dt*o.AnimationSpeed,o.AnimationDuration,o.AnimationLoop));SampleObjectAnimation(o,g_TimelineTime,false);}
      ImGui::TextWrapped("Animated transforms are kinematic. Keep Dynamic off for animated platforms/props. This timeline animates objects; skeletal skinning is not included.");
    }
  }ImGui::End();
}
