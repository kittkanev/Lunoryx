#pragma once
static std::vector<nlohmann::json> g_ToManagedMessages,g_ToLuaMessages;
static int l_luaryx_emit(lua_State* L){
  size_t nameSize=0,valueSize=0;const char* name=luaL_checklstring(L,1,&nameSize);const char* value=luaL_optlstring(L,2,"null",&valueSize);
  if(nameSize>128||valueSize>65536||g_ToManagedMessages.size()>=256)return luaL_error(L,"Luaryx message budget exceeded");
  auto payload=nlohmann::json::parse(value,value+valueSize);g_ToManagedMessages.push_back({{"name",std::string(name,nameSize)},{"value",std::move(payload)}});return 0;
}
static void InstallLuaryxApi(lua_State* L){
  lua_register(L,"luaryx_emit",LuaBinding<l_luaryx_emit>);
  const char* profile=R"LUA(
local Entity={};Entity.__index=Entity
local function index(self) local i=object_index(self.id);if i<0 then error('Entity no longer exists: '..self.id,3) end;return i end
function Entity:position(...) if select('#',...)==0 then return get_position(index(self)) end;return set_position(index(self),...) end
function Entity:rotation(...) if select('#',...)==0 then return get_rotation(index(self)) end;return set_rotation(index(self),...) end
function Entity:scale(...) if select('#',...)==0 then return get_scale(index(self)) end;return set_scale(index(self),...) end
function Entity:velocity(...) if select('#',...)==0 then return get_velocity(index(self)) end;return set_velocity(index(self),...) end
function Entity:property(name,...) if select('#',...)==0 then return get_property(index(self),name) end;return set_property(index(self),name,...) end
function Entity:destroy() return remove_object(index(self)) end
luaryx={emit=luaryx_emit,version='1.0',language='Lua 5.4'}
function luaryx.get(id) if object_index(id)<0 then return nil end;return setmetatable({id=id},Entity) end
function luaryx.create(name,kind) return luaryx.get(object_id(add_object(name,kind or 'cube'))) end
function luaryx.find(name) local ids=find_objects(name);if #ids>1 then error('Ambiguous name; use a stable entity ID',2) end;return ids[1] and luaryx.get(ids[1]) end
)LUA";
  if(luaL_loadbufferx(L,profile,std::strlen(profile),"Luaryx API","t")!=LUA_OK||lua_pcall(L,0,0,0)!=LUA_OK)throw std::runtime_error("Cannot initialize Luaryx API");
}
static int l_require_module(lua_State* L){
  size_t n=0;const char* requested=luaL_checklstring(L,1,&n);if(!n||n>63||std::memchr(requested,'\0',n)||std::strstr(requested,"..")||std::strchr(requested,'/')||std::strchr(requested,'\\')||std::strchr(requested,':'))return luaL_error(L,"Invalid project module name");
  const char* code=nullptr;const char* filename=nullptr;
  {std::string name(requested,n);for(const auto& doc:g_Studio.Scripts)if(doc.Allocated&&doc.Module&&doc.Language==0){std::string candidate=doc.Name;auto extension=candidate.find_last_of('.');if(extension!=std::string::npos)candidate.resize(extension);std::replace(candidate.begin(),candidate.end(),'/','.');if(candidate==name){code=doc.Code;filename=doc.Name;break;}}}
  if(!code)return luaL_error(L,"Project module not found: %s",requested);
  lua_getfield(L,LUA_REGISTRYINDEX,"luaryx.modules");lua_getfield(L,-1,requested);if(!lua_isnil(L,-1))return 1;lua_pop(L,1);
  lua_getfield(L,LUA_REGISTRYINDEX,"luaryx.loading");lua_getfield(L,-1,requested);bool cycle=lua_toboolean(L,-1)!=0;lua_pop(L,1);if(cycle)return luaL_error(L,"Cyclic project module: %s",requested);lua_pushboolean(L,1);lua_setfield(L,-2,requested);
  int status=luaL_loadbufferx(L,code,std::strlen(code),filename,"t");if(status==LUA_OK)status=lua_pcall(L,0,1,0);
  lua_pushnil(L);lua_setfield(L,-3,requested);if(status!=LUA_OK)return lua_error(L);
  if(lua_isnil(L,-1)){lua_pop(L,1);lua_pushboolean(L,1);}lua_pushvalue(L,-1);lua_setfield(L,-4,requested);return 1;
}
static void DispatchLuaryxMessages(){
  auto messages=std::move(g_ToLuaMessages);g_ToLuaMessages.clear();
  for(const auto& message:messages){LuaStackGuard stack(g_LuaState);LuaExecutionGuard budget(g_LuaState);lua_getglobal(g_LuaState,"on_message");if(!lua_isfunction(g_LuaState,-1))continue;auto name=message.at("name").get<std::string>(),value=message.at("value").dump();lua_pushlstring(g_LuaState,name.data(),name.size());lua_pushlstring(g_LuaState,value.data(),value.size());if(lua_pcall(g_LuaState,2,0,0)!=LUA_OK)AppendConsole("Luaryx message: "+LuaErrorText(g_LuaState)+"\n");}
}
