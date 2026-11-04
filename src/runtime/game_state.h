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
  auto text=[&](const std::string& s){bytes(s.data(),s.size());size_t n=s.size();bytes(&n,sizeof(n));};
  bytes(&game.StartupScript,sizeof(game.StartupScript));bytes(&game.CameraTarget,sizeof(game.CameraTarget));
  bytes(&game.CameraDistance,sizeof(float));bytes(&game.CameraPitch,sizeof(float));bytes(&game.CameraYaw,sizeof(float));
  text(game.Title);text(game.IconPath);bytes(&game.Width,sizeof(int));bytes(&game.Height,sizeof(int));bytes(&game.WindowMode,sizeof(int));
  bytes(&game.Resizable,sizeof(bool));bytes(&game.VSync,sizeof(bool));bytes(&game.DefaultHUD,sizeof(bool));
  for(const auto& w:game.Widgets){text(w.Id);text(w.Text);bytes(&w.Type,sizeof(int));bytes(&w.X,sizeof(float));bytes(&w.Y,sizeof(float));bytes(&w.Width,sizeof(float));bytes(&w.Height,sizeof(float));bytes(&w.Value,sizeof(float));bytes(w.Color,sizeof(w.Color));bytes(w.TextColor,sizeof(w.TextColor));bytes(&w.Visible,sizeof(bool));}
  return h;
}

static GameWidget* FindGameWidget(const std::string& id) {
  for(auto& w:g_Game.Widgets)if(w.Id==id)return &w;
  return nullptr;
}
