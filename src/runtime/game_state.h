#pragma once
struct GameWidget {
  std::string Id="widget", Text="Text";
  int Type=0; // label, button, panel, progress
  float X=.05f,Y=.05f,Width=.25f,Height=.08f,Value=.5f;
  float Color[4]={.09f,.22f,.3f,.92f},TextColor[4]={1,1,1,1};
  bool Visible=true;
};
struct StudioGameSettings {
  int StartupScript=-1;
  uint64_t CameraTarget=0;
  float CameraDistance=6,CameraPitch=22,CameraYaw=0;
  std::string Title="My Game",IconPath;
  std::vector<unsigned char> IconData;
  int Width=1280,Height=720,WindowMode=1; // window, borderless fullscreen
  bool Resizable=true,VSync=true,DefaultHUD=false;
  std::vector<GameWidget> Widgets;
};
static StudioGameSettings g_Game,g_PlayGame;
static bool g_PlayerMode=false;
static int g_GameScore=0;
static std::string g_GameHud;
static bool g_RestartGame=false;
static std::vector<std::string> g_GuiClicks;
static std::string g_GuiPressed;
inline uint64_t GameSettingsFingerprint(const StudioGameSettings& game) {
  uint64_t h=1469598103934665603ULL;
  auto bytes=[&](const void* ptr,size_t n){auto p=static_cast<const unsigned char*>(ptr);while(n--)h=(h^*p++)*1099511628211ULL;};
  auto text=[&](const std::string& s){bytes(s.data(),s.size());uint64_t n=s.size();bytes(&n,sizeof(n));};
  auto number=[&](float v){if(v==0.f||!std::isfinite(v))v=0.f;bytes(&v,sizeof(v));};
  if(!game.IconData.empty())bytes(game.IconData.data(),game.IconData.size());
  bytes(&game.StartupScript,sizeof(game.StartupScript));bytes(&game.CameraTarget,sizeof(game.CameraTarget));
  number(game.CameraDistance);number(game.CameraPitch);number(game.CameraYaw);
  text(game.Title);text(game.IconPath);bytes(&game.Width,sizeof(int));bytes(&game.Height,sizeof(int));bytes(&game.WindowMode,sizeof(int));
  bytes(&game.Resizable,sizeof(bool));bytes(&game.VSync,sizeof(bool));bytes(&game.DefaultHUD,sizeof(bool));
  for(const auto& w:game.Widgets){text(w.Id);text(w.Text);bytes(&w.Type,sizeof(int));number(w.X);number(w.Y);number(w.Width);number(w.Height);number(w.Value);for(float v:w.Color)number(v);for(float v:w.TextColor)number(v);bytes(&w.Visible,sizeof(bool));}
  return h;
}

static GameWidget* FindGameWidget(const std::string& id) {
  for(auto& w:g_Game.Widgets)if(w.Id==id)return &w;
  return nullptr;
}
