#pragma once
#include "src/io/game_file_dialog.h"
static bool g_ShowBuild=false;
static void RenderGameSettings() {
  if (!ImGui::CollapsingHeader("Game / Build", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  if (ImGui::Button("New third-person example"))
    RequestProjectAction(4);
  ImGui::BeginDisabled(g_PhysicsRunning);
  ImGui::SeparatorText("Game identity");
  char title[256];
  std::snprintf(title, sizeof(title), "%s", g_Game.Title.c_str());
  if (ImGui::InputText("Game title", title, sizeof(title)))
    g_Game.Title = title;
  const char* modes[]={"Windowed","Fullscreen (desktop)"};
  ImGui::Combo("Startup display",&g_Game.WindowMode,modes,2);
  ImGui::InputInt("Window width",&g_Game.Width);g_Game.Width=std::max(320,std::min(7680,g_Game.Width));
  ImGui::InputInt("Window height",&g_Game.Height);g_Game.Height=std::max(200,std::min(4320,g_Game.Height));
  ImGui::Checkbox("Resizable window",&g_Game.Resizable);ImGui::Checkbox("Game VSync",&g_Game.VSync);
  ImGui::Checkbox("Built-in score / HUD",&g_Game.DefaultHUD);
  char icon[512];std::snprintf(icon,sizeof(icon),"%s",g_Game.IconPath.c_str());
  if(ImGui::InputText("Game icon PNG",icon,sizeof(icon)))g_Game.IconPath=icon;
#ifdef _WIN32
  if(ImGui::Button("Choose icon...")){auto chosen=GameFileDialog(false);if(!chosen.empty())g_Game.IconPath=chosen;}
  ImGui::SameLine();
#endif
  if(ImGui::Button("Clear icon"))g_Game.IconPath.clear();
  ImGui::TextWrapped("Square PNG, 16-256 px, recommended 256x256. Embedded in the EXE and game window. Empty = system icon, no Studio branding.");
  ImGui::SeparatorText("Startup and camera");
  if (ImGui::BeginCombo("Startup script",
                        g_Game.StartupScript < 0
                            ? "(None)"
                            : g_Studio
                                  .Scripts[size_t(std::max(
                                      0, std::min(g_Game.StartupScript, 7)))]
                                  .Name)) {
    if (ImGui::Selectable("(None)", g_Game.StartupScript < 0))
      g_Game.StartupScript = -1;
    for (int i = 0; i < 8; ++i)
      if (g_Studio.Scripts[size_t(i)].Allocated &&
          ImGui::Selectable(g_Studio.Scripts[size_t(i)].Name,
                            i == g_Game.StartupScript))
        g_Game.StartupScript = i;
    ImGui::EndCombo();
  }
  if (ImGui::Button("Follow selected character") &&
      g_SelectedObjectIndex >= 0 &&
      size_t(g_SelectedObjectIndex) < g_Objects.size())
    g_Game.CameraTarget = g_Objects[size_t(g_SelectedObjectIndex)].Id;
  ImGui::DragFloat("Follow distance", &g_Game.CameraDistance, .1f, .2f, 1000);
  ImGui::SliderFloat("Follow pitch", &g_Game.CameraPitch, -85, 85);
  ImGui::SliderFloat("Follow yaw", &g_Game.CameraYaw, -180, 180);
  ImGui::SeparatorText("Output");
  static char path[512] = "Game.exe";
  ImGui::InputText("Game EXE path", path, sizeof(path));
#ifdef _WIN32
  if(ImGui::Button("Choose output EXE...")){auto chosen=GameFileDialog(true);if(!chosen.empty())std::snprintf(path,sizeof(path),"%s",chosen.c_str());}
#endif
  ImGui::BeginDisabled(g_PhysicsRunning || g_ModelBusy);
  static int result=0;
  if (ImGui::Button("Build standalone Game.exe"))result=BuildStudioGame(path)?1:-1;
  if(result)ImGui::TextColored(result>0?ImVec4(.3f,1,.7f,1):ImVec4(1,.4f,.3f,1),"%s",result>0?"Build complete. Your game is ready.":"Build failed. See Output for the exact error.");
  ImGui::EndDisabled();
  ImGui::TextWrapped("One executable includes the scene, imported resources "
                     "and Lua startup script. No compiler is needed.");
  ImGui::EndDisabled();
}

static void RenderBuildWindow(){
  if(!g_ShowBuild)return;
  ImGui::SetNextWindowSize(ImVec2(540,720),ImGuiCond_FirstUseEver);
  if(ImGui::Begin("Build Game",&g_ShowBuild)){RenderGameSettings();}
  ImGui::End();
}
