#pragma once
#include <fstream>
struct EditorPreferences {
  float OrbitSensitivity = .4f, PanSensitivity = .02f, ZoomSensitivity = .8f,
        Fov = 45, FarClip = 50000, GridStep = 1, RotationSnap = 15,
        ScaleSnap = .1f, MasterVolume = .8f, MoveSnap = .5f;
  bool AdaptiveGrid = true, ShowSettings = false, VSync = true,
       SnapEnabled = false, LocalTransform = false;
  int Theme = 0, RenderQuality = 2;
  int ViewportShading=3;
  bool FreeFly=true;
  float FlySpeed=5;
  float RenderScale = 1.f, Exposure = 1.f;
  bool CachedRenderer = true;
  float FontSize=17;
  int DocsLanguage=0;
};
static EditorPreferences g_Preferences;
static bool g_RenderReport=false;
static std::string g_PreferencesPath;
static bool SaveEditorPreferences() {
  if (g_PreferencesPath.empty())
    return false;
  nlohmann::json j = {{"viewport_shading",g_Preferences.ViewportShading},
                      {"free_fly",g_Preferences.FreeFly},{"fly_speed",g_Preferences.FlySpeed},
                      {"font_size",g_Preferences.FontSize},{"docs_language",g_Preferences.DocsLanguage},
                      {"orbit", g_Preferences.OrbitSensitivity},
                      {"pan", g_Preferences.PanSensitivity},
                      {"zoom", g_Preferences.ZoomSensitivity},
                      {"fov", g_Preferences.Fov},
                      {"far_clip", g_Preferences.FarClip},
                      {"grid_step", g_Preferences.GridStep},
                      {"adaptive_grid", g_Preferences.AdaptiveGrid},
                      {"grid", g_ShowGrid},
                      {"axes", g_ShowAxes},
                      {"vsync", g_Preferences.VSync},
                      {"render_quality",g_Preferences.RenderQuality},
                      {"render_scale",g_Preferences.RenderScale},
                      {"exposure",g_Preferences.Exposure},
                      {"cached_renderer",g_Preferences.CachedRenderer},
                      {"theme", g_Preferences.Theme},
                      {"rotation_snap", g_Preferences.RotationSnap},
                      {"scale_snap", g_Preferences.ScaleSnap},
                      {"master_volume", g_Preferences.MasterVolume},
                      {"move_snap", g_Preferences.MoveSnap},
                      {"snap_enabled", g_Preferences.SnapEnabled},
                      {"local_transform", g_Preferences.LocalTransform}};
  std::string temporary = g_PreferencesPath + ".tmp";
  try {
    std::string text=j.dump(2);WriteAssetFile(temporary,text.data(),text.size());ReplaceAssetFile(temporary,g_PreferencesPath);
  } catch(const std::exception&) { return false; }
  return true;
}
static void LoadEditorPreferences() {
  try {
    FILE* file=AssetOpen(g_PreferencesPath,"rb");if(!file)return;std::fclose(file);
    auto data=ReadAssetFile(g_PreferencesPath,1024*1024);
    nlohmann::json j=nlohmann::json::parse(data.begin(),data.end());
    EditorPreferences p = g_Preferences;
    auto number = [&](const char *key, float fallback, float lo, float hi) {
      if (!j.contains(key))
        return fallback;
      if (!j[key].is_number())
        throw std::runtime_error("Invalid setting");
      double n = j[key].get<double>();
      if (!std::isfinite(n) || n < lo || n > hi)
        throw std::runtime_error("Invalid setting range");
      return float(n);
    };
    p.FreeFly=j.value("free_fly",true);p.FlySpeed=number("fly_speed",5,.1f,1000);
    p.FontSize=number("font_size",17,13,26);
    p.DocsLanguage=j.value("docs_language",0);if(p.DocsLanguage<0||p.DocsLanguage>1)throw std::runtime_error("Invalid docs language");
    p.OrbitSensitivity = number("orbit", p.OrbitSensitivity, .01f, 3);
    p.PanSensitivity = number("pan", p.PanSensitivity, .0001f, 2);
    p.ZoomSensitivity = number("zoom", p.ZoomSensitivity, .01f, 100);
    p.Fov = number("fov", p.Fov, 15, 120);
    p.FarClip = number("far_clip", p.FarClip, 500, 1000000);
    p.GridStep = number("grid_step", p.GridStep, .01f, 10000);
    p.RotationSnap = number("rotation_snap", p.RotationSnap, .1f, 180);
    p.ScaleSnap = number("scale_snap", p.ScaleSnap, .001f, 10);
    p.MasterVolume = number("master_volume", p.MasterVolume, 0, 1);
    p.MoveSnap = number("move_snap", p.MoveSnap, .01f, 1000);
    p.SnapEnabled = j.value("snap_enabled", false);
    p.LocalTransform = j.value("local_transform", false);
    p.AdaptiveGrid = j.value("adaptive_grid", true);
    p.VSync = j.value("vsync", true);
    p.RenderQuality = j.value("render_quality",2);
    if(!j.contains("viewport_shading"))p.RenderQuality=2;
    p.ViewportShading=j.value("viewport_shading",3);if(p.ViewportShading<0||p.ViewportShading>3)throw std::runtime_error("Invalid viewport shading");
    if(p.RenderQuality<0||p.RenderQuality>2)throw std::runtime_error("Invalid render quality");
    p.RenderScale=number("render_scale",p.RenderScale,.5f,1.f);
    p.Exposure=number("exposure",p.Exposure,.5f,2.f);
    p.CachedRenderer=j.value("cached_renderer",true);
    p.Theme = j.value("theme", 0);
    if (p.Theme < 0 || p.Theme > 1)
      throw std::runtime_error("Invalid theme");
    bool grid = j.value("grid", true), axes = j.value("axes", true);
    g_Preferences = p;
    g_ShowGrid = grid;
    g_ShowAxes = axes;
  } catch (const std::exception &e) {
    AppendConsole(std::string("Settings ignored: ") + e.what() + "\n");
  }
}
