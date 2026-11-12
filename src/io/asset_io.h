#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
inline std::wstring WidePath(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                              int(s.size()), nullptr, 0);
  if (!n)
    throw std::runtime_error("Invalid UTF-8 path");
  std::wstring w(size_t(n), L' ');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()),
                      &w[0], n);
  if(w.size()>240&&w.size()>2&&w[1]==L':'&&(w[2]==L'/'||w[2]==L'\\')){
    std::replace(w.begin(),w.end(),L'/',L'\\');w=L"\\\\?\\"+w;
  }
  return w;
}
#else
#include <unistd.h>
#include <fcntl.h>
#endif
inline std::map<std::string, std::vector<unsigned char>> &VirtualAssets() {
  static std::map<std::string, std::vector<unsigned char>> assets;
  return assets;
}
inline FILE* SecureOpenIfConfined(const std::string&,const char*,bool&);
inline bool SecureRemoveAsset(const std::string&);
inline bool SecureReplaceAsset(const std::string&,const std::string&);
inline FILE* SecureTemporaryAsset(const std::string&,std::string&);
inline FILE *AssetOpen(const std::string &path, const char *mode) {
  if (path.empty() || path.find('\0') != std::string::npos)
    return nullptr;
  if(path.compare(0,sizeof("/__lunoryx_bundle__")-1,"/__lunoryx_bundle__")==0)return nullptr;
  bool confined=false;FILE* secured=SecureOpenIfConfined(path,mode,confined);if(confined)return secured;
#ifdef _WIN32
  std::wstring m;
  for (const char *c = mode; *c; ++c)
    m += wchar_t(*c);
  return _wfopen(WidePath(path).c_str(), m.c_str());
#else
  return std::fopen(path.c_str(), mode);
#endif
}
inline std::string NormalizePath(std::string p) {
  std::replace(p.begin(), p.end(), '\\', '/');
  std::string prefix;
  size_t start = 0;
  if (p.size() > 1 && p[1] == ':') {
    prefix = p.substr(0, 2);
    start = 2;
  }
  bool rooted = start < p.size() && p[start] == '/';
  if (rooted) {
    prefix += '/';
    ++start;
  }
  std::vector<std::string> parts;
  while (start < p.size()) {
    size_t end = p.find('/', start);
    if (end == std::string::npos)
      end = p.size();
    auto part = p.substr(start, end - start);
    start = end + 1;
    if (part.empty() || part == ".")
      continue;
    if (part == ".." && !parts.empty() && parts.back() != "..")
      parts.pop_back();
    else if (part != ".." || !rooted)
      parts.push_back(part);
  }
  std::string out = prefix;
  for (const auto &part : parts) {
    if (!out.empty() && out.back() != '/')
      out += '/';
    out += part;
  }
  return out.empty() ? "." : out;
}
inline bool AbsolutePath(const std::string &p) {
  return !p.empty() &&
         (p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':'));
}
inline std::string AbsoluteAssetPath(const std::string &p) {
  if (AbsolutePath(p))
    return NormalizePath(p);
#ifdef _WIN32
  wchar_t cwd[32768];
  if (!_wgetcwd(cwd, 32768))
    throw std::runtime_error("Cannot read working directory");
  int n =
      WideCharToMultiByte(CP_UTF8, 0, cwd, -1, nullptr, 0, nullptr, nullptr);
  std::string base(size_t(n), ' ');
  WideCharToMultiByte(CP_UTF8, 0, cwd, -1, &base[0], n, nullptr, nullptr);
  base.resize(size_t(n - 1));
#else
  char cwd[4096];
  if (!getcwd(cwd, sizeof(cwd)))
    throw std::runtime_error("Cannot read working directory");
  std::string base = cwd;
#endif
  return NormalizePath(base + "/" + p);
}
inline std::string PathDirectory(const std::string &p) {
  size_t i = p.find_last_of("/\\");
  if(i==0)return p.substr(0,1);
  if(i==2&&p.size()>2&&p[1]==':')return p.substr(0,3);
  return i == std::string::npos ? "." : p.substr(0, i);
}
inline std::string RelativeAssetPath(const std::string &asset,
                                     const std::string &project) {
  std::string a = AbsoluteAssetPath(asset),
              b = PathDirectory(AbsoluteAssetPath(project));
  if (a.size() > 1 && b.size() > 1 && a[1] == ':' &&
      a.substr(0, 2) != b.substr(0, 2))
    return a;
  size_t common = 0;
  while (common < a.size() && common < b.size() && a[common] == b[common])
    ++common;
  if (common == b.size() && common < a.size() && a[common] == '/')
    return a.substr(common + 1);
  while (common && a[common - 1] != '/')
    --common;
  std::string result;
  for (size_t k = common; k < b.size(); ++k)
    if (b[k] == '/')
      result += "../";
  if (common < b.size())
    result += "../";
  return result + a.substr(common);
}
inline std::vector<unsigned char>
ReadAssetFile(const std::string &path, size_t limit = 256 * 1024 * 1024) {
  auto embedded = VirtualAssets().find(NormalizePath(path));
  if (embedded != VirtualAssets().end()) {
    if (embedded->second.size() > limit)
      throw std::runtime_error("Resource exceeds size limit");
    return embedded->second;
  }
  FILE *f = AssetOpen(path, "rb");
  if (!f)
    throw std::runtime_error("Cannot open resource: " + path);
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    throw std::runtime_error("Cannot seek resource");
  }
  long size = std::ftell(f);
  if (size < 0 || size_t(size) > limit) {
    std::fclose(f);
    throw std::runtime_error("Resource exceeds size limit");
  }
  std::rewind(f);
  std::vector<unsigned char> data(size_t(size), 0);
  bool ok =
      data.empty() || std::fread(data.data(), 1, data.size(), f) == data.size();
  std::fclose(f);
  if (!ok)
    throw std::runtime_error("Cannot read resource");
  return data;
}
inline bool AssetExists(const std::string &path) {
  if (VirtualAssets().count(NormalizePath(path)))
    return true;
  try { FILE *f = AssetOpen(path, "rb");if(!f)return false;std::fclose(f);return true; } catch(...) { return false; }
}
inline void WriteAssetFile(const std::string &path, const void *data,
                           size_t n) {
  FILE *f = AssetOpen(path, "wb");
  if (!f)
    throw std::runtime_error("Cannot write file: " + path);
  bool ok = std::fwrite(data, 1, n, f) == n;
  if (std::fclose(f) != 0)
    ok = false;
  if (!ok)
    throw std::runtime_error("Cannot finish writing file: " + path);
}
inline void ReplaceAssetFile(const std::string &from, const std::string &to) {
  if(SecureReplaceAsset(from,to))return;
#ifdef _WIN32
  if (!MoveFileExW(WidePath(from).c_str(), WidePath(to).c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Cannot replace file");
#else
  if (std::rename(from.c_str(), to.c_str()))
    throw std::runtime_error("Cannot replace file");
#endif
}

inline void RemoveAssetFile(const std::string& path) {
  if(SecureRemoveAsset(path))return;
#ifdef _WIN32
  _wremove(WidePath(path).c_str());
#else
  std::remove(path.c_str());
#endif
}
// Reject network/device paths before performing any filesystem operation.
inline bool HasEmbeddedNullOrControl(const std::string& value) {
  for(unsigned char byte:value)if(byte<32||byte==127)return true;
  return false;
}
inline void CheckLocalAssetPath(const std::string& path) {
  if(path.empty()||HasEmbeddedNullOrControl(path))throw std::runtime_error("Invalid local path");
  if(path.size()>1&&path[1]==':'&&(path.size()<3||(path[2]!='/'&&path[2]!='\\')))throw std::runtime_error("Drive-relative path");
  std::string p=path;std::replace(p.begin(),p.end(),'\\','/');
  if(p.compare(0,2,"//")==0)throw std::runtime_error("Network and device paths are not allowed");
#ifdef _WIN32
  for(size_t at=0;at<p.size();){size_t end=p.find('/',at);if(end==std::string::npos)end=p.size();auto part=p.substr(at,end-at);at=end+1;
    if(part.size()==2&&part[1]==':'&&part[0]>='A'&&part[0]<='z'&&at==3)continue;
    if(part.find(':')!=std::string::npos||(!part.empty()&&(part.back()==' '||part.back()=='.')&&part!="."&&part!=".."))throw std::runtime_error("Device or ambiguous path");
    part=part.substr(0,part.find('.'));for(char& c:part)if(c>='a'&&c<='z')c=char(c-'a'+'A');
    if(part=="CON"||part=="PRN"||part=="AUX"||part=="NUL"||part=="CONIN$"||part=="CONOUT$"||(part.size()==4&&(part.compare(0,3,"COM")==0||part.compare(0,3,"LPT")==0)&&part[3]>='0'&&part[3]<='9'))throw std::runtime_error("Device path");
  }
#endif
}
inline std::string CanonicalAssetPath(const std::string& path) {
  CheckLocalAssetPath(path);
  std::string full=AbsoluteAssetPath(path),parent=full,suffix;
  if(full=="/__lunoryx_bundle__")return full;
  if(full.compare(0,sizeof("/__lunoryx_bundle__/")-1,"/__lunoryx_bundle__/")==0){if(VirtualAssets().count(full)||full=="/__lunoryx_bundle__")return full;throw std::runtime_error("Missing embedded asset");}
  for(;;){
#ifdef _WIN32
    HANDLE handle=CreateFileW(WidePath(parent).c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(handle!=INVALID_HANDLE_VALUE){wchar_t result[32768];DWORD n=GetFinalPathNameByHandleW(handle,result,32768,FILE_NAME_NORMALIZED);CloseHandle(handle);if(!n||n>=32768)throw std::runtime_error("Cannot resolve path");
      std::wstring w(result,n);if(w.compare(0,8,L"\\\\?\\UNC\\")==0)throw std::runtime_error("Network path");if(w.compare(0,4,L"\\\\?\\")==0)w.erase(0,4);
      int count=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string out(size_t(count),' ');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),&out[0],count,nullptr,nullptr);return NormalizePath(out+suffix);}
#else
    char resolved[4096];if(realpath(parent.c_str(),resolved))return NormalizePath(std::string(resolved)+suffix);
#endif
    std::string next=PathDirectory(parent);if(next==parent||parent.empty())throw std::runtime_error("Cannot resolve path");
    suffix="/"+parent.substr(parent.find_last_of('/')+1)+suffix;parent=next.empty()?"/":next;
  }
}
#include "src/io/secure_asset_io.h"
inline std::string ConfinedAssetPath(const std::string& path,const std::string& root) {
  CheckLocalAssetPath(path);
  auto absoluteRoot=AbsoluteAssetPath(root);std::shared_ptr<AssetDirectoryAnchor> existing;
  {std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());auto known=AssetRootAnchors().find(absoluteRoot);if(known!=AssetRootAnchors().end())existing=known->second.lock();}
  auto base=existing?existing->Path:CanonicalAssetPath(root);
  auto full=CanonicalAssetPath(AbsolutePath(path)?path:base+"/"+path);
  auto compareBase=base,compareFull=full;
#ifdef _WIN32
  for(char& c:compareBase)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
  for(char& c:compareFull)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
#endif
  if(compareBase.back()!='/')compareBase+='/';
  if(compareFull!=compareBase.substr(0,compareBase.size()-1)&&compareFull.compare(0,compareBase.size(),compareBase)!=0)throw std::runtime_error("Path is outside the allowed directory");
  RegisterConfinedAssetPath(full,base);
  return full;
}
inline std::string WorkspaceAssetPath(const std::string& path) {
  static const std::string workspace=CanonicalAssetPath(".");return ConfinedAssetPath(path,workspace);
}
inline std::string& ScriptAssetRoot(){static std::string root;return root;}
inline std::string ScriptAssetPath(const std::string& path) {
  return ScriptAssetRoot().empty()?WorkspaceAssetPath(path):ConfinedAssetPath(path,ScriptAssetRoot());
}
inline std::string ProjectAssetPath(const std::string& path,const std::string& project) {
  return path.empty()?path:ConfinedAssetPath(path,PathDirectory(AbsoluteAssetPath(project)));
}

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif
#include <random>
inline FILE* OpenTemporaryAsset(const std::string& destination,std::string& temporary) {return SecureTemporaryAsset(destination,temporary);}
inline void WriteAssetFileAtomic(const std::string& destination,const void* data,size_t size) {
  std::string temporary;FILE* file=OpenTemporaryAsset(destination,temporary);
  if(!file)throw std::runtime_error("Cannot open temporary output");
  bool ok=std::fwrite(data,1,size,file)==size;if(std::fclose(file))ok=false;
  try{if(!ok)throw std::runtime_error("Cannot finish output");ReplaceAssetFile(temporary,destination);}catch(...){RemoveAssetFile(temporary);throw;}
}
inline bool SameAssetFile(const std::string& a,const std::string& b) {
#ifdef _WIN32
  HANDLE left=CreateFileW(WidePath(a).c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
  HANDLE right=CreateFileW(WidePath(b).c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
  BY_HANDLE_FILE_INFORMATION x={},y={};bool same=left!=INVALID_HANDLE_VALUE&&right!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(left,&x)&&GetFileInformationByHandle(right,&y)&&x.dwVolumeSerialNumber==y.dwVolumeSerialNumber&&x.nFileIndexHigh==y.nFileIndexHigh&&x.nFileIndexLow==y.nFileIndexLow;
  if(left!=INVALID_HANDLE_VALUE)CloseHandle(left);
  if(right!=INVALID_HANDLE_VALUE)CloseHandle(right);
  return same;
#else
  struct stat x={},y={};return stat(a.c_str(),&x)==0&&stat(b.c_str(),&y)==0&&x.st_dev==y.st_dev&&x.st_ino==y.st_ino;
#endif
}
