#pragma once
// All coroutines share allocator and instruction budgets through allocator data.
struct LuaBudget {
    size_t Bytes = 0;
    size_t Limit = 256 * 1024 * 1024;
    int Remaining = 1000000;
    bool Exhausted = false;
};
static LuaBudget g_LuaBudget;
static void* BoundedLuaAlloc(void* context, void* pointer, size_t oldSize, size_t size) {
    auto& budget = *static_cast<LuaBudget*>(context);
    if (!pointer) oldSize = 0; // Lua uses osize as a type tag for new allocations.
    if (!size) { std::free(pointer); budget.Bytes -= oldSize; return nullptr; }
    if (size > oldSize && size - oldSize > budget.Limit - budget.Bytes) return nullptr;
    void* result = std::realloc(pointer, size);
    if (result) budget.Bytes = budget.Bytes - oldSize + size;
    return result;
}
static void LuaInstructionLimit(lua_State* L, lua_Debug*) {
    void* context = nullptr; lua_getallocf(L, &context);
    auto& budget = *static_cast<LuaBudget*>(context);
    budget.Remaining -= lua_gethookcount(L);
    if (budget.Exhausted || budget.Remaining <= 0) {
        budget.Exhausted = true;
        lua_sethook(L, LuaInstructionLimit, LUA_MASKCOUNT, 1);
        luaL_error(L, "Lua instruction budget exceeded");
    }
}
static int SandboxResume(lua_State* L) {
    lua_State* thread = lua_tothread(L, 1);
    if (thread) lua_sethook(thread, LuaInstructionLimit, LUA_MASKCOUNT, g_LuaBudget.Exhausted ? 1 : 1000);
    int count = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1)); lua_insert(L, 1);
    lua_call(L, count, LUA_MULTRET);
    return lua_gettop(L);
}
static int SandboxRep(lua_State* L) {
    size_t length = 0, separator = 0; luaL_checklstring(L, 1, &length);
    lua_Integer count = luaL_checkinteger(L, 2);
    if (!lua_isnoneornil(L, 3)) luaL_checklstring(L, 3, &separator);
    const size_t limit = 1024 * 1024;
    if (count > 0 && (length > limit / size_t(count) ||
        (count > 1 && separator > (limit - length * size_t(count)) / size_t(count - 1))))
        return luaL_error(L, "string.rep exceeds 1 MiB limit");
    int arguments = lua_gettop(L); lua_pushvalue(L, lua_upvalueindex(1)); lua_insert(L, 1);
    lua_call(L, arguments, 1); return 1;
}
// Lua's native matcher does not invoke VM hooks. Conservatively bound the
// backtracking search space before entering it, including lazy/optional repeats.
static int SandboxPattern(lua_State* L) {
    size_t length=0,patternLength=0;luaL_checklstring(L,1,&length);const char* pattern=luaL_checklstring(L,2,&patternLength);
    bool plain=lua_toboolean(L,lua_upvalueindex(2))&&lua_toboolean(L,4);
    if(length>65536||patternLength>1024)return luaL_error(L,"Pattern input exceeds limit");
    if(!plain){size_t cost=(length+1)*(patternLength+1);bool escaped=false,inClass=false;
        for(size_t i=0;i<patternLength;++i){char c=pattern[i];if(escaped){escaped=false;if(c=='b'||(c>='1'&&c<='9')){if(cost>10000000/(length+1))return luaL_error(L,"Pattern complexity exceeds budget");cost*=length+1;}continue;}if(c=='%'){escaped=true;continue;}if(c=='['){inClass=true;continue;}if(c==']'){inClass=false;continue;}
            if(!inClass&&(c=='*'||c=='+'||c=='-'||c=='?')){if(cost>10000000/(length+1))return luaL_error(L,"Pattern complexity exceeds budget");cost*=length+1;}}
        if(cost>10000000)return luaL_error(L,"Pattern complexity exceeds budget");}
    int count=lua_gettop(L);lua_pushvalue(L,lua_upvalueindex(1));lua_insert(L,1);lua_call(L,count,LUA_MULTRET);return lua_gettop(L);
}
static int SandboxSetMetatable(lua_State* L) {
    // Lua deliberately disables VM hooks inside __gc. User finalizers cannot
    // be safely budgeted, so reject them before Lua marks a table finalizable.
    if(lua_istable(L,2)){lua_pushliteral(L,"__gc");lua_rawget(L,2);bool finalizer=!lua_isnil(L,-1);lua_pop(L,1);if(finalizer)return luaL_error(L,"Script finalizers are not supported");}
    int count=lua_gettop(L);lua_pushvalue(L,lua_upvalueindex(1));lua_insert(L,1);lua_call(L,count,1);return 1;
}
static void OpenSandboxLibraries(lua_State* L) {
    const luaL_Reg libraries[] = {{"_G",luaopen_base},{LUA_COLIBNAME,luaopen_coroutine},
        {LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},
        {LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8},{nullptr,nullptr}};
    for (const auto* lib = libraries; lib->func; ++lib) { luaL_requiref(L, lib->name, lib->func, 1); lua_pop(L, 1); }
    for (const char* name : {"dofile","loadfile","load"}) { lua_pushnil(L); lua_setglobal(L,name); }
    lua_getglobal(L,"setmetatable");lua_pushcclosure(L,SandboxSetMetatable,1);lua_setglobal(L,"setmetatable");
    luaopen_os(L); lua_newtable(L);
    for (const char* name : {"time","clock","date","difftime"}) { lua_getfield(L,-2,name); lua_setfield(L,-2,name); }
    lua_setglobal(L,"os"); lua_pop(L,1);
    lua_getglobal(L,"string"); lua_getfield(L,-1,"rep"); lua_pushcclosure(L,SandboxRep,1); lua_setfield(L,-2,"rep");
    for(const char* name:{"find","match","gmatch","gsub"}){lua_getfield(L,-1,name);lua_pushboolean(L,std::strcmp(name,"find")==0);lua_pushcclosure(L,SandboxPattern,2);lua_setfield(L,-2,name);}
    lua_pushnil(L); lua_setfield(L,-2,"dump"); lua_pop(L,1);
    lua_getglobal(L,"coroutine"); lua_getfield(L,-1,"resume"); lua_pushcclosure(L,SandboxResume,1); lua_setfield(L,-2,"resume"); lua_pop(L,1);
    const char* wrap = "local create,resume,pack,unpack,err=coroutine.create,coroutine.resume,table.pack,table.unpack,error; "
        "coroutine.wrap=function(f) local co=create(f); return function(...) local r=pack(resume(co,...)); if not r[1] then err(r[2],2) end; return unpack(r,2,r.n) end end";
    if (luaL_loadbufferx(L,wrap,std::strlen(wrap),"sandbox","t") != LUA_OK || lua_pcall(L,0,0,0) != LUA_OK)
        throw std::runtime_error("Cannot initialize Lua sandbox");
}
