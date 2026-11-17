#include "src/io/asset_io.h"
#include <filesystem>
#include <cassert>
#include <iostream>
int main(){
  namespace fs=std::filesystem;
  auto root=fs::temp_directory_path()/fs::path("lunoryx-io-"+std::to_string(std::random_device{}()));fs::create_directory(root);
  try{auto path=ConfinedAssetPath("asset.bin",CanonicalAssetPath(root.u8string()));
    const std::string first="first",second="second";WriteAssetFileAtomic(path,first.data(),first.size());WriteAssetFileAtomic(path,second.data(),second.size());
    assert(ReadAssetFile(path)==std::vector<unsigned char>(second.begin(),second.end()));
    auto empty=ConfinedAssetPath("empty.bin",CanonicalAssetPath(root.u8string()));
    WriteAssetFileAtomic(empty,nullptr,0);assert(AssetExists(empty)&&ReadAssetFile(empty).empty());
    auto fresh=ConfinedAssetPath("new.bin",CanonicalAssetPath(root.u8string()));WriteNewAssetFileAtomic(fresh,nullptr,0);assert(AssetExists(fresh)&&ReadAssetFile(fresh).empty());
    bool preserved=false;try{WriteNewAssetFileAtomic(path,"replace",7);}catch(const std::exception&){preserved=true;}
    assert(preserved&&ReadAssetFile(path)==std::vector<unsigned char>(second.begin(),second.end()));assert(TemporaryAssetOperations().empty());
    auto direct=(root/"direct.bin").u8string();WriteAssetFile(direct,nullptr,0);assert(AssetExists(direct)&&ReadAssetFile(direct).empty());
    CreateAssetDirectories(CanonicalAssetPath(root.u8string()),"nested/child");auto nested=ConfinedAssetPath("nested/child/asset.bin",CanonicalAssetPath(root.u8string()));WriteAssetFileAtomic(nested,first.data(),first.size());assert(ReadAssetFile(nested)==std::vector<unsigned char>(first.begin(),first.end()));
    ConfinedAssetPolicies().clear();AssetRootAnchors().clear();fs::remove_all(root);std::cout<<"PASS: anchored files, atomic overwrite/no-overwrite, empty writes, nested directories and reads\n";return 0;
  }catch(const std::exception& error){std::cerr<<"Asset I/O: "<<error.what()<<"\n";return 1;}
}
