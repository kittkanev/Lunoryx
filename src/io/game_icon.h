#pragma once
static std::vector<unsigned char> ValidateGameIcon(std::vector<unsigned char> bytes) {
  if(bytes.empty())return {};
  int w=0,h=0,c=0;
  const unsigned char signature[]={137,80,78,71,13,10,26,10};
  if(bytes.size()<8||std::memcmp(bytes.data(),signature,8)||!stbi_info_from_memory(bytes.data(),int(bytes.size()),&w,&h,&c)||w<16||h<16||w>256||h>256||w!=h)
    throw std::runtime_error("Choose a square PNG icon, 16 to 256 pixels (recommended 256x256)");
  auto* decoded=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&c,4);
  if(!decoded)throw std::runtime_error("Cannot decode game icon PNG");
  stbi_image_free(decoded);return bytes;
}
static std::vector<unsigned char> ReadGameIcon(const std::string& path) {return path.empty()?std::vector<unsigned char>():ValidateGameIcon(ReadAssetFile(path,4*1024*1024));}
#ifdef _WIN32
#include <sddl.h>
// Resource update APIs require a pathname. Stage only in an owner-only
// directory under the current user profile.
struct PrivateIconRuntime {
  std::string Path;std::wstring Directory;HANDLE Lock=INVALID_HANDLE_VALUE;
  PrivateIconRuntime(){
    wchar_t base[32768];DWORD n=GetTempPathW(32768,base);if(!n||n>=32768)throw std::runtime_error("Cannot find private icon staging directory");
    PSECURITY_DESCRIPTOR descriptor=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)(A;;FA;;;SY)(A;;FA;;;BA)",SDDL_REVISION_1,&descriptor,nullptr))throw std::runtime_error("Cannot protect icon staging directory");
    SECURITY_ATTRIBUTES security={sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};std::random_device random;
    for(int attempt=0;attempt<16;++attempt){Directory=std::wstring(base)+L"Lunoryx-icon-"+std::to_wstring(random())+L"-"+std::to_wstring(random());if(CreateDirectoryW(Directory.c_str(),&security))break;Directory.clear();}
    LocalFree(descriptor);if(Directory.empty())throw std::runtime_error("Cannot create private icon staging directory");
    Lock=CreateFileW(Directory.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(Lock==INVALID_HANDLE_VALUE){RemoveDirectoryW(Directory.c_str());throw std::runtime_error("Cannot pin private icon staging directory");}
    auto wide=Directory+L"\\runtime.exe";int count=WideCharToMultiByte(CP_UTF8,0,wide.data(),int(wide.size()),nullptr,0,nullptr,nullptr);Path.resize(size_t(count));WideCharToMultiByte(CP_UTF8,0,wide.data(),int(wide.size()),&Path[0],count,nullptr,nullptr);
  }
  ~PrivateIconRuntime(){if(!Path.empty())DeleteFileW(WidePath(Path).c_str());if(Lock!=INVALID_HANDLE_VALUE)CloseHandle(Lock);if(!Directory.empty())RemoveDirectoryW(Directory.c_str());}
};
struct GameIconResource { WORD Type,Language;bool Integer;WORD Id;std::wstring Name; };
static BOOL CALLBACK GameIconLanguage(HMODULE, LPCWSTR type,LPCWSTR name,WORD language,LONG_PTR data) {
  auto* list=reinterpret_cast<std::vector<GameIconResource>*>(data);GameIconResource r={WORD(reinterpret_cast<ULONG_PTR>(type)),language,IS_INTRESOURCE(name)!=0,0,L""};
  if(r.Integer)r.Id=WORD(reinterpret_cast<ULONG_PTR>(name));else r.Name=name;list->push_back(r);return TRUE;
}
static BOOL CALLBACK GameIconName(HMODULE module,LPCWSTR type,LPWSTR name,LONG_PTR data) {
  return EnumResourceLanguagesW(module,type,name,GameIconLanguage,data);
}
static void ReplaceGameExeIcon(const std::string& path,const std::vector<unsigned char>& png) {
  if(png.empty())return;
  const auto wide=WidePath(path);
  HMODULE module=LoadLibraryExW(wide.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  if(!module)throw std::runtime_error("Cannot inspect EXE icons");
  std::vector<GameIconResource> old;
  EnumResourceNamesW(module,MAKEINTRESOURCEW(3),GameIconName,reinterpret_cast<LONG_PTR>(&old));
  EnumResourceNamesW(module,MAKEINTRESOURCEW(14),GameIconName,reinterpret_cast<LONG_PTR>(&old));FreeLibrary(module);
  HANDLE update=BeginUpdateResourceW(wide.c_str(),FALSE);if(!update)throw std::runtime_error("Cannot update EXE icon resources");
  bool ok=true;
  for(const auto& r:old)if(!UpdateResourceW(update,MAKEINTRESOURCEW(r.Type),r.Integer?MAKEINTRESOURCEW(r.Id):r.Name.c_str(),r.Language,nullptr,0))ok=false;
  if(!png.empty()){
    int w=0,h=0,c=0;stbi_info_from_memory(png.data(),int(png.size()),&w,&h,&c);
    unsigned char group[20]={0,0,1,0,1,0,static_cast<unsigned char>(w==256?0:w),static_cast<unsigned char>(h==256?0:h),0,0,1,0,32,0,0,0,0,0,1,0};
    uint32_t size=uint32_t(png.size());for(int k=0;k<4;++k)group[14+k]=static_cast<unsigned char>(size>>(8*k));
    if(!UpdateResourceW(update,MAKEINTRESOURCEW(3),MAKEINTRESOURCEW(1),0,const_cast<unsigned char*>(png.data()),DWORD(png.size()))||!UpdateResourceW(update,MAKEINTRESOURCEW(14),MAKEINTRESOURCEW(101),0,group,sizeof(group)))ok=false;
  }
  if(!ok){EndUpdateResourceW(update,TRUE);throw std::runtime_error("Cannot write game icon resources");}
  if(!EndUpdateResourceW(update,FALSE))throw std::runtime_error("Cannot finish game icon update");
}
#endif
