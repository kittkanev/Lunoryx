#pragma once
#include "src/scripting/luaryx_runtime.h"
static WorkerProcess g_ManagedWorker;
static std::string ManagedWorkerPath(){
  if(!g_ManagedRuntimeOverride.empty())return g_ManagedRuntimeOverride;
  const char* configured=TestEnvironment("LUNORYX_CSHARP_WORKER");if(configured)return AbsoluteAssetPath(configured);
  auto base=PathDirectory(GamePackage::ExecutablePath());
#ifdef _WIN32
  return base+"/Runtime/Luaryx.Worker.exe";
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
  auto bundled=NormalizePath(base+"/../Resources/CSharp/arm64/Luaryx.Worker");return AssetExists(bundled)?bundled:base+"/Runtime/Luaryx.Worker";
#else
  auto bundled=NormalizePath(base+"/../Resources/CSharp/x64/Luaryx.Worker");return AssetExists(bundled)?bundled:base+"/Runtime/Luaryx.Worker";
#endif
#else
  return base+"/Runtime/Luaryx.Worker";
#endif
}
static bool HasManagedScripts(){for(const auto& doc:g_Studio.Scripts)if(doc.Allocated&&doc.Language==1)return true;return false;}
static nlohmann::json ManagedSceneSnapshot(){
  auto scene=nlohmann::json::array();for(const auto& object:g_Objects){auto properties=nlohmann::json::object();for(const auto& entry:object.Properties){const auto& property=entry.second;if(property.Type==ObjectProperty::Number)properties[entry.first]=property.NumberValue;else if(property.Type==ObjectProperty::Boolean)properties[entry.first]=property.BoolValue;else properties[entry.first]=property.TextValue;}scene.push_back({{"id",object.Id},{"name",object.Name},{"position",SaveVector(object.Position)},{"rotation",SaveVector(object.Rotation)},{"scale",SaveVector(object.Scale)},{"velocity",SaveVector(object.Velocity)},{"properties",std::move(properties)}});}return scene;
}
static void ApplyManagedResponse(const nlohmann::json& response){
  if(!response.value("ok",false)){g_CSharpDiagnostics=response.value("diagnostics",nlohmann::json::array());throw std::runtime_error(response.value("error",std::string("C# worker error")));}
  const auto& commands=response.at("commands");if(!commands.is_array()||commands.size()>4096)throw std::runtime_error("C# command budget exceeded");
  if(!commands.empty()){
    auto changed=g_Objects;uint64_t next=ObjectIdCounter().load();
    for(const auto& command:commands){auto op=command.at("op").get<std::string>();uint64_t id=ReadUnsigned(command.at("id"),uint64_t(std::numeric_limits<int64_t>::max()));
      auto it=std::find_if(changed.begin(),changed.end(),[&](const GameObject& object){return object.Id==id;});
      if(op=="add"){if(it!=changed.end()||id<=next)throw std::runtime_error("Invalid new C# entity ID");auto name=command.at("name").get<std::string>();if(name.size()>255||name.find('\0')!=std::string::npos)throw std::runtime_error("Invalid C# entity name");CheckObjectLimit(changed.size());GameObject object(name,ParseMeshType(command.at("type").get<std::string>()),false);object.Id=id;next=id;changed.push_back(std::move(object));continue;}
      if(it==changed.end())throw std::runtime_error("C# entity no longer exists");
      if(op=="remove"){changed.erase(it);continue;}
      if(op=="transform"){for(const char* key:{"position","rotation","scale","velocity"})if(command.contains(key)){float* out=std::strcmp(key,"position")==0?it->Position:std::strcmp(key,"rotation")==0?it->Rotation:std::strcmp(key,"scale")==0?it->Scale:it->Velocity;ReadVector(command.at(key),out);if(std::strcmp(key,"velocity")==0)for(float value:it->Velocity)if(std::abs(value)>10000)throw std::runtime_error("Velocity outside budget");}ValidateTransform(it->Position,it->Rotation,it->Scale);}
      else if(op=="property"){auto name=command.at("name").get<std::string>();const auto& value=command.at("value");ObjectProperty property;if(value.is_boolean()){property.Type=ObjectProperty::Boolean;property.BoolValue=value.get<bool>();}else if(value.is_number()){property.Type=ObjectProperty::Number;property.NumberValue=value.get<double>();}else if(value.is_string()){property.Type=ObjectProperty::Text;property.TextValue=value.get<std::string>();}else throw std::runtime_error("Engine properties accept number, bool or string");if(!SetProperty(*it,name,property))throw std::runtime_error("Invalid C# property");}
      else throw std::runtime_error("Unknown C# scene command");
    }
    CheckSceneMeshBudget(changed,0);g_Objects.swap(changed);ReserveObjectId(next);
  }
  const auto& messages=response.at("messages");if(!messages.is_array()||messages.size()>256)throw std::runtime_error("C# message budget exceeded");for(const auto& message:messages){if(g_ToLuaMessages.size()>=256)throw std::runtime_error("Luaryx receive budget exceeded");g_ToLuaMessages.push_back(message);}
  for(const auto& log:response.at("logs")){auto text=log.get<std::string>();AppendConsole("[C#] "+text.substr(0,4096)+"\n");}
}
static void ManagedExchange(nlohmann::json request,int milliseconds){request["scene"]=ManagedSceneSnapshot();request["nextId"]=ObjectIdCounter().load();auto text=request.dump();if(text.size()>8*1024*1024)throw std::runtime_error("C# scene/message request exceeds 8 MiB");auto reply=g_ManagedWorker.Exchange(text,milliseconds,8*1024*1024);CheckJsonBudget(reply.begin(),reply.end());ApplyManagedResponse(nlohmann::json::parse(reply));}
static void StartManagedScripts(){
  if(!HasManagedScripts())return;
  if(!g_CSharpTrusted){g_RequestCSharpTrust=true;g_SimulationRunning=false;g_DebugPaused=g_PhysicsRunning;AppendConsole("C# requires explicit project trust before execution.\n");return;}
  try{g_CSharpDiagnostics=nlohmann::json::array();g_ManagedWorker.Start(ManagedWorkerPath());auto sources=nlohmann::json::array();for(const auto& doc:g_Studio.Scripts)if(doc.Allocated&&doc.Language==1)sources.push_back({{"name",std::string(doc.Name)},{"code",std::string(doc.Code)},{"module",doc.Module}});ManagedExchange({{"action","compile"},{"scripts",sources}},15000);DispatchLuaryxMessages();}
  catch(const std::exception& error){g_ManagedWorker.Stop();g_LastLuaError=error.what();g_SimulationRunning=false;g_DebugPaused=g_PhysicsRunning;AppendConsole(std::string("C# error: ")+error.what()+"\n");for(const auto& item:g_CSharpDiagnostics)if(item.is_object()){auto message=item.value("message",std::string());auto file=item.value("file",std::string());AppendConsole(file+":"+std::to_string(item.value("line",1))+":"+std::to_string(item.value("column",1))+": "+message.substr(0,4096)+"\n");}}
}
static void UpdateManagedScripts(float delta){if(!g_ManagedWorker.Running())return;try{auto messages=std::move(g_ToManagedMessages);g_ToManagedMessages.clear();auto triggers=nlohmann::json::array();for(const auto& event:g_TriggerEvents)triggers.push_back({{"event",event.Event},{"zone",event.Zone},{"actor",event.Actor}});ManagedExchange({{"action","update"},{"dt",delta},{"messages",messages},{"triggers",triggers}},500);DispatchLuaryxMessages();}catch(const std::exception& error){g_ManagedWorker.Stop();g_SimulationRunning=false;g_DebugPaused=g_PhysicsRunning;g_LastLuaError=error.what();AppendConsole(std::string("C# stopped: ")+error.what()+"\n");}}
static void StopManagedScripts(){if(g_ManagedWorker.Running())try{ManagedExchange({{"action","stop"}},500);}catch(const std::exception& error){AppendConsole(std::string("C# cleanup: ")+error.what()+"\n");}g_ManagedWorker.Stop();g_ToManagedMessages.clear();g_ToLuaMessages.clear();}
static void RunStudioScript(size_t index){if(index>=g_Studio.Scripts.size()||!g_Studio.Scripts[index].Allocated)return;auto& doc=g_Studio.Scripts[index];if(doc.Module)AppendConsole("Module saved. Use it from a normal script.\n");else if(doc.Language==1){StopManagedScripts();StartManagedScripts();}else {InitLuaState();ExecuteLuaScript(doc.Code);} }
