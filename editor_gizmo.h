#pragma once
static void RenderTransformGizmo(float distance, float pitch, float yaw,
                                 const float target[3]) {
  if (g_Studio.SculptEnabled || g_PhysicsRunning ||
      g_Studio.TransformMode == 0 || g_SelectedObjectIndex < 0 ||
      size_t(g_SelectedObjectIndex) >= g_Objects.size())
    return;
  auto &selected = g_Objects[size_t(g_SelectedObjectIndex)];
  StudioMatrix matrix = StudioObjectMatrix(selected), before = matrix, view,
               projection;
  StudioCameraMatrices(
      distance, pitch, yaw, target, g_Studio.SceneSize.x / g_Studio.SceneSize.y,
      g_Preferences.Fov, g_Preferences.FarClip, view, projection);
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::AllowAxisFlip(false);
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
  ImGuizmo::SetRect(g_Studio.SceneMin.x, g_Studio.SceneMin.y,
                    g_Studio.SceneSize.x, g_Studio.SceneSize.y);
  ImGuizmo::OPERATION operation = g_Studio.TransformMode == 2 ? ImGuizmo::ROTATE
                                  : g_Studio.TransformMode == 3
                                      ? ImGuizmo::SCALE
                                      : ImGuizmo::TRANSLATE;
  float snap[3] = {g_Studio.Snap, g_Studio.Snap, g_Studio.Snap};
  if (operation == ImGuizmo::ROTATE)
    snap[0] = g_Preferences.RotationSnap;
  if (operation == ImGuizmo::SCALE)
    snap[0] = g_Preferences.ScaleSnap;
  if (ImGuizmo::Manipulate(
          view.M, projection.M, operation,
          g_Studio.LocalTransform ? ImGuizmo::LOCAL : ImGuizmo::WORLD, matrix.M,
          nullptr, g_Studio.TransformSnap ? snap : nullptr)) {
    if (!g_Studio.TransformDragging) {
      g_ObjectHistory.Capture(g_Objects);
      g_Studio.TransformDragging = true;
    }
    StudioMatrix delta = StudioMultiply(matrix, StudioInverse(before));
    for (auto &object : g_Objects)
      if (object.Selected || object.Id == selected.Id) {
        StudioMatrix transformed =
            StudioMultiply(delta, StudioObjectMatrix(object));
        StudioApplyMatrix(object, transformed);
      }
    g_SimulationRunning = false;
  }
  if (g_Studio.TransformDragging && !ImGuizmo::IsUsing()) {
    g_ObjectHistory.Capture(g_Objects);
    g_Studio.TransformDragging = false;
  }
}
static void PersistStudioPreferences() {
  g_Preferences.MoveSnap = g_Studio.Snap;
  g_Preferences.SnapEnabled = g_Studio.TransformSnap;
  g_Preferences.LocalTransform = g_Studio.LocalTransform;
  SaveEditorPreferences();
}
static void RenderEditorSettings() {
  if (!g_Preferences.ShowSettings)
    return;
  if(g_RenderReport)ImGui::SetNextWindowPos(ImVec2(1015,55),ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(g_RenderReport?ImVec2(335,700):ImVec2(490, 570), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Settings", &g_Preferences.ShowSettings)) {
    if(g_RenderReport)ImGui::PushItemWidth(145);
    ImGui::TextUnformatted("Camera");
    ImGui::SliderFloat("Orbit sensitivity", &g_Preferences.OrbitSensitivity,
                       .01f, 3);
    ImGui::SliderFloat("Pan sensitivity", &g_Preferences.PanSensitivity, .0001f,
                       .2f, "%.4f");
    ImGui::SliderFloat("Zoom sensitivity", &g_Preferences.ZoomSensitivity, .01f,
                       10);
    ImGui::SliderFloat("Field of view", &g_Preferences.Fov, 15, 120);
    ImGui::SliderFloat("Far clipping", &g_Preferences.FarClip, 500, 1000000,
                       "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::SeparatorText("World / transforms");
    ImGui::Checkbox("Grid", &g_ShowGrid);
    ImGui::Checkbox("Adaptive grid", &g_Preferences.AdaptiveGrid);
    ImGui::DragFloat("Grid unit", &g_Preferences.GridStep, .1f, .01f, 10000,
                     "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("World axes", &g_ShowAxes);
    ImGui::Checkbox("Transform snapping", &g_Studio.TransformSnap);
    ImGui::DragFloat("Move snap", &g_Studio.Snap, .01f, .01f, 1000);
    ImGui::DragFloat("Rotation snap", &g_Preferences.RotationSnap, 1, .1f, 180);
    ImGui::DragFloat("Scale snap", &g_Preferences.ScaleSnap, .01f, .001f, 10);
    ImGui::SeparatorText("Rendering");
    const char* presets[]={"Office PC","Balanced","Quality"};
    int preset=g_Preferences.RenderQuality;
    if(ImGui::Combo("Render preset",&preset,presets,3)){
      g_Preferences.RenderQuality=preset;
      g_Preferences.RenderScale=preset==0?.75f:1.f;
      g_Preferences.Exposure=1.f;
      g_Preferences.CachedRenderer=true;
    }
    ImGui::SliderFloat("Resolution scale",&g_Preferences.RenderScale,.5f,1.f,"%.2f");
    if(g_Preferences.RenderQuality==2)ImGui::SliderFloat("Exposure",&g_Preferences.Exposure,.5f,2.f);
    ImGui::Checkbox("Cached mesh renderer",&g_Preferences.CachedRenderer);
    ImGui::TextWrapped("Office: reduced resolution, direct colour. Balanced: FXAA. Quality: FXAA and filmic colour. UI stays sharp.");
    if(!g_SceneShadows.Error.empty())ImGui::TextWrapped("%s",g_SceneShadows.Error.c_str());
    if(!g_ViewportTarget.PostProcessError().empty())ImGui::TextWrapped("%s",g_ViewportTarget.PostProcessError().c_str());
    ImGui::Text("Scene draw calls: %zu | Triangles: %zu",g_StudioRenderStats.DrawCalls,g_StudioRenderStats.Triangles);
    ImGui::Text("Culled objects: %zu",g_StudioRenderStats.CulledObjects);
    ImGui::Text("Mesh cache: %.1f MiB | %s",g_StudioRenderStats.CacheBytes/(1024.f*1024.f),g_StudioRenderStats.VBO?"VBO":"compatibility");
    ImGui::SeparatorText("Editor");
    const char *themes[] = {"Dark", "Light"};
    if (ImGui::Combo("Theme", &g_Preferences.Theme, themes, 2)) {
      if (g_Preferences.Theme == 0)
        ImGui::StyleColorsDark();
      else
        ImGui::StyleColorsLight();
    }
    if (ImGui::Checkbox("VSync", &g_Preferences.VSync))
      SDL_GL_SetSwapInterval(g_Preferences.VSync ? 1 : 0);
    ImGui::SliderFloat("Master volume", &g_Preferences.MasterVolume, 0, 1);
    g_Preferences.MoveSnap = g_Studio.Snap;
    g_Preferences.SnapEnabled = g_Studio.TransformSnap;
    g_Preferences.LocalTransform = g_Studio.LocalTransform;
    if (ImGui::Button("Save settings"))
      AppendConsole(SaveEditorPreferences() ? "Settings saved.\n"
                                            : "Settings could not be saved.\n");
    ImGui::SameLine();
    if (ImGui::Button("Reset settings")) {
      g_Preferences = EditorPreferences();
      g_Preferences.ShowSettings = true;
      g_Studio.Snap = .5f;
      g_Studio.TransformSnap = false;
      g_Studio.LocalTransform = false;
      g_ShowGrid = g_ShowAxes = true;
      SDL_GL_SetSwapInterval(1);
      ImGui::StyleColorsDark();
    }
    if(g_RenderReport)ImGui::PopItemWidth();
  }
  ImGui::End();
}
