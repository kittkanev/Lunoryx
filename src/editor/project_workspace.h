#pragma once
#include <filesystem>
namespace ProjectFS=std::filesystem;
static ProjectFS::path ProjectNativePath(const std::string& value){
#ifdef _WIN32
  return ProjectFS::path(WidePath(value));
#else
  return ProjectFS::u8path(value);
#endif
}
static std::vector<std::string> g_RecentProjects;
static std::string RecentProjectsFile(){return PathDirectory(g_PreferencesPath)+"/projects.json";}
static void LoadRecentProjects(){try{auto bytes=ReadAssetFile(RecentProjectsFile(),65536);auto list=nlohmann::json::parse(bytes);if(!list.is_array()||list.size()>20)return;for(const auto& value:list){auto path=value.get<std::string>();CheckLocalAssetPath(path);if(path.size()<8192)g_RecentProjects.push_back(path);}}catch(const std::exception&){} }
static void RememberProject(){if(g_ProjectDirectory.empty()||g_PreferencesPath.empty())return;g_RecentProjects.erase(std::remove(g_RecentProjects.begin(),g_RecentProjects.end(),g_ProjectDirectory),g_RecentProjects.end());g_RecentProjects.insert(g_RecentProjects.begin(),g_ProjectDirectory);if(g_RecentProjects.size()>20)g_RecentProjects.resize(20);try{auto text=nlohmann::json(g_RecentProjects).dump();WriteAssetFileAtomic(RecentProjectsFile(),text.data(),text.size());}catch(const std::exception&){} }
static std::string StoredAsset(const std::vector<unsigned char>& bytes,const std::string& suffix){if(bytes.empty())return {};auto stem="assets/"+std::to_string(GamePackage::CRC(bytes.data(),bytes.size()))+"-"+std::to_string(bytes.size());std::string relative=stem+suffix;for(int collision=0;;++collision){auto path=ConfinedAssetPath(relative,g_ProjectDirectory);if(!AssetExists(path)){WriteAssetFileAtomic(path,bytes.data(),bytes.size());return path;}if(ReadAssetFile(path,16*1024*1024)==bytes)return path;relative=stem+"-"+std::to_string(collision+1)+suffix;}}
static bool SyncProjectFiles(){
  if(g_ProjectDirectory.empty())return true;
  try{CheckUniqueScriptNames(g_Studio.Scripts);CreateAssetDirectories(g_ProjectDirectory,"scripts");CreateAssetDirectories(g_ProjectDirectory,"assets");
    for(const auto& doc:g_Studio.Scripts)if(doc.Allocated){std::string name=doc.Name;if(name.empty()||name.find("..")!=std::string::npos||name.find(':')!=std::string::npos||name.find('\\')!=std::string::npos||AbsolutePath(name))throw std::runtime_error("Script names must be safe project-relative paths");auto parent=PathDirectory(name);if(parent!=".")CreateAssetDirectories(g_ProjectDirectory,"scripts/"+parent);auto destination=ConfinedAssetPath("scripts/"+name,g_ProjectDirectory);WriteAssetFileAtomic(destination,doc.Code,std::strlen(doc.Code));}
    for(auto& object:g_Objects)for(auto& material:object.Materials)if(!material.Image.empty()||!material.Texture.empty()){auto bytes=MaterialImageBytes(material);material.Texture=StoredAsset(bytes,".image");material.Image.clear();}
    for(auto& sound:g_Sounds)if(!sound.Data.empty()||!sound.Path.empty()){auto bytes=sound.Data.empty()?ReadAssetFile(sound.Path,16*1024*1024):sound.Data;sound.Path=StoredAsset(bytes,".audio");sound.Data.clear();}
    if(!g_Game.IconData.empty()||!g_Game.IconPath.empty()){auto bytes=g_Game.IconData.empty()?ReadGameIcon(g_Game.IconPath):ValidateGameIcon(g_Game.IconData);g_Game.IconPath=StoredAsset(bytes,".png");g_Game.IconData.clear();}
    return true;
  }catch(const std::exception& error){AppendConsole(std::string("Project files: ")+error.what()+"\n");return false;}
}
static bool CreateProjectFolder(const std::string& name,const std::string& folder){
  try{if(name.empty()||name.size()>128||name.find('\0')!=std::string::npos)throw std::runtime_error("Choose a project name");CheckLocalAssetPath(folder);auto root=AbsoluteAssetPath(folder);ProjectFS::create_directories(ProjectNativePath(root));root=CanonicalAssetPath(root);auto manifest=ConfinedAssetPath("project.lunoryx",root);if(AssetExists(manifest))throw std::runtime_error("This folder already contains a project; open it instead");
    if(g_PhysicsRunning)StopSimulation();
    StopManagedScripts();for(const auto& sound:g_Sounds)studio_audio_unload(sound.Id);g_Objects.clear();g_Sounds.clear();g_Game=StudioGameSettings();for(auto& doc:g_Studio.Scripts)doc=ScriptDocument();auto& main=g_Studio.Scripts[0];main.Allocated=main.Open=true;std::snprintf(main.Name,sizeof(main.Name),"Main.luaryx");std::snprintf(main.Code,sizeof(main.Code),"-- Luaryx project entry\nfunction on_update(dt)\nend\n");
    // Format source strings contain real newlines on disk.
    std::string source=main.Code;for(size_t at=0;(at=source.find("\\n",at))!=std::string::npos;)source.replace(at,2,"\n");std::snprintf(main.Code,sizeof(main.Code),"%s",source.c_str());
    g_ProjectDirectory=root;g_ProjectName=name;g_OpenedProject=manifest;g_CSharpTrusted=true;g_ShowProjectPicker=false;g_Studio.ActiveScript=0;g_Studio.RequestDocument=-2;g_ObjectHistory.Clear();InitLuaState();if(!SyncProjectFiles()||!SaveStudioProject(manifest))throw std::runtime_error("Cannot save new project");g_ObjectHistory.Capture(g_Objects);RememberProject();return true;
  }catch(const std::exception& error){AppendConsole(std::string("Create project: ")+error.what()+"\n");return false;}
}
static bool OpenProjectFolder(const std::string& folder){try{auto root=CanonicalAssetPath(folder);auto manifest=ConfinedAssetPath("project.lunoryx",root);if(!LoadStudioProject(manifest))return false;g_ProjectDirectory=root;if(g_ProjectName.empty())g_ProjectName=ProjectFS::path(ProjectNativePath(root)).filename().u8string();g_ShowProjectPicker=false;RememberProject();return true;}catch(const std::exception& error){AppendConsole(std::string("Open project: ")+error.what()+"\n");return false;}}
static std::string ChooseProjectDirectory(){return ChooseNativePath(false,true);}
static void RenderProjectLauncher(){
  if(g_PlayerMode)return;
  if(g_RequestCSharpTrust){ImGui::OpenPopup("Trust C# code?");g_RequestCSharpTrust=false;}
  if(ImGui::BeginPopupModal("Trust C# code?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("C# can access files, network and processes with your OS permissions.\nRun only projects and C# sources you trust. Trust lasts for this session.");if(ImGui::Button("Trust and run")){g_CSharpTrusted=true;if(g_PhysicsRunning){StopSimulation();StartSimulation();}else StartManagedScripts();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel")){if(g_PhysicsRunning)StopSimulation();ImGui::CloseCurrentPopup();}ImGui::EndPopup();}
  if(!g_ShowProjectPicker)return;
  auto viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);ImGui::Begin("Projects",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking);
  ImGui::SetCursorPos(ImVec2(std::max(24.f,(viewport->WorkSize.x-760)*.5f),std::max(24.f,(viewport->WorkSize.y-510)*.5f)));ImGui::BeginChild("Project home",ImVec2(760,510),ImGuiChildFlags_Borders);if(g_LogoTexture){ImGui::Image(ImTextureID(g_LogoTexture),ImVec2(56,56));ImGui::SameLine();}ImGui::TextUnformatted("Lunoryx Studio\nProjects / Проекты");ImGui::SeparatorText("Create a project / Создать проект");
  static char name[129]="My Project",folder[2048]="";ImGui::InputText("Name / Имя",name,sizeof(name));ImGui::InputText("Folder / Папка",folder,sizeof(folder));ImGui::SameLine();if(ImGui::Button("Browse...")){auto chosen=ChooseProjectDirectory();if(!chosen.empty())std::snprintf(folder,sizeof(folder),"%s",chosen.c_str());}
  if(ImGui::Button("Create project")){if(folder[0])CreateProjectFolder(name,folder);else AppendConsole("Choose the project folder first.\n");}ImGui::SameLine();if(ImGui::Button("Open folder")){auto chosen=folder[0]?std::string(folder):ChooseProjectDirectory();if(!chosen.empty())OpenProjectFolder(chosen);}
  ImGui::SeparatorText("Recent projects / Недавние");for(const auto& recent:g_RecentProjects)if(ImGui::Selectable(recent.c_str())){OpenProjectFolder(recent);break;}
  ImGui::Separator();ImGui::TextWrapped("A project folder stores its scene, scripts, assets and recovery snapshots together.\nLua/Luaryx and C# share scene entities, properties and messages.");ImGui::TextWrapped("%s",g_ConsoleOutput.substr(g_ConsoleOutput.size()>900?g_ConsoleOutput.size()-900:0).c_str());if(ImGui::Button("Exit"))g_ExitRequested=true;ImGui::EndChild();ImGui::End();
}
#include "src/io/sha256.h"
static void PackManagedRuntime(std::map<std::string,std::vector<unsigned char>>& assets,nlohmann::json& project,size_t& bytes){
  auto root=PathDirectory(ManagedWorkerPath());
#ifdef __APPLE__
  auto name=ProjectFS::path(root).filename().u8string();if(name=="arm64"||name=="x64")root=PathDirectory(root);
#endif
  if(!AssetExists(ManagedWorkerPath()))throw std::runtime_error("The installed C# runtime is missing");
  for(const auto& entry:ProjectFS::recursive_directory_iterator(ProjectNativePath(root))){if(entry.is_symlink())throw std::runtime_error("C# runtime must contain regular installed files");if(!entry.is_regular_file())continue;auto relative=entry.path().lexically_relative(ProjectNativePath(root)).generic_u8string();auto name="runtime/"+relative;GamePackage::CheckResourceName(name);auto data=ReadAssetFile(entry.path().u8string(),128*1024*1024);if(data.size()>GamePackage::MaxPayload-bytes||assets.size()>=4095)throw std::runtime_error("C# runtime exceeds game resource budget");bytes+=data.size();assets.emplace(name,std::move(data));}
  project["game"]["managed_runtime"]=true;
}
static void PrepareManagedRuntime(){
  Sha256 digest;size_t files=0;for(const auto& asset:VirtualAssets())if(asset.first.compare(0,sizeof("/__lunoryx_bundle__/runtime/")-1,"/__lunoryx_bundle__/runtime/")==0){digest.Add(asset.first.c_str(),asset.first.size()+1);digest.Add(asset.second.data(),asset.second.size());++files;}
  if(!files)throw std::runtime_error("Game has C# scripts but no packaged .NET runtime");
  char* preference=SDL_GetPrefPath("Lunoryx","ManagedRuntime");if(!preference)throw std::runtime_error("Cannot create private C# runtime cache");std::string base=preference;SDL_free(preference);CreateAssetDirectories(base,digest.Finish());auto root=NormalizePath(base+"/"+digest.Finish());
  for(const auto& asset:VirtualAssets())if(asset.first.compare(0,sizeof("/__lunoryx_bundle__/runtime/")-1,"/__lunoryx_bundle__/runtime/")==0){auto relative=asset.first.substr(sizeof("/__lunoryx_bundle__/runtime/")-1);auto parent=PathDirectory(relative);if(parent!=".")CreateAssetDirectories(root,parent);auto destination=ConfinedAssetPath(relative,root);bool same=false;if(AssetExists(destination))try{same=ReadAssetFile(destination,128*1024*1024)==asset.second;}catch(const std::exception&){}if(!same)WriteAssetFileAtomic(destination,asset.second.data(),asset.second.size());
#ifndef _WIN32
    if(relative=="Luaryx.Worker"||(relative.size()>14&&relative.substr(relative.size()-14)=="/Luaryx.Worker")){AssetParentAnchor pinned(destination);int fd=openat(pinned.Descriptor,pinned.Name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)throw std::runtime_error("Cannot open extracted C# worker");int result=fchmod(fd,0700);close(fd);if(result)throw std::runtime_error("Cannot mark C# runtime executable");}
#endif
  }
#ifdef _WIN32
  g_ManagedRuntimeOverride=root+"/Luaryx.Worker.exe";
#else
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  g_ManagedRuntimeOverride=root+"/arm64/Luaryx.Worker";
#elif defined(__APPLE__)
  g_ManagedRuntimeOverride=root+"/x64/Luaryx.Worker";
#else
  g_ManagedRuntimeOverride=root+"/Luaryx.Worker";
#endif
#endif
}
