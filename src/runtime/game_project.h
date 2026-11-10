#pragma once
static nlohmann::json SaveGameSettings(const StudioGameSettings& g,const std::string& path) {
  nlohmann::json j={{"startup_script",g.StartupScript},{"camera_target",g.CameraTarget},{"camera_distance",g.CameraDistance},{"camera_pitch",g.CameraPitch},{"camera_yaw",g.CameraYaw},{"title",g.Title},
    {"icon",g.IconPath.empty()?"":RelativeAssetPath(g.IconPath,path)},{"width",g.Width},{"height",g.Height},{"window_mode",g.WindowMode},{"resizable",g.Resizable},{"vsync",g.VSync},{"default_hud",g.DefaultHUD},{"gui",nlohmann::json::array()}};
  for(const auto& w:g.Widgets)j["gui"].push_back({{"id",w.Id},{"text",w.Text},{"type",w.Type},{"x",w.X},{"y",w.Y},{"width",w.Width},{"height",w.Height},{"value",w.Value},{"visible",w.Visible},{"color",{w.Color[0],w.Color[1],w.Color[2],w.Color[3]}},{"text_color",{w.TextColor[0],w.TextColor[1],w.TextColor[2],w.TextColor[3]}}});
  if(!g.IconData.empty())j["icon_data"]=SaveBinary(g.IconData);
  if(!g.IconPath.empty()){try{ProjectAssetPath(j["icon"].get<std::string>(),path);}catch(const std::exception&){j["icon_data"]=SaveBinary(ReadAssetFile(g.IconPath,4*1024*1024));j["icon"]="";}}
  return j;
}
static StudioGameSettings ReadGameSettings(const nlohmann::json& j,const std::string& path) {
  StudioGameSettings g;
  g.StartupScript=ReadInteger(j.at("startup_script"),-1,int(kScriptSlots)-1);g.CameraTarget=ReadUnsigned(j.at("camera_target"),uint64_t(std::numeric_limits<int64_t>::max()));
  g.CameraDistance=ReadRange(j.at("camera_distance"),.2f,1000);g.CameraPitch=ReadRange(j.at("camera_pitch"),-85,85);g.CameraYaw=ReadRange(j.at("camera_yaw"),-180,180);
  g.Title=j.at("title").get<std::string>();g.IconPath=j.value("icon",std::string());
  if(g.Title.empty()||g.Title.size()>255||g.Title.find('\0')!=std::string::npos||g.IconPath.size()>511||g.IconPath.find('\0')!=std::string::npos)throw std::runtime_error("Invalid game title or icon path");
  g.IconPath=ProjectAssetPath(g.IconPath,path);
  if(j.contains("icon_data"))g.IconData=ReadBinary(j.at("icon_data"),4*1024*1024);
  if(j.contains("width"))g.Width=ReadInteger(j.at("width"),320,7680);
  if(j.contains("height"))g.Height=ReadInteger(j.at("height"),200,4320);
  // Preserve old projects' windowed startup and built-in HUD.
  g.WindowMode=j.contains("window_mode")?ReadInteger(j.at("window_mode"),0,1):0;
  g.Resizable=j.value("resizable",true);g.VSync=j.value("vsync",true);g.DefaultHUD=j.value("default_hud",true);
  if(j.contains("gui")){
    const auto& widgets=j.at("gui");if(!widgets.is_array()||widgets.size()>128)throw std::runtime_error("GUI supports at most 128 widgets");
    std::set<std::string> names;
    for(const auto& v:widgets){GameWidget w;w.Id=v.at("id").get<std::string>();w.Text=v.at("text").get<std::string>();
      if(w.Id.empty()||w.Id.size()>63||w.Id.find('\0')!=std::string::npos||w.Text.size()>1024||w.Text.find('\0')!=std::string::npos||!names.insert(w.Id).second)throw std::runtime_error("Invalid or duplicate GUI ID/text");
      w.Type=ReadInteger(v.at("type"),0,3);w.X=ReadRange(v.at("x"),0,1);w.Y=ReadRange(v.at("y"),0,1);w.Width=ReadRange(v.at("width"),.01f,1);w.Height=ReadRange(v.at("height"),.01f,1);w.Value=ReadRange(v.at("value"),0,1);w.Visible=v.at("visible").get<bool>();
      for(const char* key:{"color","text_color"}){const auto& c=v.at(key);if(!c.is_array()||c.size()!=4)throw std::runtime_error("Expected RGBA color");for(int k=0;k<4;++k)(std::strcmp(key,"color")==0?w.Color:w.TextColor)[k]=ReadRange(c[k],0,1);}
      g.Widgets.push_back(w);
    }
  }
  return g;
}
