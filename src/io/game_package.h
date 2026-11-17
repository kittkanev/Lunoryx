#pragma once
#include <array>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "src/io/game_icon.h"
#include "src/io/package_auth.h"
namespace GamePackage {
static const size_t MaxPayload = 1024ull * 1024 * 1024;
inline int SeekFile64(FILE* file,int64_t offset,int origin) {
#ifdef _WIN32
  return _fseeki64(file,offset,origin);
#else
  static_assert(sizeof(off_t)>=sizeof(int64_t),"64-bit file offsets are required");
  return fseeko(file,static_cast<off_t>(offset),origin);
#endif
}
inline int64_t TellFile64(FILE* file) {
#ifdef _WIN32
  return _ftelli64(file);
#else
  return static_cast<int64_t>(ftello(file));
#endif
}
inline void CheckResourceName(const std::string &name) {
  if (name.empty() || name == "." || name.size() > 511 ||
      name.find('\0') != std::string::npos || AbsolutePath(name) ||
      NormalizePath(name) != name || name == ".." ||
      name.find("../") != std::string::npos ||
      name.find(':') != std::string::npos)
    throw std::runtime_error("Invalid embedded resource path");
}
inline void Number(std::vector<unsigned char> &out, uint64_t n, int count) {
  for (int i = 0; i < count; ++i)
    out.push_back(static_cast<unsigned char>(n >> (i * 8)));
}
inline uint64_t NumberAt(const std::vector<unsigned char> &data, size_t offset,
                         int count) {
  if (offset > data.size() || size_t(count) > data.size() - offset)
    throw std::runtime_error("Truncated game container");
  uint64_t value = 0;
  for (int i = 0; i < count; ++i)
    value |= uint64_t(data[offset + i]) << (i * 8);
  return value;
}
inline uint32_t CRC(const unsigned char *data, size_t size) {
  // Portable IEEE CRC32, slicing by eight. Same container checksum as 1.0.1;
  // the benchmark's equivalent C++ kernel outperformed the Rust candidate.
  static const auto tables = []() {
    std::array<std::array<uint32_t, 256>, 8> t = {};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k)
        c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
      t[0][i] = c;
    }
    for (int k = 1; k < 8; ++k)
      for (int i = 0; i < 256; ++i) {
        auto c = t[k - 1][i];
        t[k][i] = (c >> 8) ^ t[0][c & 255];
      }
    return t;
  }();
  uint32_t crc = ~uint32_t(0);
  size_t at = 0;
  for (; size - at >= 8; at += 8) {
    uint32_t a = crc ^ uint32_t(data[at]) ^ (uint32_t(data[at + 1]) << 8) ^
                 (uint32_t(data[at + 2]) << 16) ^
                 (uint32_t(data[at + 3]) << 24);
    crc = tables[7][a & 255] ^ tables[6][(a >> 8) & 255] ^
          tables[5][(a >> 16) & 255] ^ tables[4][a >> 24] ^
          tables[3][data[at + 4]] ^ tables[2][data[at + 5]] ^
          tables[1][data[at + 6]] ^ tables[0][data[at + 7]];
  }
  for (; at < size; ++at)
    crc = (crc >> 8) ^ tables[0][(crc ^ data[at]) & 255];
  return ~crc;
}
inline std::string ExecutablePath() {
#ifdef _WIN32
  wchar_t path[32768];
  DWORD length = GetModuleFileNameW(nullptr, path, 32768);
  if (!length || length >= 32768)
    throw std::runtime_error("Cannot find runtime executable");
  int n = WideCharToMultiByte(CP_UTF8, 0, path, int(length), nullptr, 0,
                              nullptr, nullptr);
  std::string result(size_t(n), ' ');
  WideCharToMultiByte(CP_UTF8, 0, path, int(length), &result[0], n, nullptr,
                      nullptr);
  return result;
#elif defined(__APPLE__)
  uint32_t size=0;_NSGetExecutablePath(nullptr,&size);std::vector<char> path(size);
  if(_NSGetExecutablePath(path.data(),&size))throw std::runtime_error("Cannot find macOS executable");
  return CanonicalAssetPath(path.data());
#else
  char path[4096];
  ssize_t n = readlink("/proc/self/exe", path, sizeof(path));
  if (n <= 0 || size_t(n) >= sizeof(path))
    throw std::runtime_error("Cannot find runtime executable");
  return std::string(path, size_t(n));
#endif
}
struct StreamChecksum {
  uint32_t State=~uint32_t(0);
  void Add(const void* pointer,size_t n){static const auto table=[](){std::array<uint32_t,256> values={};for(uint32_t i=0;i<256;++i){uint32_t c=i;for(int k=0;k<8;++k)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));values[i]=c;}return values;}();auto bytes=static_cast<const unsigned char*>(pointer);for(size_t i=0;i<n;++i)State=(State>>8)^table[(State^bytes[i])&255];}
  uint32_t Value()const{return ~State;}
};
struct PackageFooter {
  std::vector<unsigned char> Bytes;
  uint64_t Position=0;
  bool Legacy=false;
};
template<class Reader> inline PackageFooter LocateFooter(uint64_t fileSize,Reader readAt) {
  PackageFooter result;if(fileSize<64)return result;
  uint64_t end=fileSize;
  auto dos=readAt(0,64);
  if(dos[0]=='M'&&dos[1]=='Z'){
    size_t pe=size_t(NumberAt(dos,60,4));
    if(pe<=1024*1024&&pe+24<=end){auto header=readAt(pe,24);size_t optional=size_t(NumberAt(header,20,2));
      if(!std::memcmp(header.data(),"PE\0\0",4)&&optional>=152&&pe+24+optional<=end){auto opt=readAt(pe+24,optional);size_t directories=NumberAt(opt,0,2)==0x20b?112:NumberAt(opt,0,2)==0x10b?96:0;
        if(directories&&NumberAt(opt,directories-4,4)>4&&directories+40<=opt.size()){
          size_t certificate=size_t(NumberAt(opt,directories+32,4)),certificateSize=size_t(NumberAt(opt,directories+36,4));
          if(certificate&&certificateSize>=8&&certificate<=end&&certificateSize==end-certificate&&certificate%8==0)end=certificate;
        }
      }
    }
  }
  for(size_t padding=0;padding<8;++padding){
    for(size_t width:{size_t(64),size_t(32)}){
      if(end<width+padding)continue;
      auto candidate=readAt(end-width-padding,width+padding);
      bool zeros=true;for(size_t i=width;i<candidate.size();++i)zeros=zeros&&candidate[i]==0;
      const char* magic=width==64?"LNXPCK02":"LNXPCK01";
      if(zeros&&!std::memcmp(candidate.data(),magic,8)){
        candidate.resize(width);result.Bytes=std::move(candidate);result.Position=end-width-padding;result.Legacy=width==32;return result;
      }
    }
  }
  return result;
}
inline void CheckBudgets(uint64_t entries,uint64_t manifest,uint64_t data,uint64_t runtime) {
  if(entries>4096)throw std::runtime_error("Game supports at most 4095 external resources");
  if(runtime>128ull*1024*1024)throw std::runtime_error("Runtime executable exceeds 128 MiB budget");
  if(manifest>4ull*1024*1024)throw std::runtime_error("Game manifest exceeds 4 MiB budget");
  if(data>MaxPayload||manifest>MaxPayload-data||8>MaxPayload-data-manifest)throw std::runtime_error("Game assets/manifest exceed 1 GiB budget");
}
inline std::vector<unsigned char> FooterHeader(uint64_t offset,uint64_t length) {
  std::vector<unsigned char> out={'L','N','X','P','C','K','0','2'};
  Number(out,offset,8);Number(out,length,8);Number(out,2,4);Number(out,0,4);return out;
}
inline void AuthenticateHeader(HmacSha256& mac,const std::vector<unsigned char>& header) {
  static const char domain[]="Lunoryx authenticated game package v2";
  if(header.size()<32)throw std::runtime_error("Invalid authentication header");
  mac.Add(domain,sizeof(domain));mac.Add(header.data(),32);
}
inline std::array<unsigned char,32> AuthenticationTag(const PackageAuth::Key& key,const std::vector<unsigned char>& header,const void* payload,size_t size) {
  HmacSha256 mac(key.Bytes.data(),key.Bytes.size());AuthenticateHeader(mac,header);mac.Add(payload,size);return mac.Finish();
}
template<class Reader> inline bool ReadContent(uint64_t fileSize,Reader readAt,std::map<std::string,std::vector<unsigned char>>& assets) {
  auto located=LocateFooter(fileSize,readAt);
  if(located.Bytes.empty())return false;
  if(located.Legacy)throw std::runtime_error("Unauthenticated legacy game package; rebuild it with an external package key");
  const auto& footerBytes=located.Bytes;uint64_t footer=located.Position;
  uint64_t offset=NumberAt(footerBytes,8,8),length=NumberAt(footerBytes,16,8);
  if(NumberAt(footerBytes,24,4)!=2||NumberAt(footerBytes,28,4)!=0||length<8||length>MaxPayload||offset>footer||length!=footer-offset)throw std::runtime_error("Unsupported or truncated game package");
  auto key=PackageAuth::LoadKey();HmacSha256 mac(key.Bytes.data(),key.Bytes.size());AuthenticateHeader(mac,footerBytes);
  const uint64_t payloadOffset=offset;
  auto sizeBytes=readAt(payloadOffset,8);uint64_t manifestSize=NumberAt(sizeBytes,0,8);
  if(manifestSize>4*1024*1024||manifestSize>length-8)throw std::runtime_error("Invalid game manifest length");
  auto manifestBytes=readAt(payloadOffset+8,size_t(manifestSize));
  mac.Add(sizeBytes.data(),sizeBytes.size());mac.Add(manifestBytes.data(),manifestBytes.size());
  // Manifest parsing is bounded pre-authentication. Only the exact captured
  // manifest/resource bytes enter the MAC; no authenticated file bytes are
  // reread into the committed scene (same-inode mutation is possible).
  CheckJsonBudget(manifestBytes.begin(),manifestBytes.end());
  nlohmann::json manifest=nlohmann::json::parse(manifestBytes);
  if (!manifest.is_array() || manifest.size() > 4096)
    throw std::runtime_error("Invalid game manifest");
  uint64_t base=payloadOffset+8+manifestSize;
  size_t available=size_t(length)-8-size_t(manifestSize);
  std::map<std::string, std::vector<unsigned char>> parsed;
  struct Resource {
    std::string Key;
    size_t Start, Size;
  };
  std::vector<Resource> resources;
  std::vector<std::pair<size_t, size_t>> ranges;
  std::set<std::string> names;
  for (const auto &entry : manifest) {
    std::string name = entry.at("name").get<std::string>();
    CheckResourceName(name);
    if (!entry.at("offset").is_number_unsigned() ||
        !entry.at("size").is_number_unsigned())
      throw std::runtime_error("Invalid embedded range");
    uint64_t start = entry.at("offset").get<uint64_t>(),
             size = entry.at("size").get<uint64_t>();
    if (start > available || size > available - start)
      throw std::runtime_error("Embedded resource exceeds payload");
    auto key = "/__lunoryx_bundle__/" + name;
    if (!names.insert(key).second)
      throw std::runtime_error("Duplicate embedded resource");
    resources.push_back({key, size_t(start), size_t(size)});
    if (size)
      ranges.push_back({size_t(start), size_t(start + size)});
  }
  if (!names.count("/__lunoryx_bundle__/project.lunoryx"))
    throw std::runtime_error("Game project is missing");
  // Validate the entire manifest before copying any payload. Aliased ranges
  // could otherwise amplify a bounded input into hundreds of allocations.
  std::sort(ranges.begin(), ranges.end());
  for (size_t i = 1; i < ranges.size(); ++i)
    if (ranges[i].first < ranges[i - 1].second)
      throw std::runtime_error("Overlapping embedded resource ranges");
  std::sort(resources.begin(),resources.end(),[](const Resource& a,const Resource& b){return a.Start<b.Start;});
  size_t position=0;
  auto hashGap=[&](size_t until){while(position<until){auto block=readAt(base+position,std::min(size_t(1024*1024),until-position));mac.Add(block.data(),block.size());position+=block.size();}};
  for(const auto& resource:resources){
    if(!resource.Size){parsed.emplace(resource.Key,std::vector<unsigned char>());continue;}
    hashGap(resource.Start);auto bytes=readAt(base+resource.Start,resource.Size);mac.Add(bytes.data(),bytes.size());position=resource.Start+resource.Size;parsed.emplace(resource.Key,std::move(bytes));
  }
  hashGap(available);
  auto actual=mac.Finish();if(!EqualAuthenticationTag(actual.data(),footerBytes.data()+32,actual.size()))throw std::runtime_error("Game package authentication failed");

  assets = std::move(parsed);
  return true;
}
inline bool Read(const std::string &executable,
                 std::map<std::string, std::vector<unsigned char>> &assets) {
  CheckLocalAssetPath(executable);
  std::unique_ptr<FILE,int(*)(FILE*)> file(AssetOpen(executable,"rb"),std::fclose);
  if(!file||SeekFile64(file.get(),0,SEEK_END))throw std::runtime_error("Cannot open executable");
  int64_t measured=TellFile64(file.get());if(measured<0)throw std::runtime_error("Cannot inspect executable size");
  uint64_t fileSize=uint64_t(measured);if(fileSize<64)return false;
  FILE* input=file.get();
  auto readAt=[input,fileSize](uint64_t at,size_t count){if(at>fileSize||uint64_t(count)>fileSize-at||SeekFile64(input,int64_t(at),SEEK_SET))throw std::runtime_error("Truncated executable");std::vector<unsigned char> out(count);if(std::fread(out.data(),1,count,input)!=count)throw std::runtime_error("Truncated executable");return out;};
  return ReadContent(fileSize,readAt,assets);
}
inline void Write(const std::string& runtime,const std::string& destination,const std::map<std::string,std::vector<unsigned char>>& assets,const std::vector<unsigned char>& icon={},bool rawPayload=false){
  CheckLocalAssetPath(runtime);CheckLocalAssetPath(destination);
  if(assets.size()>4096)throw std::runtime_error("Game supports at most 4095 external resources");
  if(!assets.count("project.lunoryx"))throw std::runtime_error("Game project is missing");
  for(const auto& asset:assets)CheckResourceName(asset.first);
  if(SameAssetFile(runtime,destination)||CanonicalAssetPath(runtime)==CanonicalAssetPath(destination))throw std::runtime_error("Choose an output different from the editor executable");
  size_t total=0;nlohmann::json manifest=nlohmann::json::array();
  for(const auto& asset:assets){CheckResourceName(asset.first);if(asset.second.size()>MaxPayload-total)throw std::runtime_error("Game assets exceed 1 GiB budget");manifest.push_back({{"name",asset.first},{"offset",uint64_t(total)},{"size",uint64_t(asset.second.size())}});total+=asset.second.size();}
  auto metadata=manifest.dump();CheckBudgets(assets.size(),metadata.size(),total,0);
  auto key=PackageAuth::LoadKey();
  PackageAuth::RejectKeyPath(runtime);PackageAuth::RejectKeyPath(destination);
  {PackageAuth::RejectEmbeddedKey scan(key);scan.Add(metadata.data(),metadata.size());}
  for(const auto& asset:assets){PackageAuth::RejectEmbeddedKey scan(key);scan.Add(asset.second.data(),asset.second.size());}
  std::string temporary,iconTemporary;FILE* output=nullptr;
#ifdef _WIN32
  std::unique_ptr<PrivateIconRuntime> privateIcon;
#endif
  auto copy=[&key](FILE* source,FILE* target,PackageAuth::RejectEmbeddedKey* sharedScan=nullptr){std::vector<unsigned char> block(1024*1024);PackageAuth::RejectEmbeddedKey ownScan(key);auto& scan=sharedScan?*sharedScan:ownScan;uint64_t copied=0;for(;;){size_t n=std::fread(block.data(),1,block.size(),source);if(n){copied+=n;if(copied>128*1024*1024)throw std::runtime_error("Runtime executable exceeds 128 MiB budget");scan.Add(block.data(),n);if(std::fwrite(block.data(),1,n,target)!=n)throw std::runtime_error("Cannot copy runtime executable");}if(n<block.size()){if(std::ferror(source))throw std::runtime_error("Cannot read runtime executable");break;}}return copied;};
  try{
    std::unique_ptr<FILE,int(*)(FILE*)> source(AssetOpen(runtime,"rb"),std::fclose);if(!source)throw std::runtime_error("Cannot open runtime template");
    if(SeekFile64(source.get(),0,SEEK_END))throw std::runtime_error("Cannot inspect runtime");
    int64_t sourceSize=TellFile64(source.get());if(sourceSize<0)throw std::runtime_error("Cannot inspect runtime size");
    if(sourceSize>128*1024*1024)throw std::runtime_error("Runtime executable exceeds 128 MiB budget");
    FILE* input=source.get();
    auto inspect=[input,sourceSize](uint64_t at,size_t count){if(at>uint64_t(sourceSize)||count>uint64_t(sourceSize)-at||SeekFile64(input,int64_t(at),SEEK_SET))throw std::runtime_error("Cannot inspect runtime footer");std::vector<unsigned char> bytes(count);if(std::fread(bytes.data(),1,count,input)!=count)throw std::runtime_error("Cannot inspect runtime footer");return bytes;};
    if(!LocateFooter(uint64_t(sourceSize),inspect).Bytes.empty())throw std::runtime_error("Choose the editor executable as runtime template");
    if(SeekFile64(source.get(),0,SEEK_SET))throw std::runtime_error("Cannot rewind runtime");
#ifdef _WIN32
    if(!icon.empty()){privateIcon.reset(new PrivateIconRuntime());iconTemporary=privateIcon->Path;std::unique_ptr<FILE,int(*)(FILE*)> staging(AssetOpen(iconTemporary,"wb"),std::fclose);if(!staging)throw std::runtime_error("Cannot stage icon");copy(source.get(),staging.get());if(std::fclose(staging.release()))throw std::runtime_error("Cannot finish icon staging");ReplaceGameExeIcon(iconTemporary,icon);source.reset(AssetOpen(iconTemporary,"rb"));if(!source)throw std::runtime_error("Cannot open icon runtime");}
#else
    (void)icon;
#endif
    output=OpenTemporaryAsset(destination,temporary);if(!output)throw std::runtime_error("Cannot create game output");PackageAuth::RejectEmbeddedKey outputScan(key);uint64_t offset=rawPayload?0:copy(source.get(),output,&outputScan);source.reset();
    auto header=FooterHeader(offset,8+metadata.size()+total);HmacSha256 mac(key.Bytes.data(),key.Bytes.size());AuthenticateHeader(mac,header);
    auto write=[&](const void* data,size_t n){outputScan.Add(data,n);if(n&&std::fwrite(data,1,n,output)!=n)throw std::runtime_error("Cannot write game payload");mac.Add(data,n);};
    std::vector<unsigned char> sizeBytes;Number(sizeBytes,metadata.size(),8);write(sizeBytes.data(),sizeBytes.size());write(metadata.data(),metadata.size());for(const auto& asset:assets)write(asset.second.data(),asset.second.size());
    auto tag=mac.Finish();header.insert(header.end(),tag.begin(),tag.end());outputScan.Add(header.data(),header.size());
    if(std::fwrite(header.data(),1,header.size(),output)!=header.size())throw std::runtime_error("Cannot finish authenticated game footer");
#ifndef _WIN32
    if(!rawPayload&&fchmod(fileno(output),0755))throw std::runtime_error("Cannot make game executable");
#endif
    bool ok=std::fclose(output)==0;output=nullptr;if(!ok)throw std::runtime_error("Cannot close game output");ReplaceAssetFile(temporary,destination);temporary.clear();if(!iconTemporary.empty())RemoveAssetFile(iconTemporary);
  }catch(...){if(output)std::fclose(output);if(!temporary.empty())RemoveAssetFile(temporary);if(!iconTemporary.empty())RemoveAssetFile(iconTemporary);throw;}
}
} // namespace GamePackage
static bool BuildStudioGame(const std::string &destination,
                            const std::string &runtime = "") {
  try {
    if (g_PhysicsRunning)
      throw std::runtime_error("Stop simulation before building");
    auto project = StudioProjectJson(
        g_OpenedProject.empty() ? std::string(g_ProjectPath) : g_OpenedProject,true);
    std::map<std::string,std::vector<unsigned char>> assets;size_t packedBytes=0;
    std::map<std::pair<uint32_t,size_t>,std::vector<std::string>> buckets;
    auto pack=[&](const std::string& proposed,std::vector<unsigned char> bytes){auto key=std::make_pair(GamePackage::CRC(bytes.data(),bytes.size()),bytes.size());for(const auto& name:buckets[key])if(assets.at(name)==bytes)return name;if(bytes.size()>GamePackage::MaxPayload-packedBytes||assets.size()>=4095)throw std::runtime_error("Game resource count or size budget exceeded before packing");packedBytes+=bytes.size();buckets[key].push_back(proposed);assets.emplace(proposed,std::move(bytes));return proposed;};
    for(size_t i=0;i<g_Sounds.size();++i){const auto& sound=g_Sounds[i];auto name=pack("assets/sound"+std::to_string(i),sound.Data.empty()?ReadAssetFile(sound.Path,16*1024*1024):sound.Data);project["sounds"][i]["path"]=name;project["sounds"][i]["data"]=SaveBinary({});}
    for(size_t i=0;i<g_Objects.size();++i)for(size_t m=0;m<g_Objects[i].Materials.size();++m){const auto& material=g_Objects[i].Materials[m];if(!material.Image.empty()||!material.Texture.empty()){auto name=pack("assets/texture"+std::to_string(i)+"_"+std::to_string(m),MaterialImageBytes(material));project["objects"][i]["materials"][m]["texture"]=name;project["objects"][i]["materials"][m]["image"]=SaveBinary({});}}
    auto icon=(g_Game.IconData.empty()?ReadGameIcon(g_Game.IconPath):ValidateGameIcon(g_Game.IconData));
    if(!icon.empty()){assets["assets/game-icon.png"]=icon;project["game"]["icon"]="assets/game-icon.png";project["game"].erase("icon_data");}
    if(HasManagedScripts())PackManagedRuntime(assets,project,packedBytes);
    auto text = project.dump();
    if (text.size() > 32 * 1024 * 1024)
      throw std::runtime_error("Game project exceeds 32 MiB budget");
    assets["project.lunoryx"] =
        std::vector<unsigned char>(text.begin(), text.end());
#ifdef __APPLE__
    WriteMacGameBundle(destination,runtime.empty()?GamePackage::ExecutablePath():runtime,assets,icon);
#else
    GamePackage::Write(runtime.empty() ? GamePackage::ExecutablePath()
                                       : runtime,
                       destination, assets, icon);
#endif
    AppendConsole("Built standalone game: " + destination + "\n");
    return true;
  } catch (const std::exception &e) {
    AppendConsole(std::string("Game build failed: ") + e.what() + "\n");
    return false;
  }
}
