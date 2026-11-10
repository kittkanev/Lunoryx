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

static std::string CheckedScriptText(std::string text){
  if(text.compare(0,3,"\xEF\xBB\xBF")==0)text.erase(0,3);
  if(text.size()>65535||text.find('\0')!=std::string::npos)throw std::runtime_error("Script must be UTF-8 text within 65535 bytes, without NUL");
  (void)nlohmann::json(text).dump(); // strict UTF-8 validation before editor/serialization
  std::string normalized;normalized.reserve(text.size());
  for(size_t i=0;i<text.size();++i){if(text[i]=='\r'){normalized+='\n';if(i+1<text.size()&&text[i+1]=='\n')++i;}else normalized+=text[i];}
  return normalized;
}
#ifdef __APPLE__
extern "C" bool LuaryxScriptNamesEqual(const char*,const char*);
#endif
static bool ScriptNamesEqual(const std::string& a,const std::string& b){
#ifdef _WIN32
  auto x=WidePath(a),y=WidePath(b);return CompareStringOrdinal(x.c_str(),int(x.size()),y.c_str(),int(y.size()),TRUE)==CSTR_EQUAL;
#elif defined(__APPLE__)
  return LuaryxScriptNamesEqual(a.c_str(),b.c_str());
#else
  auto x=a,y=b;for(char& c:x)if(c>='A'&&c<='Z')c=char(c-'A'+'a');for(char& c:y)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return x==y;
#endif
}
static void CheckUniqueScriptNames(const ScriptDocuments& docs){
  for(size_t i=0;i<docs.size();++i)if(docs[i].Allocated){if(!docs[i].Name[0])throw std::runtime_error("Script name cannot be empty");
    for(size_t j=0;j<i;++j)if(docs[j].Allocated&&ScriptNamesEqual(docs[i].Name,docs[j].Name))throw std::runtime_error("Script filenames must be unique: "+std::string(docs[i].Name));}
}
