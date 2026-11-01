#pragma once

static void CheckPatchRegressions() {
  CreateSampleGame();
  g_Game.StartupScript = -1;
  const std::string path = "tests/patch-project.lunoryx";
  assert(SaveStudioProject(path));
  const auto original = ReadAssetFile(path);
  const auto savedPath = g_OpenedProject;
  const auto editorHash = CompleteDocumentHash();
  StartSimulation();
  g_Objects[0].Position[0] = 123;
  assert(!SaveStudioProject(path));
  assert(!SaveStudioProject("tests/patch-play-save.lunoryx"));
  assert(ReadAssetFile(path) == original);
  assert(!AssetExists("tests/patch-play-save.lunoryx"));
  assert(g_OpenedProject == savedPath);
  StopSimulation();
  assert(CompleteDocumentHash() == editorHash && !ProjectDirty());

  const auto valid = nlohmann::json::parse(original.begin(), original.end());
  const auto badPath = "tests/patch-invalid.lunoryx";
  auto reject = [&](const char *pointer, const nlohmann::json &value) {
    auto damaged = valid;
    damaged[nlohmann::json::json_pointer(pointer)] = value;
    const auto text = damaged.dump();
    WriteAssetFile(badPath, text.data(), text.size());
    assert(!LoadStudioProject(badPath));
    assert(CompleteDocumentHash() == editorHash);
    assert(g_OpenedProject == savedPath && !ProjectDirty());
  };
  // The first example object has a custom, textured mesh.
  assert(valid["objects"][0].contains("mesh"));
  for (const auto &value :
       {nlohmann::json(uint64_t(4294967296)), nlohmann::json(-4294967296LL),
        nlohmann::json(0.5), nlohmann::json(true)}) {
    reject("/objects/0/mesh/faces/0/0", value);
    reject("/objects/0/mesh/indices/0", value);
    reject("/objects/0/mesh/face_materials/0", value);
  }
  reject("/schema", uint64_t(4294967298));
  reject("/schema", 0);
  reject("/objects/0/type", uint64_t(4294967296));
  reject("/objects/0/id", uint64_t(1) << 63);
  reject("/objects/0/mesh/seams", nlohmann::json::array({-1}));
  reject(
      "/objects/0/mesh/colors",
      std::vector<float>(valid["objects"][0]["mesh"]["positions"].size(), 2.f));
  reject("/objects/0/materials/0/image/0", 256);
  reject("/sounds/0/data/0", -1);
  reject("/game/startup_script", 0.5);
  reject("/game/camera_target", -1);
  assert(LoadStudioProject(path));

  for (const auto &name : {".", "../outside", "a/../b", "a:stream"}) {
    std::map<std::string, std::vector<unsigned char>> assets;
    assets["project.lunoryx"] = {};
    assets[name] = {};
    bool rejected = false;
    try {
      GamePackage::Write("unused", "unused", assets);
    } catch (const std::runtime_error &e) {
      rejected =
          std::string(e.what()).find("resource path") != std::string::npos;
    }
    assert(rejected);
  }
  std::remove(path.c_str());
  std::remove(badPath);
  std::cout << "PASS: Play save protection and transactional rejection of "
               "malformed project numbers\n";
}
