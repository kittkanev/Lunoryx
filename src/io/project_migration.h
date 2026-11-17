#pragma once
#include "src/io/sha256.h"

namespace ProjectMigration {
constexpr int CurrentSchema=4;
inline bool IsBackupPath(std::string path){
  for(char& c:path)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
  for(const char* suffix:{".pre-1.3.bak",".pre-1.4.bak",".pre-1.4.recovery.lunoryx"}){size_t n=std::strlen(suffix);if(path.size()>=n&&path.compare(path.size()-n,n,suffix)==0)return true;}
  return false;
}
inline std::string Digest(const std::vector<unsigned char>& bytes){Sha256 hash;hash.Add(bytes.data(),bytes.size());return hash.Finish();}
inline bool UsesExternalScripts(const nlohmann::json& root,const std::string& path){
  auto slash=path.find_last_of("/\\");return root.contains("project")&&path.substr(slash==std::string::npos?0:slash+1)=="project.lunoryx";
}
inline void EnsureBackup(const std::string& path){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
  CheckAssetWriteTarget(path);
  if(IsBackupPath(path))throw std::runtime_error("Migration backups are read-only; use Save Project As with another filename");
  if(!AssetExists(path))return;
  auto original=ReadAssetFile(path,32*1024*1024);CheckJsonBudget(original.begin(),original.end());
  auto old=nlohmann::json::parse(original);int schema=ReadInteger(old.at("schema"),1,CurrentSchema);
  if(old.contains("format_version")&&ReadInteger(old.at("format_version"),1,CurrentSchema)!=schema)throw std::runtime_error("Project schema and format version disagree");
  if(schema==CurrentSchema)return;
  auto rawPath=SiblingAssetPath(path,".pre-1.4.bak");
  auto recoveryPath=SiblingAssetPath(path,".pre-1.4.recovery.lunoryx");
  auto digest=Digest(original);
  auto checkRecovery=[&](){
    auto bytes=ReadAssetFile(recoveryPath,32*1024*1024);CheckJsonBudget(bytes.begin(),bytes.end());auto backup=nlohmann::json::parse(bytes);
    if(backup.at("migration_backup").at("original_sha256")!=digest||ReadInteger(backup.at("schema"),1,3)!=schema||!backup.at("scripts").is_array())throw std::runtime_error("Existing migration recovery does not match the old project; preserve it and choose another project path");
  };
  if(AssetExists(rawPath)){
    if(ReadAssetFile(rawPath,32*1024*1024)!=original)throw std::runtime_error("Existing pre-1.4 backup belongs to different project bytes; preserve it and choose another project path");
    checkRecovery();return;
  }
  if(!AssetExists(recoveryPath)){
    auto recovery=old;auto& scripts=recovery.at("scripts");
    if(!scripts.is_array()||scripts.size()>32)throw std::runtime_error("Cannot back up invalid script list");
    for(auto& script:scripts){
      if(UsesExternalScripts(old,path)&&script.contains("file")){
        auto relative=script.at("file").get<std::string>();if(relative.compare(0,8,"scripts/")!=0)throw std::runtime_error("Migration script must stay in scripts/");
        auto source=ProjectAssetPath(relative,path);
        if(AssetExists(source)){
          auto bytes=ReadAssetFile(source,65535);std::string text(bytes.begin(),bytes.end());
          (void)CheckedScriptText(text);script["code"]=std::move(text);
        }
      }
      // Recovery is self-contained for all script files SyncProjectFiles can
      // overwrite. Keep relative assets beside the original project.
      script.erase("file");
    }
    recovery["migration_backup"]={{"target_schema",CurrentSchema},{"original_sha256",digest},{"original_manifest",path.substr(path.find_last_of("/\\")==std::string::npos?0:path.find_last_of("/\\")+1)}};
    auto text=recovery.dump();if(text.size()>32*1024*1024)throw std::runtime_error("Migration recovery exceeds 32 MiB; old project was not modified");
    WriteNewAssetFileAtomic(recoveryPath,text.data(),text.size());
  }else checkRecovery();
  // Publish the byte-exact manifest last. Its presence commits the pair.
  // A crash between outputs can be resumed without replacing either file.
  if(schema<3){auto legacy=SiblingAssetPath(path,".pre-1.3.bak");if(!AssetExists(legacy))WriteNewAssetFileAtomic(legacy,original.data(),original.size());}
  WriteNewAssetFileAtomic(rawPath,original.data(),original.size());
}
} // namespace ProjectMigration
