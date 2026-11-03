#pragma once
static int l_animation_key(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1);float time=CheckFiniteNumber(L,2);lua_Integer curve=luaL_optinteger(L,3,0);int easing=curve>=0&&curve<=2?int(curve):-1;
  bool ok=index>=0&&uint64_t(index)<g_Objects.size()&&CaptureAnimationKey(g_Objects[size_t(index)],time,easing);
  lua_pushboolean(L,ok);return 1;
}
static int l_animation_config(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1);float duration=CheckFiniteNumber(L,2),speed=lua_gettop(L)>=4?CheckFiniteNumber(L,4):1;
  bool loop=lua_toboolean(L,3)!=0,autoplay=lua_toboolean(L,5)!=0;
  if(index<0||uint64_t(index)>=g_Objects.size()){lua_pushboolean(L,false);return 1;}
  auto& o=g_Objects[size_t(index)];auto copy=o;copy.AnimationDuration=duration;copy.AnimationSpeed=speed;copy.AnimationLoop=loop;copy.AnimationAutoplay=autoplay;
  bool ok=ValidAnimation(copy);if(ok){o.AnimationDuration=duration;o.AnimationSpeed=speed;o.AnimationLoop=loop;o.AnimationAutoplay=autoplay;}
  lua_pushboolean(L,ok);return 1;
}
static int l_animation_play(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1);bool valid=index>=0&&uint64_t(index)<g_Objects.size();
  if(valid){auto& p=g_AnimationPlayers[g_Objects[size_t(index)].Id];p.Time=0;p.Playing=true;}
  lua_pushboolean(L,valid);return 1;
}
static int l_animation_stop(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1);bool valid=index>=0&&uint64_t(index)<g_Objects.size();
  if(valid)g_AnimationPlayers[g_Objects[size_t(index)].Id].Playing=false;
  lua_pushboolean(L,valid);return 1;
}
static int l_set_trigger(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1),kind=luaL_checkinteger(L,2);
  bool valid=index>=0&&uint64_t(index)<g_Objects.size()&&kind>=0&&kind<=2;
  if(valid){auto& o=g_Objects[size_t(index)];o.Trigger=int(kind);if(kind)o.Dynamic=false;}
  lua_pushboolean(L,valid);return 1;
}
static int l_set_solid(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1);bool valid=index>=0&&uint64_t(index)<g_Objects.size();
  if(valid)g_Objects[size_t(index)].Solid=lua_toboolean(L,2)!=0;
  lua_pushboolean(L,valid);return 1;
}
static int l_respawn(lua_State* L) {
  lua_Integer index=luaL_checkinteger(L,1);lua_pushboolean(L,index>=0&&uint64_t(index)<g_Objects.size()&&RespawnActor(g_Objects[size_t(index)]));return 1;
}
