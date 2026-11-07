#pragma once
static void CheckAuditRegressions() {
  assert(PathDirectory("/scene.lunoryx")=="/");assert(PathDirectory("C:/scene.lunoryx")=="C:/");
  for(size_t size: {size_t(0),size_t(1),size_t(2),size_t(3),size_t(4097),size_t(2*1024*1024)}){
    std::vector<unsigned char> bytes(size);for(size_t i=0;i<size;++i)bytes[i]=static_cast<unsigned char>(i);
    auto encoded=SaveBinary(bytes);assert(ReadBinary(encoded)==bytes);assert(encoded.dump().size()<size*2+100);
  }
  assert(ReadBinary(nlohmann::json::array({0,255}))==std::vector<unsigned char>({0,255}));
  for(const char* invalid:{"=AAA","AA=A","AB==","AAA","AAAA===="}){bool bad=false;try{ReadBinary({{"encoding","base64"},{"base64",invalid}});}catch(const std::exception&){bad=true;}assert(bad);}
  const auto state=ObjectIdCounter().load();
  CreateSampleGame();auto document=StudioProjectJson("tests/audit.lunoryx");auto before=ObjectIdCounter().load();document["sounds"]=nlohmann::json::array({{{"path","../outside.wav"}}});auto text=document.dump();WriteAssetFile("tests/audit-bad.lunoryx",text.data(),text.size());assert(!LoadStudioProject("tests/audit-bad.lunoryx"));assert(ObjectIdCounter().load()==before);RemoveAssetFile("tests/audit-bad.lunoryx");assert(before>=state);
  // External resources chosen in the UI become embedded on save.
  tinygltf::Image image;image.width=image.height=2;image.component=4;image.bits=8;image.image.assign(16,255);auto png=EncodeImage(image);WriteAssetFile("build/audit-image.png",png.data(),png.size());
  g_Objects.clear();g_Sounds.clear();g_Game=StudioGameSettings();g_Objects.emplace_back("textured");Model::ToObject(g_Objects.back(),Model::FromObject(g_Objects.back()));StudioMaterial material;material.Texture=AbsoluteAssetPath("build/audit-image.png");g_Objects[0].Materials.push_back(material);
  assert(SaveStudioProject("tests/audit.lunoryx"));auto savedHash=g_SavedDocumentHash;auto savedPath=g_OpenedProject;g_Objects[0].Position[0]=2;assert(SaveRecoverySnapshot()&&ProjectDirty()&&g_SavedDocumentHash==savedHash&&g_OpenedProject==savedPath);RemoveAssetFile(ProjectRecoveryPath());assert(LoadStudioProject("tests/audit.lunoryx"));assert(g_Objects[0].Materials[0].Texture.empty()&&g_Objects[0].Materials[0].Image==png);
  auto copy=g_Objects[0];copy.Id=NewObjectId();g_Objects.push_back(copy);assert(ExportGLB("tests/audit.glb"));tinygltf::TinyGLTF reader;tinygltf::Model model;std::string error,warning;assert(reader.LoadBinaryFromFile(&model,&error,&warning,"tests/audit.glb"));assert(model.images.size()==1&&model.textures.size()==1);
  const auto valid=ReadAssetFile("tests/audit.glb");g_Objects[0].Vertices[0]=std::numeric_limits<float>::infinity();assert(!ExportGLB("tests/audit.glb"));assert(ReadAssetFile("tests/audit.glb")==valid);
  // A fixed .tmp symlink cannot redirect atomic writes.
#ifndef _WIN32
  const char* target="tests/audit-target";WriteAssetFile(target,"keep",4);assert(symlink("audit-target","tests/audit.lunoryx.tmp")==0);WriteAssetFileAtomic("tests/audit.lunoryx","safe",4);assert(ReadAssetFile(target)==std::vector<unsigned char>({'k','e','e','p'}));RemoveAssetFile("tests/audit.lunoryx.tmp");RemoveAssetFile(target);
#endif
  StudioMesh collinear;collinear.Positions={0,0,0,1,0,0,2,0,0,2,1,0,0,1,0};collinear.Faces={{0,1,2,3,4}};Model::Triangulate(collinear);assert(collinear.Indices.size()==6&&collinear.Faces[0].size()==4);
  // Authenticode's terminal certificate table may follow an aligned package.
  std::vector<unsigned char> pe(512,0);pe[0]='M';pe[1]='Z';pe[60]=64;pe[64]='P';pe[65]='E';pe[84]=240;pe[88]=0x0b;pe[89]=2;pe[196]=16;
  std::string metadata=R"([{"name":"project.lunoryx","offset":0,"size":2}])";std::vector<unsigned char> payload;GamePackage::Number(payload,metadata.size(),8);payload.insert(payload.end(),metadata.begin(),metadata.end());payload.push_back('{');payload.push_back('}');auto crc=GamePackage::CRC(payload.data(),payload.size());pe.insert(pe.end(),payload.begin(),payload.end());pe.insert(pe.end(),{'L','N','X','P','C','K','0','1'});GamePackage::Number(pe,512,8);GamePackage::Number(pe,payload.size(),8);GamePackage::Number(pe,crc,4);GamePackage::Number(pe,1,4);
  while(pe.size()%8)pe.push_back(0);
  size_t cert=pe.size();for(int k=0;k<4;++k)pe[232+k]=static_cast<unsigned char>(cert>>(8*k));pe[236]=8;GamePackage::Number(pe,8,4);GamePackage::Number(pe,0x200,2);GamePackage::Number(pe,2,2);WriteAssetFile("tests/audit-signed.exe",pe.data(),pe.size());std::map<std::string,std::vector<unsigned char>> assets;assert(GamePackage::Read("tests/audit-signed.exe",assets)&&assets.at("/__lunoryx_bundle__/project.lunoryx")==std::vector<unsigned char>({'{','}'}));RemoveAssetFile("tests/audit-signed.exe");
  g_Objects.clear();g_Sounds.clear();g_Game=StudioGameSettings();RemoveAssetFile("build/audit-image.png");RemoveAssetFile("tests/audit.lunoryx");RemoveAssetFile("tests/audit.glb");
}
