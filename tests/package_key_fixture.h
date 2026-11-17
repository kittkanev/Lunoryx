#pragma once
struct TestPackageKey {
  std::string Previous=PackageAuth::KeyFile(),Path;
  TestPackageKey(){
    Path=(std::filesystem::temp_directory_path()/("Lunoryx-test-key-"+std::to_string(std::random_device{}())+".key")).u8string();
    PackageAuth::GenerateKeyFile(Path);PackageAuth::Configure(Path);
  }
  ~TestPackageKey(){PackageAuth::Configure(std::move(Previous));RemoveAssetFile(Path);}
};
