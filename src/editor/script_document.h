#pragma once
#include <array>
#include <vector>
#include <string>
static const size_t kScriptSlots=32;
struct ScriptDocument {
    bool Open=false,Allocated=false,Module=false;
    int Language=0; // 0: Lua/Luaryx, 1: C#
    char Name[64]={};char Code[65536]={};
};
using ScriptDocuments=std::array<ScriptDocument,kScriptSlots>;
static ScriptDocuments& StudioScripts();
struct ScriptSnapshot {size_t Slot;std::string Name,Code;bool Open,Module;int Language;};
struct TextEditHistory {
    std::vector<std::string> States;size_t Cursor=0,Bytes=0;
    void Capture(const char* code){if(!States.empty()&&States[Cursor]==code)return;while(States.size()>Cursor+1){Bytes-=States.back().size();States.pop_back();}std::string value=code;while(!States.empty()&&(Bytes+value.size()>1024*1024||States.size()>=64)){Bytes-=States.front().size();States.erase(States.begin());if(Cursor)--Cursor;}Bytes+=value.size();States.push_back(std::move(value));Cursor=States.size()-1;}
    bool Move(char* code,size_t capacity,int direction){Capture(code);if((direction<0&&Cursor==0)||(direction>0&&Cursor+1>=States.size()))return false;Cursor=size_t(int(Cursor)+direction);std::snprintf(code,capacity,"%s",States[Cursor].c_str());return true;}
};
static std::array<TextEditHistory,kScriptSlots> g_TextHistories;
static nlohmann::json g_CSharpDiagnostics=nlohmann::json::array();
static ImFont* g_CodeFont=nullptr;
static void RunStudioScript(size_t index);
static void StartManagedScripts();static void UpdateManagedScripts(float);static void StopManagedScripts();
static bool g_CSharpTrusted=false,g_RequestCSharpTrust=false;
static std::string g_ProjectDirectory,g_ProjectName;
static bool g_ShowProjectPicker=true;
static bool g_RequestRecovery=false;
static bool SyncProjectFiles();
static bool CreateProjectFolder(const std::string&,const std::string&);
static bool OpenProjectFolder(const std::string&);
static void RenderProjectLauncher();
static std::string ChooseProjectDirectory();
static std::string ChooseNativePath(bool,bool);
static std::string g_ManagedRuntimeOverride;
static bool HasManagedScripts();static std::string ManagedWorkerPath();
static void PackManagedRuntime(std::map<std::string,std::vector<unsigned char>>&,nlohmann::json&,size_t&);
static void PrepareManagedRuntime();

static void BeginExportGLB(const std::string&);static void PollExportGLB();

#ifdef __APPLE__
static void WriteMacGameBundle(const std::string&,const std::string&,const std::map<std::string,std::vector<unsigned char>>&,const std::vector<unsigned char>&);
#endif
