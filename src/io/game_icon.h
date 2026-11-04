#pragma once
static std::vector<unsigned char> ReadGameIcon(const std::string& path) {
  if(path.empty())return {};
  auto bytes=ReadAssetFile(path,4*1024*1024);int w=0,h=0,c=0;
  const unsigned char signature[]={137,80,78,71,13,10,26,10};
  if(bytes.size()<8||std::memcmp(bytes.data(),signature,8)||!stbi_info_from_memory(bytes.data(),int(bytes.size()),&w,&h,&c)||w<16||h<16||w>256||h>256||w!=h)
    throw std::runtime_error("Choose a square PNG icon, 16 to 256 pixels (recommended 256x256)");
  auto* decoded=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&c,4);
  if(!decoded)throw std::runtime_error("Cannot decode game icon PNG");
  stbi_image_free(decoded);return bytes;
}
#ifdef _WIN32
struct GameIconResource { WORD Type,Language;bool Integer;WORD Id;std::wstring Name; };
static BOOL CALLBACK GameIconLanguage(HMODULE, LPCWSTR type,LPCWSTR name,WORD language,LONG_PTR data) {
  auto* list=reinterpret_cast<std::vector<GameIconResource>*>(data);GameIconResource r={WORD(reinterpret_cast<ULONG_PTR>(type)),language,IS_INTRESOURCE(name)!=0,0,L""};
  if(r.Integer)r.Id=WORD(reinterpret_cast<ULONG_PTR>(name));else r.Name=name;list->push_back(r);return TRUE;
}
static BOOL CALLBACK GameIconName(HMODULE module,LPCWSTR type,LPWSTR name,LONG_PTR data) {
  return EnumResourceLanguagesW(module,type,name,GameIconLanguage,data);
}
static void ReplaceGameExeIcon(const std::string& path,const std::vector<unsigned char>& png) {
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
