#pragma once
static void CheckV140PackageAuthentication() {
  const std::string runtime="tests/auth-runtime.bin",destination="tests/auth-package.lnx",bad="tests/auth-invalid.lnx";
  WriteAssetFile(runtime,"template",8);
  std::map<std::string,std::vector<unsigned char>> assets={{"project.lunoryx",{'{','}'}},{"assets/large-resource-with-an-unambiguous-offset",std::vector<unsigned char>(8192,0x53)}},read;
  GamePackage::Write(runtime,destination,assets,{},true);auto original=ReadAssetFile(destination);
  assert(GamePackage::Read(destination,read)&&read.at("/__lunoryx_bundle__/project.lunoryx")==assets.at("project.lunoryx"));
  auto unchanged=read;auto key=PackageAuth::LoadKey();
  for(const auto& path:{std::string("//server/share/package.lnx"),std::string("\\\\server\\share\\package.lnx"),std::string("bad\npackage.lnx")}){
    bool denied=false;try{GamePackage::Read(path,read);}catch(const std::runtime_error&){denied=true;}assert(denied&&read==unchanged);
  }
  assert(std::search(original.begin(),original.end(),key.Bytes.begin(),key.Bytes.end())==original.end());
  auto invalid=[&](const std::vector<unsigned char>& bytes){
    WriteAssetFileAtomic(bad,bytes.data(),bytes.size());bool rejected=false;
    try{rejected=!GamePackage::Read(bad,read);}catch(const std::exception&){rejected=true;}
    assert(rejected&&read==unchanged);
  };
  for(size_t at:{size_t(8),size_t(100),original.size()-64+8,original.size()-64+16,original.size()-64+24,original.size()-64+28,original.size()-32,original.size()-1}){
    auto modified=original;modified[at]^=1;invalid(modified);
  }
  auto previous=PackageAuth::KeyFile();PackageAuth::Configure("");bool rejected=false;
  try{GamePackage::Read(destination,read);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&read==unchanged);
  PackageAuth::Configure(previous);
  {
    TestPackageKey privateKey;auto before=ReadAssetFile(privateKey.Path);
    rejected=false;try{GamePackage::Write(runtime,privateKey.Path,assets,{},true);}catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&ReadAssetFile(privateKey.Path)==before);
    rejected=false;try{WriteAssetFileAtomic(privateKey.Path,"not a key",9);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&ReadAssetFile(privateKey.Path)==before);
    rejected=false;try{RemoveAssetFile(privateKey.Path);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&ReadAssetFile(privateKey.Path)==before);
  }
  auto containsKey=assets;containsKey["runtime/private.key"]=std::vector<unsigned char>(key.Bytes.begin(),key.Bytes.end());
  rejected=false;try{GamePackage::Write(runtime,destination,containsKey,{},true);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&ReadAssetFile(destination)==original);
  // The final serialized scan also spans resource boundaries and copy chunks.
  containsKey=assets;containsKey["a/key-part1"]=std::vector<unsigned char>(key.Bytes.begin(),key.Bytes.begin()+16);containsKey["a/key-part2"]=std::vector<unsigned char>(key.Bytes.begin()+16,key.Bytes.end());
  rejected=false;try{GamePackage::Write(runtime,destination,containsKey,{},true);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&ReadAssetFile(destination)==original);
  std::vector<unsigned char> keyInRuntime(1024*1024+64,0x33);std::copy(key.Bytes.begin(),key.Bytes.end(),keyInRuntime.begin()+1024*1024-16);WriteAssetFileAtomic(bad,keyInRuntime.data(),keyInRuntime.size());
  rejected=false;try{GamePackage::Write(bad,destination,assets);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&ReadAssetFile(destination)==original);
  rejected=false;try{GamePackage::Write(PackageAuth::KeyFile(),destination,assets,{},true);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
  {
    TestPackageKey wrong;
    rejected=false;try{GamePackage::Read(destination,read);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&read==unchanged);
  }
  // Explicit legacy CRC format must not become an authentication downgrade.
  auto legacy=std::vector<unsigned char>(original.begin(),original.end()-64);
  std::vector<unsigned char> footer={'L','N','X','P','C','K','0','1'};GamePackage::Number(footer,0,8);GamePackage::Number(footer,legacy.size(),8);GamePackage::Number(footer,GamePackage::CRC(legacy.data(),legacy.size()),4);GamePackage::Number(footer,1,4);legacy.insert(legacy.end(),footer.begin(),footer.end());invalid(legacy);
  for(size_t length:{size_t(0),size_t(31),size_t(33)}){
    PackageAuth::Configure(previous);
    std::vector<unsigned char> bytes(length,0x31);WriteAssetFileAtomic("tests/auth-malformed.key",bytes.data(),bytes.size());PackageAuth::Configure("tests/auth-malformed.key");
    rejected=false;try{PackageAuth::LoadKey();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
  }
  PackageAuth::Configure(previous);RemoveAssetFile("tests/auth-malformed.key");
  {
    auto before=ReadAssetFile(PackageAuth::KeyFile());bool asyncDenied=false;
    g_ExportJob=std::async(std::launch::async,[&](){try{WriteAssetFileAtomic(PackageAuth::KeyFile(),"bad",3);}catch(const std::runtime_error&){asyncDenied=true;}return ExportResult{false,""};});
    g_ModelJob=std::async(std::launch::async,[](){return ModelingResult();});
    FinishBackgroundAssetJobs();assert(asyncDenied&&!g_ExportJob.valid()&&!g_ModelJob.valid()&&ReadAssetFile(PackageAuth::KeyFile())==before);
  }
#ifndef _WIN32
  WriteAssetFileAtomic("tests/auth-public.key",key.Bytes.data(),key.Bytes.size());assert(!chmod("tests/auth-public.key",0644));PackageAuth::Configure("tests/auth-public.key");
  rejected=false;try{PackageAuth::LoadKey();}catch(const std::runtime_error&){rejected=true;}assert(rejected);PackageAuth::Configure(previous);RemoveAssetFile("tests/auth-public.key");
#endif
  // A synthetic changing reader models a same-inode writer: the second read
  // of resource bytes returns altered content. Authenticated output must use
  // the bytes actually hashed, not a later reread after a successful MAC pass.
  uint64_t manifestSize=GamePackage::NumberAt(original,0,8);auto manifest=nlohmann::json::parse(original.begin()+8,original.begin()+8+size_t(manifestSize));
  size_t begin=8+size_t(manifestSize)+manifest[0]["offset"].get<size_t>(),end=begin+manifest[0]["size"].get<size_t>(),reads=0;
  auto changing=[&](uint64_t at,size_t count){assert(at<=original.size()&&count<=original.size()-at);auto bytes=std::vector<unsigned char>(original.begin()+size_t(at),original.begin()+size_t(at)+count);if(at<end&&count>begin-std::min(begin,size_t(at))){if(++reads>1)std::fill(bytes.begin(),bytes.end(),0x61);}return bytes;};
  std::map<std::string,std::vector<unsigned char>> captured;assert(GamePackage::ReadContent(original.size(),changing,captured));assert(reads==1&&captured==unchanged);
  // Shared arithmetic: exact caps, each adjacent boundary and overflow. No
  // giant vector is needed for rejection tests; large streaming cases are CLI tests.
  const uint64_t limit=GamePackage::MaxPayload,manifestBytes=1024,runtimeLimit=128ull*1024*1024;
  for(uint64_t entries:{uint64_t(4095),uint64_t(4096)})for(uint64_t data:{limit-8-manifestBytes-1,limit-8-manifestBytes})GamePackage::CheckBudgets(entries,manifestBytes,data,runtimeLimit);
  for(auto values:std::vector<std::array<uint64_t,4>>{{{4097,manifestBytes,0,0}},{{4096,manifestBytes,limit-8-manifestBytes+1,0}},{{4096,4ull*1024*1024+1,0,0}},{{4096,0,0,runtimeLimit+1}},{{4096,UINT64_MAX,UINT64_MAX,UINT64_MAX}}}){
    rejected=false;try{GamePackage::CheckBudgets(values[0],values[1],values[2],values[3]);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
  }
  // Authenticated or CRC containers (including padded ones) are never valid
  // editor runtime templates, and failed writes preserve existing destinations.
  for(auto bytes:{original,legacy}){
    bytes.insert(bytes.end(),7,0);WriteAssetFileAtomic(bad,bytes.data(),bytes.size());
    rejected=false;try{GamePackage::Write(bad,destination,assets,{},true);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&ReadAssetFile(destination)==original);
  }
  for(const auto& path:{runtime,destination,bad})RemoveAssetFile(path);
  std::cerr<<"PASS: authenticated package tamper/key/downgrade rejection, consumed-byte capture, budget edges and atomic output\n";
}
