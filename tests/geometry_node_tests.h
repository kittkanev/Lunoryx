#pragma once
static void CheckGeometryNodeRegressions() {
  auto graph = GeometryNodes::Default();
  auto mesh = GeometryNodes::Evaluate(graph);
  assert(!mesh.Faces.empty());
  StudioGeometryNode array;
  array.Type = StudioGeometryNode::Array;
  array.Id = 4;
  array.Input = 2;
  array.Count = 3;
  array.Vector[0] = 2;
  array.Vector[1] = array.Vector[2] = 0;
  graph[2].Input = 4;
  graph.push_back(array);
  auto repeated = GeometryNodes::Evaluate(graph);
  assert(repeated.Positions.size() == mesh.Positions.size() * 3);
  auto invalid = graph;
  invalid[1].Input = 4;
  bool failed = false;
  try {
    GeometryNodes::Evaluate(invalid);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
  invalid = graph;
  invalid[1].Input = 999;
  failed = false;
  try {
    GeometryNodes::Validate(invalid);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
  invalid = graph;
  invalid[0].Vector[0] = std::numeric_limits<float>::infinity();
  failed = false;
  try {
    GeometryNodes::Validate(invalid);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
  // Reject expansion before committing geometry to the live object.
  auto huge = graph;
  uint32_t input = 4;
  for (int i = 0; i < 4; ++i) {
    auto n = array;
    n.Id = uint32_t(5 + i);
    n.Input = input;
    n.Count = 32;
    huge.push_back(n);
    input = n.Id;
  }
  huge[2].Input = input;
  GameObject object("Nodes round trip");
  GeometryNodes::Apply(object, graph);
  auto positions = object.Vertices;
  failed = false;
  try {
    GeometryNodes::Apply(object, huge);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed && object.Vertices == positions);
  g_Objects = {object};
  g_Sounds.clear();
  g_Game.StartupScript = -1;
  g_ObjectHistory.Clear();
  g_ObjectHistory.Capture(g_Objects);
  assert(SaveStudioProject("tests/node-project.lunoryx"));
  auto baseline = CompleteDocumentHash();
  g_Objects.clear();
  assert(LoadStudioProject("tests/node-project.lunoryx"));
  assert(g_Objects[0].GeometryNodes.size() == 4 &&
         g_Objects[0].Vertices == positions);
  assert(GeometryNodes::Evaluate(g_Objects[0].GeometryNodes).Positions ==
         positions);
  auto applied = graph;
  applied[1].Vector[1] = 2;
  GeometryNodes::Apply(g_Objects[0], applied);
  g_ObjectHistory.Capture(g_Objects);
  assert(ProjectDirty());
  assert(g_ObjectHistory.Undo(g_Objects) && CompleteDocumentHash() == baseline);
  assert(g_ObjectHistory.Redo(g_Objects) &&
         g_Objects[0].GeometryNodes[1].Vector[1] == 2);
  g_SelectedObjectIndex = 0;
  g_Modeling.Edit = true;
  float target[3] = {0, 0, 0};
  DrawModelSelection(
      10, 25, -35,
      target); // Node meshes must exit before touching ImGui/mesh-edit state.
  g_Modeling.Edit = false;
  failed = false;
  try {
    Model::SetEditable(g_Objects[0], mesh);
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
  assert(!MakeEditable(g_Objects[0], 1));
  g_Objects[0].GeometryNodes.clear();
  assert(MakeEditable(g_Objects[0], 1));
  const auto before = g_Objects[0].Vertices;
  assert(SculptBrush(g_Objects[0], {0, 0, 0}, 10, .1f, 0) &&
         g_Objects[0].Vertices != before);
  assert(g_Objects[0].Normals.size() == g_Objects[0].Vertices.size());
  for (float n : g_Objects[0].Normals)
    assert(std::isfinite(n));
  auto encoded = StudioProjectJson("tests/node-project.lunoryx");
  encoded["objects"][0]["geometry_nodes"] =
      nlohmann::json::array({{{"id", 1}}});
  auto bytes = encoded.dump();
  WriteAssetFile("tests/node-invalid.lunoryx", bytes.data(), bytes.size());
  auto hash = CompleteDocumentHash();
  assert(!LoadStudioProject("tests/node-invalid.lunoryx") &&
         CompleteDocumentHash() == hash);
  std::remove("tests/node-project.lunoryx");
  std::remove("tests/node-invalid.lunoryx");
  GameObject symmetric("Symmetry", MeshType::Plane);
  assert(MakeEditable(symmetric));
  for (size_t i = 1; i < symmetric.Vertices.size(); i += 3)
    symmetric.Vertices[i] = 1;
  assert(SculptBrush(symmetric, {.4f, 0, 0}, 2, .3f, 2, true));
  for (size_t i = 0; i < symmetric.Vertices.size(); i += 3)
    for (size_t j = 0; j < symmetric.Vertices.size(); j += 3)
      if (symmetric.Vertices[i] == -symmetric.Vertices[j] &&
          symmetric.Vertices[i + 2] == symmetric.Vertices[j + 2])
        assert(symmetric.Vertices[i + 1] == symmetric.Vertices[j + 1]);
  std::cout << "PASS: geometry node evaluation, cycles/budgets, project round "
               "trip, undo/redo, bake and sculpt\n";
}
