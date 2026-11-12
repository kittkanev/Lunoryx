#pragma once
#include "extensions/audio/audio_backend.h"
struct StudioSound {
  uint64_t Id;
  explicit StudioSound(bool allocateId=true):Id(allocateId?NewObjectId():0){}
  std::string Name = "Sound", Path;
  std::vector<unsigned char> Data;
  float Position[3] = {0, 0, 0}, Volume = 1, Pitch = 1;
  bool Loop = false, Spatial = false, Autoplay = false;
};
static std::vector<StudioSound> g_Sounds;
static std::vector<StudioSound> g_PlaySoundSnapshot;
static int g_SelectedSound = -1;
static bool g_ShowAudio = false;
static void UnloadAllStudioSounds() {
  studio_audio_stop_all();
  // Enumerate backend voices as well: scripts may have removed their entries.
  studio_audio_unload_all();
}
static void ConfigureStudioSound(const StudioSound &s) {
  studio_audio_configure(s.Id, s.Volume, s.Pitch, s.Loop, s.Spatial,
                         s.Position);
}
static bool PlayStudioSound(const StudioSound &s) {
  char error[256];
  auto embedded = VirtualAssets().find(NormalizePath(s.Path));
  std::vector<unsigned char> fileBytes;
  bool loaded=false;
  try{const auto& bytes=!s.Data.empty()?s.Data:embedded!=VirtualAssets().end()?embedded->second:(fileBytes=ReadAssetFile(s.Path,16*1024*1024));loaded=studio_audio_load_memory(s.Id,bytes.data(),bytes.size(),error);}catch(const std::exception& e){AppendConsole(std::string("Audio: ")+e.what()+"\n");return false;}
  if (!loaded) {
    AppendConsole(std::string(error) + "\n");
    return false;
  }
  ConfigureStudioSound(s);
  studio_audio_play(s.Id);
  return true;
}
static void RegisterSoundBindings(lua_State *state);
