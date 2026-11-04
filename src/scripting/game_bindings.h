#pragma once
static int l_input_key(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  SDL_Scancode key = SDL_GetScancodeFromName(name);
  int count = 0;
  const Uint8 *keys = SDL_GetKeyboardState(&count);
  lua_pushboolean(L, keys && key > SDL_SCANCODE_UNKNOWN && key < count &&
                         keys[key]);
  return 1;
}
static int l_game_score(lua_State *L) {
  if (lua_gettop(L)) {
    lua_Integer value = luaL_checkinteger(L, 1);
    if (value < 0 || value > 1000000000)
      return luaL_error(L, "score must be 0..1000000000");
    g_GameScore = int(value);
  }
  lua_pushinteger(L, g_GameScore);
  return 1;
}
static int l_hud_text(lua_State *L) {
  size_t n = 0;
  const char *text = luaL_checklstring(L, 1, &n);
  if (n > 1024)
    return luaL_error(L, "HUD text exceeds 1024 bytes");
  g_GameHud.assign(text, n);
  return 0;
}
static int l_follow_camera(lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 1);
  float distance = lua_gettop(L) >= 2 ? CheckFiniteNumber(L, 2) : 6,
        pitch = lua_gettop(L) >= 3 ? CheckFiniteNumber(L, 3) : 22;
  bool valid = i >= 0 && uint64_t(i) < g_Objects.size() && distance >= .2f &&
               distance <= 1000 && pitch >= -85 && pitch <= 85;
  if (valid) {
    g_Game.CameraTarget = g_Objects[size_t(i)].Id;
    g_Game.CameraDistance = distance;
    g_Game.CameraPitch = pitch;
  }
  lua_pushboolean(L, valid);
  return 1;
}
static int l_restart_game(lua_State *) {
  g_RestartGame = true;
  return 0;
}

static int l_gui_text(lua_State* L) {
  const char* id=luaL_checkstring(L,1);size_t n=0;const char* text=luaL_checklstring(L,2,&n);
  if(n>1024||std::memchr(text,0,n))return luaL_error(L,"GUI text must be at most 1024 bytes without NUL");
  auto* w=FindGameWidget(id);if(w)w->Text.assign(text,n);lua_pushboolean(L,w!=nullptr);return 1;
}
static int l_gui_visible(lua_State* L) {
  const char* id=luaL_checkstring(L,1);auto* w=FindGameWidget(id);if(w)w->Visible=lua_toboolean(L,2)!=0;lua_pushboolean(L,w!=nullptr);return 1;
}
static int l_gui_value(lua_State* L) {
  const char* id=luaL_checkstring(L,1);float value=CheckFiniteNumber(L,2);auto* w=FindGameWidget(id);if(w)w->Value=std::max(0.f,std::min(1.f,value));lua_pushboolean(L,w!=nullptr);return 1;
}
static int l_trigger_contains(lua_State* L) {
  lua_Integer zone=luaL_checkinteger(L,1),actor=luaL_checkinteger(L,2);
  bool valid=zone>=0&&actor>=0&&uint64_t(zone)<g_Objects.size()&&uint64_t(actor)<g_Objects.size();
  lua_pushboolean(L,valid&&TriggerContains(g_Objects[size_t(zone)],g_Objects[size_t(actor)]));return 1;
}
