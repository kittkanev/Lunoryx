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
    CreateAssetDirectories(CanonicalAssetPath(root.u8string()),"nested/child");auto nested=ConfinedAssetPath("nested/child/asset.bin",CanonicalAssetPath(root.u8string()));WriteAssetFileAtomic(nested,first.data(),first.size());assert(ReadAssetFile(nested)==std::vector<unsigned char>(first.begin(),first.end()));
    ConfinedAssetPolicies().clear();AssetRootAnchors().clear();fs::remove_all(root);std::cout<<"PASS: anchored file creation, atomic overwrite, nested directories and reads\n";return 0;
  }catch(const std::exception& error){std::cerr<<"Asset I/O: "<<error.what()<<"\n";return 1;}
}
