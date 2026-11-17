#pragma once
static void CheckProjectMigration(){
  StopSimulation();StopManagedScripts();
  auto folder=ProjectFS::temp_directory_path()/ProjectFS::u8path("Lunoryx migration Проект "+std::to_string(std::random_device{}()));
  assert(CreateProjectFolder("Migration",folder.u8string()));
  auto manifest=g_OpenedProject,source=ConfinedAssetPath("scripts/Main.luaryx",g_ProjectDirectory);
  auto root=nlohmann::json::parse(ReadAssetFile(manifest));assert(root["schema"]==4&&root["format_version"]==4);
  root["schema"]=3;root["format_version"]=3;root["scripts"][0]["code"]="-- stale embedded source";
  auto original=root.dump(2);WriteAssetFileAtomic(manifest,original.data(),original.size());
  std::string disk="-- original external source / Привет\r\n";
  WriteAssetFileAtomic(source,disk.data(),disk.size());std::strcpy(StudioScripts()[0].Code,"-- current unsaved replacement\n");
  assert(SyncProjectFiles());
  auto raw=SiblingAssetPath(manifest,".pre-1.4.bak"),recovery=SiblingAssetPath(manifest,".pre-1.4.recovery.lunoryx");
  auto backup=ReadAssetFile(raw),recoverBytes=ReadAssetFile(recovery);
  assert(std::string(backup.begin(),backup.end())==original);
  auto recoverRoot=nlohmann::json::parse(recoverBytes);assert(recoverRoot["scripts"][0]["code"]==disk&&!recoverRoot["scripts"][0].contains("file"));
  assert(ReadAssetFile(manifest)==backup); // Backup preceded the first script write.
  assert(SaveStudioProject(manifest));assert(nlohmann::json::parse(ReadAssetFile(manifest))["schema"]==4);
  assert(LoadStudioProject(recovery));assert(std::string(StudioScripts()[0].Code)==CheckedScriptText(disk));
  auto writtenSource=ReadAssetFile(source);assert(!SaveCurrentProject()&&!SaveStudioProject(recovery));
  assert(ReadAssetFile(recovery)==recoverBytes&&ReadAssetFile(source)==writtenSource);
  assert(LoadStudioProject(manifest));assert(std::string(StudioScripts()[0].Code)=="-- current unsaved replacement\n");
  std::strcpy(StudioScripts()[0].Code,"-- next edit\n");assert(SaveCurrentProject());
  assert(ReadAssetFile(raw)==backup&&ReadAssetFile(recovery)==recoverBytes);
  auto stableSource=ReadAssetFile(source),stableManifest=ReadAssetFile(manifest);
  StartSimulation();assert(g_PhysicsRunning);std::strcpy(StudioScripts()[0].Code,"-- Play edit must not be saved\n");
  assert(!SaveCurrentProject());assert(ReadAssetFile(source)==stableSource&&ReadAssetFile(manifest)==stableManifest);StopSimulation();

  auto invalid=ConfinedAssetPath("invalid.lunoryx",g_ProjectDirectory);
  auto future=nlohmann::json::parse(stableManifest);future["schema"]=5;future["format_version"]=5;auto text=future.dump();WriteAssetFileAtomic(invalid,text.data(),text.size());
  auto hash=CompleteDocumentHash();assert(!LoadStudioProject(invalid)&&CompleteDocumentHash()==hash);
  future["schema"]=4;future["format_version"]=3;text=future.dump();WriteAssetFileAtomic(invalid,text.data(),text.size());
  assert(!LoadStudioProject(invalid)&&CompleteDocumentHash()==hash);

  auto blocked=ConfinedAssetPath("blocked.lunoryx",g_ProjectDirectory);text=root.dump();WriteAssetFileAtomic(blocked,text.data(),text.size());
  auto blockedBackup=SiblingAssetPath(blocked,".pre-1.4.bak");WriteAssetFileAtomic(blockedBackup,"preserve",8);
  g_OpenedProject=blocked;assert(!SyncProjectFiles());assert(ReadAssetFile(source)==stableSource);
  assert(!SaveStudioProject(blocked)&&ReadAssetFile(blocked)==std::vector<unsigned char>(text.begin(),text.end()));
  assert(ReadAssetFile(blockedBackup)==std::vector<unsigned char>({'p','r','e','s','e','r','v','e'}));g_OpenedProject=manifest;

  auto resumed=ConfinedAssetPath("resumed.lunoryx",g_ProjectDirectory);WriteAssetFileAtomic(resumed,text.data(),text.size());
  ProjectMigration::EnsureBackup(resumed);auto resumedRaw=SiblingAssetPath(resumed,".pre-1.4.bak");auto resumedRecovery=SiblingAssetPath(resumed,".pre-1.4.recovery.lunoryx");auto first=ReadAssetFile(resumedRecovery);
  RemoveAssetFile(resumedRaw);ProjectMigration::EnsureBackup(resumed);assert(ReadAssetFile(resumedRecovery)==first);
  assert(ReadAssetFile(resumedRaw)==std::vector<unsigned char>(text.begin(),text.end()));
  bool refused=false;try{WriteNewAssetFileAtomic(resumedRaw,"replace",7);}catch(const std::exception&){refused=true;}
  assert(refused&&ReadAssetFile(resumedRaw)==std::vector<unsigned char>(text.begin(),text.end()));

  auto legacy=ConfinedAssetPath("legacy.lunoryx",g_ProjectDirectory);root["schema"]=2;root["format_version"]=2;text=root.dump();WriteAssetFileAtomic(legacy,text.data(),text.size());
  auto earlier=SiblingAssetPath(legacy,".pre-1.3.bak");WriteAssetFileAtomic(earlier,"older backup",12);ProjectMigration::EnsureBackup(legacy);
  assert(ReadAssetFile(earlier)==std::vector<unsigned char>({'o','l','d','e','r',' ','b','a','c','k','u','p'}));
  assert(ReadAssetFile(SiblingAssetPath(legacy,".pre-1.4.bak"))==std::vector<unsigned char>(text.begin(),text.end()));
  // Invalid disk source blocks migration before changing any tracked files.
  auto pending=root;pending["schema"]=3;pending["format_version"]=3;text=pending.dump();WriteAssetFileAtomic(manifest,text.data(),text.size());
  RemoveAssetFile(raw);RemoveAssetFile(recovery);std::string invalidCode(1,'\0');WriteAssetFileAtomic(source,invalidCode.data(),invalidCode.size());
  assert(!SyncProjectFiles());assert(!AssetExists(raw)&&!AssetExists(recovery));assert(ReadAssetFile(manifest)==std::vector<unsigned char>(text.begin(),text.end()));
  assert(ReadAssetFile(source)==std::vector<unsigned char>(1,0));

  ShutdownLuaState();g_ProjectDirectory.clear();ScriptAssetRoot().clear();g_ProjectName.clear();g_OpenedProject.clear();
  {std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());ConfinedAssetPolicies().clear();AssetRootAnchors().clear();}
  ProjectFS::remove_all(folder);
  std::cout<<"PASS: schema 4 migration, byte-exact immutable backups, external script recovery, interrupted backup resume and Play write protection\n";
}
