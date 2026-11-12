#pragma once
static void CheckV131Regressions() {
  InitLuaState();UnloadAllStudioSounds();g_Sounds.clear();
  CheckLua(R"LUA(
    for _,bad in ipairs({'safe.wav'..string.char(0)..'../outside.wav',string.char(0)..'safe.wav',
                          'safe.wav'..string.char(0)..string.rep('x',600),'a\nb.wav','a\127b.wav'}) do
      assert(not pcall(add_sound,bad))
    end
    assert(not pcall(add_sound,'safe.wav','bad'..string.char(0)..'name'))
    assert(not pcall(add_sound,'safe.wav','bad\nname'))
    assert(not pcall(add_sound,'../outside.wav'))
    assert(add_sound('tests/Звук с пробелом.wav')==0)
    assert(add_sound('tests/future.wav','')==1)
  )LUA");
  assert(g_Sounds.size()==2&&g_Sounds[0].Name=="Sound"&&g_Sounds[1].Name.empty());
  g_Sounds.clear();
  g_Game=StudioGameSettings();g_Game.Widgets.push_back(GameWidget());
  g_Game.CameraYaw=0.f;g_Game.Widgets[0].Value=0.f;g_Game.Widgets[0].Color[0]=0.f;
  g_SavedDocumentHash=CompleteDocumentHash();assert(!ProjectDirty());
  g_Game.CameraYaw=-0.f;g_Game.Widgets[0].Value=-0.f;g_Game.Widgets[0].Color[0]=-0.f;
  assert(!ProjectDirty());
  auto positive=GameSettingsFingerprint(g_Game);
  g_Game.CameraYaw=std::numeric_limits<float>::quiet_NaN();assert(GameSettingsFingerprint(g_Game)==positive);
  g_Game.CameraYaw=std::numeric_limits<float>::infinity();assert(GameSettingsFingerprint(g_Game)==positive);
  g_Game.CameraYaw=0.f;g_Game.Widgets[0].Text+=" changed";assert(ProjectDirty());

  // Stop frees decoder/encoded buffers both outside Play and for orphaned IDs.
  SDL_setenv("LUNORYX_AUDIO_NULL","1",1);
  auto tone=ExampleTone();char error[256];
  g_PhysicsRunning=false;g_Game.StartupScript=-1;
  for(int i=0;i<10;++i){StudioSound sound;sound.Data=tone;g_Sounds.push_back(sound);assert(PlayStudioSound(g_Sounds.back()));}
  assert(studio_audio_loaded_count()==10);
  StopSimulation();assert(studio_audio_loaded_count()==0&&g_Sounds.size()==10);
  for(int iteration=0;iteration<3;++iteration){
    StartSimulation();for(const auto& sound:g_Sounds)assert(PlayStudioSound(sound));
    assert(studio_audio_loaded_count()==10);g_Sounds.clear();StopSimulation();
    assert(studio_audio_loaded_count()==0&&g_Sounds.size()==10);
    assert(g_PlaySnapshot.empty()&&g_PlaySoundSnapshot.empty());
  }
  auto orphan=NewObjectId();assert(studio_audio_load_memory(orphan,tone.data(),tone.size(),error));
  assert(studio_audio_loaded_count()==1);StopSimulation();assert(studio_audio_loaded_count()==0);
  studio_audio_shutdown();g_Sounds.clear();ShutdownLuaState();

  const std::string small="tests/v131-small.lnx",large="tests/v131-sparse.lnx";
  std::map<std::string,std::vector<unsigned char>> assets={{"project.lunoryx",{'{','}'}},{"assets/data",{1,2,3}}},read;
  GamePackage::Write(GamePackage::ExecutablePath(),small,assets,{},true);
  assert(GamePackage::Read(small,read)&&read.size()==assets.size());
  auto bytes=ReadAssetFile(small);const uint64_t offset=(uint64_t(3)<<30)+123;
  for(int i=0;i<8;++i)bytes[bytes.size()-24+size_t(i)]=static_cast<unsigned char>(offset>>(i*8));
  {
    std::unique_ptr<FILE,int(*)(FILE*)> file(AssetOpen(large,"wb"),std::fclose);assert(file);
#ifdef _WIN32
    DWORD returned=0;HANDLE handle=reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file.get())));
    assert(DeviceIoControl(handle,0x900C4,nullptr,0,nullptr,0,&returned,nullptr)); // FSCTL_SET_SPARSE
#endif
    assert(GamePackage::SeekFile64(file.get(),int64_t(offset),SEEK_SET)==0);
    assert(std::fwrite(bytes.data(),1,bytes.size(),file.get())==bytes.size());
    assert(GamePackage::TellFile64(file.get())==int64_t(offset+bytes.size()));
  }
  read.clear();assert(GamePackage::Read(large,read));
  for(const auto& asset:assets)assert(read.at("/__lunoryx_bundle__/"+asset.first)==asset.second);
  bool rejected=false;try{GamePackage::Write(large,"tests/v131-unused.lnx",assets);}catch(const std::runtime_error& e){rejected=std::string(e.what()).find("128 MiB")!=std::string::npos;}assert(rejected);
  {
    std::unique_ptr<FILE,int(*)(FILE*)> file(AssetOpen(large,"r+b"),std::fclose);assert(file);
    unsigned char invalid[8];std::fill(invalid,invalid+8,255);
    assert(!GamePackage::SeekFile64(file.get(),int64_t(offset+bytes.size()-24),SEEK_SET));
    assert(std::fwrite(invalid,1,8,file.get())==8);
  }
  auto retained=read;rejected=false;try{GamePackage::Read(large,read);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&read==retained);
  std::remove(small.c_str());std::remove(large.c_str());
  std::cerr<<"PASS: 1.3.1 Lua bytes, signed-zero dirty state, audio Stop cleanup, sparse >3 GiB package and invalid offset\n";
}
