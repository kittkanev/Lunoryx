#pragma once
struct GameSmokeTest {
  bool Enabled = false, Passed = false, Captured = false;
  double AudioEnergy = 0;
  int Phase=0,Frames=0;
  void CheckWindow(SDL_Window* window) {
    if(!Enabled||!g_PlayerMode)return;
    if(g_Game.Title!=SDL_GetWindowTitle(window))throw std::runtime_error("Game window inherited editor title");
    if(g_Game.WindowMode==1&&(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=SDL_WINDOW_FULLSCREEN_DESKTOP)throw std::runtime_error("Game fullscreen setting was not applied");
    if(!g_Game.IconPath.empty()&&ReadGameIcon(g_Game.IconPath).empty())throw std::runtime_error("Game icon was not embedded");
  }
  void Input(const GameObject &player, float &x, float &z, bool &jump) {
    if (!Enabled)
      return;
    ++Frames;
    x = z = 0;
    jump = false;
    for (int gem = 1; gem <= 3; ++gem) {
      const GameObject *target = nullptr;
      for (const auto &o : g_Objects)
        if (o.Name == "Gem " + std::to_string(gem))
          target = &o;
      if (!target)
        continue;
      auto property = target->Properties.find("Collected");
      if (property != target->Properties.end() && property->second.BoolValue)
        continue;
      float dx = target->Position[0] - player.Position[0],
            dz = target->Position[2] - player.Position[2],
            length = std::sqrt(dx * dx + dz * dz);
      if (length > .08f) {
        x = dx / length;
        z = dz / length;
      }
      jump = length < 3 && player.Grounded;
      return;
    }
  }
  void Audio() {
    if (!Enabled)
      return;
    float samples[1024] = {};
    if (studio_audio_render(samples, 512))
      for (float value : samples)
        AudioEnergy += std::abs(value);
  }
};
