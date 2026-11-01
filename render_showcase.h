#pragma once
// Reproducible editor scene used for actual release screenshots.
static void CreateRenderShowcase() {
  g_Objects.clear();
  GameObject floor("Studio floor");
  floor.Scale[0] = floor.Scale[2] = 16;
  floor.Scale[1] = .25f;
  floor.Position[1] = -.125f;
  floor.Color[0] = .18f;
  floor.Color[1] = .24f;
  floor.Color[2] = .29f;
  floor.Roughness = .9f;
  g_Objects.push_back(floor);
  for (int i = 0; i < 5; ++i) {
    GameObject object(i == 0   ? "Polished sphere"
                      : i == 1 ? "Beveled copper"
                      : i == 2 ? "Cyan column"
                      : i == 3 ? "Violet sphere"
                               : "Gold pyramid",
                      i == 0 || i == 3 ? MeshType::Sphere
                      : i == 2         ? MeshType::Cylinder
                      : i == 4         ? MeshType::Pyramid
                                       : MeshType::Cube);
    object.Position[0] = float(i - 2) * 1.5f;
    object.Position[1] = .8f;
    object.Position[2] = (i % 2) * .7f;
    object.Scale[0] = object.Scale[1] = object.Scale[2] = 1.3f;
    const float colors[5][3] = {{.7f, .85f, .92f},
                                {.8f, .38f, .17f},
                                {.12f, .66f, .72f},
                                {.5f, .25f, .74f},
                                {.95f, .7f, .15f}};
    std::copy(colors[i], colors[i] + 3, object.Color);
    object.Metallic = i < 2 ? .8f : .15f;
    object.Roughness = i < 2 ? .18f : .55f;
    if (i == 1) {
      auto mesh = Model::FromObject(object);
      Model::Polygonize(mesh);
      Model::Bevel(mesh, .12f);
      Model::ToObject(object, mesh);
      object.Rotation[1] = 20;
    }
    g_Objects.push_back(std::move(object));
  }
  GameObject plinth("Exhibit plinth");
  plinth.Scale[0] = 8;
  plinth.Scale[1] = .3f;
  plinth.Scale[2] = 3;
  plinth.Position[1] = .15f;
  plinth.Color[0] = .25f;
  plinth.Color[1] = .32f;
  plinth.Color[2] = .38f;
  g_Objects.push_back(plinth);
  g_SelectedObjectIndex = -1;
  g_ShowGrid = false;
  g_ShowAxes = false;
  g_RenderReport = true;
  g_Preferences.ShowSettings = true;
  g_Studio.RequestTool = 3;
  ClearConsole();
  AppendConsole("Experimental renderer: cached indexed meshes, scalable scene "
                "resolution and optional FXAA.\n");
}

static void CreateNodeShowcase() {
  g_Objects.clear();
  CreateNodeObject();
  auto graph = GeometryNodes::Default();
  graph[0].Type = StudioGeometryNode::Sphere;
  graph[0].X = 0;
  graph[0].Y = 20;
  graph[1].Type = StudioGeometryNode::Noise;
  graph[1].Amount = .16f;
  graph[1].Frequency = 2;
  graph[1].X = 230;
  graph[1].Y = 20;
  StudioGeometryNode array;
  array.Type = StudioGeometryNode::Array;
  array.Id = 4;
  array.Input = 2;
  array.Count = 3;
  array.Vector[0] = 2;
  array.Vector[1] = array.Vector[2] = 0;
  array.X = 460;
  array.Y = 20;
  graph[2].Input = 4;
  graph[2].X = 690;
  graph[2].Y = 20;
  graph.push_back(array);
  GeometryNodes::Apply(g_Objects[0], graph);
  g_Objects[0].Position[0] = -2;
  g_NodeObject = 0;
  GameObject floor("Node preview floor");
  floor.Scale[0] = 12;
  floor.Scale[1] = .2f;
  floor.Scale[2] = 10;
  floor.Position[1] = -.1f;
  floor.Color[0] = .2f;
  floor.Color[1] = .26f;
  floor.Color[2] = .3f;
  g_Objects.push_back(floor);
  g_ReportNodes = true;
  g_ShowNodes = true;
  g_ShowGrid = false;
  g_Preferences.ShowSettings = false;
  ClearConsole();
}
static void CreateSculptShowcase() {
  g_Objects.clear();
  GameObject clay("Sculpted clay", MeshType::Sphere);
  if (!MakeEditable(clay, 2))
    throw std::runtime_error("Sculpt showcase exceeds budget");
  SculptBrush(clay, {0, .5f, 0}, .6f, .3f, 0);
  SculptBrush(clay, {.4f, .1f, 0}, .45f, .18f, 0);
  clay.Color[0] = .18f;
  clay.Color[1] = .67f;
  clay.Color[2] = .72f;
  clay.Scale[0] = clay.Scale[1] = clay.Scale[2] = 3;
  clay.Position[1] = 1.8f;
  g_Objects.push_back(std::move(clay));
  SelectObject(0);
  g_Studio.RequestTool = 1;
  g_Studio.SculptEnabled = true;
  g_Studio.SculptSymmetry = true;
  g_Preferences.ShowSettings = false;
}
