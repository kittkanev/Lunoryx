#pragma once
#include "src/io/asset_io.h"
#include "src/io/hmac_sha256.h"
#include <cstdlib>
#ifdef _WIN32
#include <wincrypt.h>
#include <sddl.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <sys/random.h>
#endif
#endif
namespace PackageAuth {
inline void RejectKeyPath(const std::string& path);
struct Key {
  std::array<unsigned char,32> Bytes={};
  ~Key(){EraseSecret(Bytes.data(),Bytes.size());}
};
inline std::string& KeyFileStorage() {
  static std::string path=[](){const char* value=std::getenv("LUNORYX_PACKAGE_KEY_FILE");return value?std::string(value):std::string();}();
  return path;
}
inline std::string KeyFile(){std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());AssetWriteGuard().store(&RejectKeyPath);return KeyFileStorage();}
inline void Configure(std::string path){if(!path.empty())CheckLocalAssetPath(path);std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());AssetWriteGuard().store(&RejectKeyPath);KeyFileStorage()=std::move(path);}
inline void RejectKeyPath(const std::string& path) {
  CheckLocalAssetPath(path);
  const auto selected=KeyFile();
  if(!selected.empty()){
    CheckLocalAssetPath(selected);
    if(SameAssetFile(path,selected)||CanonicalAssetPath(path)==CanonicalAssetPath(selected))
      throw std::runtime_error("The active package key is protected; choose a different resource or output path");
  }
}
struct RejectEmbeddedKey {
  const Key& Secret;std::array<unsigned char,31> Tail={};size_t Used=0;
  explicit RejectEmbeddedKey(const Key& key):Secret(key){}
  ~RejectEmbeddedKey(){EraseSecret(Tail.data(),Tail.size());}
  void Add(const void* pointer,size_t size) {
    const auto* bytes=static_cast<const unsigned char*>(pointer);
    if(!size)return;
    auto contains=[&](const unsigned char* data,size_t n){return n>=Secret.Bytes.size()&&std::search(data,data+n,Secret.Bytes.begin(),Secret.Bytes.end())!=data+n;};
    std::array<unsigned char,62> edge={};size_t prefix=std::min(size,Tail.size());
    std::copy(Tail.begin(),Tail.begin()+Used,edge.begin());std::copy(bytes,bytes+prefix,edge.begin()+Used);
    bool found=contains(bytes,size)||contains(edge.data(),Used+prefix);
    if(size>=Tail.size()){std::copy(bytes+size-Tail.size(),bytes+size,Tail.begin());Used=Tail.size();}
    else {size_t keep=std::min(Tail.size(),Used+size);std::copy(edge.begin()+Used+prefix-keep,edge.begin()+Used+prefix,Tail.begin());Used=keep;}
    EraseSecret(edge.data(),edge.size());
    if(found)throw std::runtime_error("Package input contains the active authentication key; remove copied keys from project/runtime resources");
  }
};
inline Key LoadKey() {
  const auto selected=KeyFile();
  if(selected.empty())throw std::runtime_error("Choose an external package key file (or set LUNORYX_PACKAGE_KEY_FILE)");
  CheckLocalAssetPath(selected);
  auto path=AbsoluteAssetPath(selected);auto root=CanonicalAssetPath(PathDirectory(path));
  path=ConfinedAssetPath(path,root);
  std::unique_ptr<FILE,int(*)(FILE*)> file(AssetOpen(path,"rb"),std::fclose);
  if(!file)throw std::runtime_error("Cannot open package key file");
#ifndef _WIN32
  struct stat info={};if(fstat(fileno(file.get()),&info)||!S_ISREG(info.st_mode)||(info.st_mode&0077))throw std::runtime_error("Package key must be an owner-only regular file (chmod 600)");
#endif
  Key key;if(std::fread(key.Bytes.data(),1,key.Bytes.size(),file.get())!=key.Bytes.size()||std::fgetc(file.get())!=EOF||std::ferror(file.get()))throw std::runtime_error("Package key file must contain exactly 32 raw bytes");
  return key;
}
inline void GenerateKeyFile(const std::string& path) {
  CheckLocalAssetPath(path);Key key;
#ifdef _WIN32
  HCRYPTPROV provider=0;
  if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT))throw std::runtime_error("System random provider is unavailable");
  BOOL generated=CryptGenRandom(provider,DWORD(key.Bytes.size()),key.Bytes.data());CryptReleaseContext(provider,0);
  if(!generated)throw std::runtime_error("Cannot generate package key");
  AssetParentAnchor anchor(AbsoluteAssetPath(path));PSECURITY_DESCRIPTOR descriptor=nullptr;
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)(A;;FA;;;SY)(A;;FA;;;BA)",SDDL_REVISION_1,&descriptor,nullptr))throw std::runtime_error("Cannot protect package key");
  HANDLE file=RelativeWindowsOpen(anchor.Directory(),anchor.Name,GENERIC_WRITE|DELETE,2,0x40,0,descriptor);LocalFree(descriptor);
  if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot exclusively create key file; choose a new path");
  DWORD written=0;bool ok=WriteFile(file,key.Bytes.data(),DWORD(key.Bytes.size()),&written,nullptr)&&written==key.Bytes.size()&&FlushFileBuffers(file);
  if(!ok){FILE_DISPOSITION_INFO remove={TRUE};SetFileInformationByHandle(file,FileDispositionInfo,&remove,sizeof(remove));}
  CloseHandle(file);if(!ok)throw std::runtime_error("Cannot finish package key file");
#else
  if(getentropy(key.Bytes.data(),key.Bytes.size()))throw std::runtime_error("Cannot generate package key");
  AssetParentAnchor anchor(AbsoluteAssetPath(path));
  int file=openat(anchor.Descriptor,anchor.Name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
  if(file<0)throw std::runtime_error("Cannot exclusively create key file; choose a new path");
  size_t at=0;while(at<key.Bytes.size()){ssize_t n=write(file,key.Bytes.data()+at,key.Bytes.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=size_t(n);}
  bool ok=at==key.Bytes.size()&&fsync(file)==0;if(close(file))ok=false;
  if(!ok){unlinkat(anchor.Descriptor,anchor.Name.c_str(),0);throw std::runtime_error("Cannot finish package key file");}
#endif
}
} // namespace PackageAuth
