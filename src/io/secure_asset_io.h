// MinGW libstdc++ types carry hidden visibility; this is not a DLL interface.
#if defined(__GNUC__) && defined(_WIN32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#endif
inline std::recursive_mutex& AssetPolicyMutex(){static std::recursive_mutex value;return value;}
// Directory write sharing is denied as well as delete sharing: a second
// writable handle could otherwise install a junction after validation.
// Roots are pinned as directory objects. Reopens never follow untrusted
// symlinks/reparse points; writes and replacement share the same parent anchor.
struct AssetDirectoryAnchor {
  std::string Path;
#ifdef _WIN32
  std::vector<HANDLE> Handles;
#else
  int Descriptor=-1;
#endif
  explicit AssetDirectoryAnchor(const std::string& path):Path(path){
#ifdef _WIN32
    if(path.size()<3||path[1]!=':'||path[2]!='/')throw std::runtime_error("Expected a local absolute directory");
    std::string current=path.substr(0,3);size_t at=3;
    try{for(;;){HANDLE h=CreateFileW(WidePath(current).c_str(),FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot pin asset directory (Windows error "+std::to_string(GetLastError())+")");BY_HANDLE_FILE_INFORMATION info={};if(!GetFileInformationByHandle(h,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){CloseHandle(h);throw std::runtime_error("Reparse directories are not permitted");}Handles.push_back(h);if(at>=path.size())break;size_t end=path.find('/',at);if(end==std::string::npos)end=path.size();if(current.back()!='/')current+='/';current+=path.substr(at,end-at);at=end+1;}}
    catch(...){for(auto h:Handles)CloseHandle(h);
      Handles.clear();throw;}
#else
    int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)throw std::runtime_error("Cannot pin filesystem root");
    try{size_t at=1;while(at<path.size()){size_t end=path.find('/',at);if(end==std::string::npos)end=path.size();auto component=path.substr(at,end-at);at=end+1;if(component.empty())continue;if(component=="."||component=="..")throw std::runtime_error("Invalid anchored directory");int child=openat(fd,component.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(child<0)throw std::runtime_error("Asset directory changed or is a symlink");close(fd);fd=child;}Descriptor=fd;}
    catch(...){close(fd);throw;}
#endif
  }
  AssetDirectoryAnchor(const AssetDirectoryAnchor&)=delete;
  ~AssetDirectoryAnchor(){
#ifdef _WIN32
    for(auto h:Handles)CloseHandle(h);
#else
    if(Descriptor>=0)close(Descriptor);
#endif
  }
};
struct ConfinedAssetPolicy {std::shared_ptr<AssetDirectoryAnchor> Root;std::string Relative;};
inline std::map<std::string,ConfinedAssetPolicy>& ConfinedAssetPolicies(){static std::map<std::string,ConfinedAssetPolicy> entries;return entries;}
inline std::map<std::string,std::weak_ptr<AssetDirectoryAnchor>>& AssetRootAnchors(){static std::map<std::string,std::weak_ptr<AssetDirectoryAnchor>> roots;return roots;}
inline std::shared_ptr<AssetDirectoryAnchor> PinAssetRoot(const std::string& root){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());auto& cache=AssetRootAnchors();auto existing=cache[root].lock();if(existing)return existing;auto result=std::make_shared<AssetDirectoryAnchor>(root);cache[root]=result;return result;}
inline void RegisterConfinedAssetPath(const std::string& full,const std::string& root){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
  if(full.compare(0,sizeof("/__lunoryx_bundle__/")-1,"/__lunoryx_bundle__/")==0)return;
  auto& entries=ConfinedAssetPolicies();if(entries.size()>=65536&&!entries.count(full))throw std::runtime_error("Asset path policy budget exceeded");
  std::string base=root;if(base.back()!='/')base+='/';entries[full]={PinAssetRoot(root),full==root?std::string():full.substr(base.size())};
}
struct AssetParentAnchor {
  std::shared_ptr<AssetDirectoryAnchor> Root;std::string Path,Name;
#ifdef _WIN32
  std::vector<HANDLE> Handles;
#else
  int Descriptor=-1;
#endif
  explicit AssetParentAnchor(const std::string& target){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
    auto policy=ConfinedAssetPolicies().find(target);std::string relative;
    if(policy!=ConfinedAssetPolicies().end()){Root=policy->second.Root;relative=policy->second.Relative;}
    else {auto absolute=AbsoluteAssetPath(target);Root=PinAssetRoot(CanonicalAssetPath(PathDirectory(absolute)));relative=absolute.substr(absolute.find_last_of('/')+1);}
    if(relative.empty())throw std::runtime_error("Expected an asset filename");
    auto slash=relative.find_last_of('/');Name=slash==std::string::npos?relative:relative.substr(slash+1);std::string parents=slash==std::string::npos?std::string():relative.substr(0,slash);Path=Root->Path;
    if(Name.empty()||Name=="."||Name=="..")throw std::runtime_error("Invalid asset filename");
#ifndef _WIN32
    int fd=dup(Root->Descriptor);if(fd<0)throw std::runtime_error("Cannot duplicate asset root");fcntl(fd,F_SETFD,FD_CLOEXEC);
#endif
    try{size_t at=0;while(at<parents.size()){size_t end=parents.find('/',at);if(end==std::string::npos)end=parents.size();auto part=parents.substr(at,end-at);at=end+1;if(part.empty()||part=="."||part=="..")throw std::runtime_error("Invalid asset ancestor");if(Path.back()!='/')Path+='/';Path+=part;
#ifdef _WIN32
      HANDLE h=CreateFileW(WidePath(Path).c_str(),FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Asset ancestor changed");BY_HANDLE_FILE_INFORMATION info={};if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){CloseHandle(h);throw std::runtime_error("Asset ancestor is not a plain directory");}Handles.push_back(h);
#else
      int child=openat(fd,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(child<0)throw std::runtime_error("Asset ancestor changed or is a symlink");close(fd);fd=child;
#endif
    }
#ifndef _WIN32
    Descriptor=fd;
#endif
    }catch(...){
#ifdef _WIN32
      for(auto h:Handles)CloseHandle(h);
      Handles.clear();
#else
      close(fd);
#endif
      throw;}
  }
  AssetParentAnchor(const AssetParentAnchor&)=delete;
  ~AssetParentAnchor(){
#ifdef _WIN32
    for(auto h:Handles)CloseHandle(h);
#else
    if(Descriptor>=0)close(Descriptor);
#endif
  }
  std::string Full(const std::string& name)const{return Path+(Path.back()=='/'?"":"/")+name;}
};
inline FILE* SecureOpenIfConfined(const std::string& path,const char* mode,bool& handled){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
  handled=ConfinedAssetPolicies().count(path)!=0;if(!handled)return nullptr;
  if(mode[0]!='r'||std::strchr(mode,'+'))throw std::runtime_error("Confined writes require atomic output");
  AssetParentAnchor parent(path);
#ifdef _WIN32
  HANDLE handle=CreateFileW(WidePath(parent.Full(parent.Name)).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(handle==INVALID_HANDLE_VALUE)return nullptr;BY_HANDLE_FILE_INFORMATION info={};if(GetFileType(handle)!=FILE_TYPE_DISK||!GetFileInformationByHandle(handle,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){CloseHandle(handle);throw std::runtime_error("Expected a regular asset file");}int fd=_open_osfhandle(reinterpret_cast<intptr_t>(handle),_O_RDONLY|_O_BINARY);if(fd<0){CloseHandle(handle);return nullptr;}FILE* file=_fdopen(fd,"rb");if(!file)_close(fd);return file;
#else
  int fd=openat(parent.Descriptor,parent.Name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return nullptr;struct stat info={};if(fstat(fd,&info)||!S_ISREG(info.st_mode)){close(fd);throw std::runtime_error("Expected a regular asset file");}FILE* file=fdopen(fd,"rb");if(!file)close(fd);return file;
#endif
}
struct TemporaryAssetOperation {std::shared_ptr<AssetParentAnchor> Parent;std::string Temporary,Destination;};
inline std::map<std::string,TemporaryAssetOperation>& TemporaryAssetOperations(){static std::map<std::string,TemporaryAssetOperation> values;return values;}
inline FILE* SecureTemporaryAsset(const std::string& destination,std::string& temporary){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
  auto parent=std::make_shared<AssetParentAnchor>(AbsoluteAssetPath(destination));std::random_device random;
  for(int attempt=0;attempt<16;++attempt){auto name=parent->Name+"."+std::to_string(random())+"."+std::to_string(random())+".tmp";
#ifdef _WIN32
    HANDLE handle=CreateFileW(WidePath(parent->Full(name)).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)continue;int fd=_open_osfhandle(reinterpret_cast<intptr_t>(handle),_O_WRONLY|_O_BINARY);if(fd<0){CloseHandle(handle);DeleteFileW(WidePath(parent->Full(name)).c_str());throw std::runtime_error("Cannot attach temporary output");}FILE* file=_fdopen(fd,"wb");if(!file){_close(fd);DeleteFileW(WidePath(parent->Full(name)).c_str());throw std::runtime_error("Cannot buffer temporary output");}
#else
    int fd=openat(parent->Descriptor,name.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)continue;FILE* file=fdopen(fd,"wb");if(!file){close(fd);unlinkat(parent->Descriptor,name.c_str(),0);throw std::runtime_error("Cannot buffer temporary output");}
#endif
    temporary=parent->Full(name);TemporaryAssetOperations()[temporary]={parent,name,AbsoluteAssetPath(destination)};return file;
  }throw std::runtime_error("Cannot exclusively create temporary output");
}
inline bool SecureRemoveAsset(const std::string& path){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());auto& values=TemporaryAssetOperations();auto it=values.find(path);if(it==values.end())return false;
#ifdef _WIN32
  DeleteFileW(WidePath(it->second.Parent->Full(it->second.Temporary)).c_str());
#else
  unlinkat(it->second.Parent->Descriptor,it->second.Temporary.c_str(),0);
#endif
  values.erase(it);return true;
}
inline bool SecureReplaceAsset(const std::string& from,const std::string& to){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());auto& values=TemporaryAssetOperations();auto it=values.find(from);if(it==values.end())return false;auto& operation=it->second;if(AbsoluteAssetPath(to)!=operation.Destination)throw std::runtime_error("Temporary output destination changed");
#ifdef _WIN32
  if(!MoveFileExW(WidePath(operation.Parent->Full(operation.Temporary)).c_str(),WidePath(operation.Parent->Full(operation.Parent->Name)).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot atomically replace output (Windows error "+std::to_string(GetLastError())+")");
#else
  if(renameat(operation.Parent->Descriptor,operation.Temporary.c_str(),operation.Parent->Descriptor,operation.Parent->Name.c_str()))throw std::runtime_error("Cannot atomically replace output (Windows error "+std::to_string(GetLastError())+")");
#endif
  values.erase(it);return true;
}
inline void MakeAssetExecutable(const std::string& path){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
#ifndef _WIN32
  auto it=TemporaryAssetOperations().find(path);if(it==TemporaryAssetOperations().end())throw std::runtime_error("Executable output is not anchored");int fd=openat(it->second.Parent->Descriptor,it->second.Temporary.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)throw std::runtime_error("Cannot reopen executable output");int result=fchmod(fd,0755);close(fd);if(result)throw std::runtime_error("Cannot make output executable");
#else
  (void)path;
#endif
}
inline void CreateAssetDirectories(const std::string& root,const std::string& relative){
  auto pinned=PinAssetRoot(AbsoluteAssetPath(root));std::string current=pinned->Path;
#ifdef _WIN32
  std::vector<HANDLE> locks;
#else
  int fd=dup(pinned->Descriptor);if(fd<0)throw std::runtime_error("Cannot pin project directory");
#endif
  try{size_t at=0;while(at<relative.size()){size_t end=relative.find('/',at);if(end==std::string::npos)end=relative.size();auto part=relative.substr(at,end-at);at=end+1;if(part.empty()||part=="."||part==".."||part.find(':')!=std::string::npos||part.find('\\')!=std::string::npos)throw std::runtime_error("Invalid project directory component");current+="/"+part;
#ifdef _WIN32
    if(!CreateDirectoryW(WidePath(current).c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("Cannot create project directory");
    HANDLE handle=CreateFileW(WidePath(current).c_str(),FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot pin new project directory");BY_HANDLE_FILE_INFORMATION info={};if(!GetFileInformationByHandle(handle,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){CloseHandle(handle);throw std::runtime_error("Project directory is a reparse point");}locks.push_back(handle);
#else
    if(mkdirat(fd,part.c_str(),0700)&&errno!=EEXIST)throw std::runtime_error("Cannot create project directory");
    int child=openat(fd,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(child<0)throw std::runtime_error("Project directory is a symlink");close(fd);fd=child;
#endif
  }}catch(...){
#ifdef _WIN32
    for(auto handle:locks)CloseHandle(handle);
#else
    close(fd);
#endif
    throw;}
#ifdef _WIN32
  for(auto handle:locks)CloseHandle(handle);
#else
  close(fd);
#endif
}

#if defined(__GNUC__) && defined(_WIN32)
#pragma GCC diagnostic pop
#endif

inline std::string SiblingAssetPath(const std::string& path,const std::string& suffix){
  std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());
  if(suffix.find_first_of("/\\:")!=std::string::npos)throw std::runtime_error("Invalid sibling suffix");
  auto original=AbsoluteAssetPath(path);auto sibling=original+suffix;auto found=ConfinedAssetPolicies().find(original);
  if(found!=ConfinedAssetPolicies().end()){
    if(ConfinedAssetPolicies().size()>=65536&&!ConfinedAssetPolicies().count(sibling))throw std::runtime_error("Asset policy budget exceeded");
    ConfinedAssetPolicies()[sibling]={found->second.Root,found->second.Relative+suffix};
  }
  return sibling;
}
