#pragma once
// Runs without a graphics context; test the exact packed CPU representation
// later uploaded to OpenGL, including edits/history that retain vector
// capacity.
static void CheckRenderCacheRegressions() {
  GameObject object("Render cache");
  auto mesh = Model::FromObject(object);
  Model::Polygonize(mesh);
  Model::Triangulate(mesh);
  mesh.UV.assign(mesh.Positions.size() / 3 * 2, .5f);
  mesh.Colors.assign(mesh.Positions.size(), 1.f);
  mesh.FaceMaterials.assign(mesh.Faces.size(), 0);
  StudioMaterial material;
  material.Color[0] = .6f;
  object.Materials.push_back(material);
  auto view = [&]() {
    return StudioGeometry::MeshView{
        mesh.Positions, mesh.Normals,       mesh.Colors, mesh.UV,
        mesh.Indices,   mesh.FaceMaterials, mesh.Faces};
  };
  StudioGeometry::Entry smooth;
  StudioGeometry::Build(smooth, object, view());
  assert(smooth.Batches.size() == 1 &&
         smooth.Elements.size() == mesh.Indices.size());
  assert(smooth.Vertices.size() == mesh.Positions.size() / 3);
  assert(StudioGeometry::Matches(smooth, object, view()));
  const auto color = smooth.Vertices[0].Color[0];
  assert(std::abs(color - material.Color[0] * object.Color[0]) < 1e-6f);
  object.Position[0] = 25;
  object.Rotation[1] = 43;
  object.Scale[2] = 2;
  assert(StudioGeometry::Matches(smooth, object, view()));
  object.Color[0] += .05f;
  assert(!StudioGeometry::Matches(smooth, object, view()));
  object.Color[0] -= .05f;
  // Use saved bytes to avoid float round-trip error in comparison tests.
  std::copy(smooth.ObjectColor, smooth.ObjectColor + 3, object.Color);
  mesh.Positions[0] += .25f;
  assert(!StudioGeometry::Matches(smooth, object, view()));
  mesh.Positions = smooth.Positions;
  mesh.UV[0] = .75f;
  assert(!StudioGeometry::Matches(smooth, object, view()));
  mesh.UV = smooth.UV;
  mesh.Normals[0] = .125f;
  assert(!StudioGeometry::Matches(smooth, object, view()));
  mesh.Normals = smooth.Normals;
  object.Materials[0].Metallic = 1;
  assert(StudioGeometry::Matches(smooth, object, view()));
  object.Materials[0].Color[1] = .25f;
  assert(!StudioGeometry::Matches(smooth, object, view()));
  object.Materials[0] = material;
  object.Materials.push_back(StudioMaterial());
  mesh.FaceMaterials[0] = 1;
  StudioGeometry::Entry split;
  StudioGeometry::Build(split, object, view());
  assert(split.Batches.size() == 2 &&
         split.Elements.size() == mesh.Indices.size());
  for (auto index : split.Elements)
    assert(index < split.Vertices.size());
  mesh.Normals.clear();
  StudioGeometry::Entry flat;
  StudioGeometry::Build(flat, object, view());
  assert(flat.Vertices.size() == mesh.Indices.size());
  for (size_t i = 0; i < flat.Elements.size(); i += 3) {
    const auto &a = flat.Vertices[flat.Elements[i]];
    const auto &b = flat.Vertices[flat.Elements[i + 1]];
    const auto &c = flat.Vertices[flat.Elements[i + 2]];
    assert(!std::memcmp(a.Normal, b.Normal, sizeof(a.Normal)) &&
           !std::memcmp(a.Normal, c.Normal, sizeof(a.Normal)));
  }
  // Conservative six-plane culling: boundary contact stays visible, including
  // mirrored/nonuniform transforms. Invalid transforms fail open.
  const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  object.Position[0] = object.Position[1] = object.Position[2] = 0;
  object.Rotation[0] = object.Rotation[1] = object.Rotation[2] = 0;
  object.Scale[0] = object.Scale[1] = object.Scale[2] = 1;
  SetStudioRenderFrustum(identity);
  assert(StudioGeometry::Visible(smooth, object));
  for (int axis = 0; axis < 3; ++axis)
    for (int sign = -1; sign <= 1; sign += 2) {
      object.Position[axis] = sign * 1.5f;
      assert(StudioGeometry::Visible(smooth, object));
      object.Position[axis] = sign * 2.f;
      assert(!StudioGeometry::Visible(smooth, object));
      object.Position[axis] = 0;
    }
  object.Scale[0] = -2;
  object.Scale[1] = 3;
  object.Scale[2] = .5f;
  object.Position[0] = 2;
  assert(StudioGeometry::Visible(smooth, object));
  object.Position[0] = 2.25f;
  assert(!StudioGeometry::Visible(smooth, object));
  object.Rotation[2] = 45;
  assert(StudioGeometry::Visible(smooth, object));
  object.Scale[0] = std::numeric_limits<float>::quiet_NaN();
  assert(StudioGeometry::Visible(smooth, object));
  SetStudioRenderFrustum(nullptr);
  object.Scale[0] = object.Scale[1] = object.Scale[2] = 1;
  object.Rotation[2] = 0;
  object.Position[0] = 0;
  // No GL allocations are attempted in this context-free cache lifetime test.
  assert(!SDL_GL_GetCurrentContext());
  StudioGeometry::Clear();
  StudioGeometry::g_Functions = StudioGeometry::Functions();
  ++StudioGeometry::g_Frame;
  auto *cached = StudioGeometry::Get(object, view());
  assert(cached && StudioGeometry::g_Bytes <= StudioGeometry::Budget);
  const auto misses = g_StudioRenderStats.CacheMisses;
  object.Position[1] = 5;
  assert(StudioGeometry::Get(object, view()) == cached &&
         g_StudioRenderStats.CacheMisses == misses);
  EndStudioRenderFrame();
  assert(!StudioGeometry::g_Entries.empty());
  ++StudioGeometry::g_Frame;
  EndStudioRenderFrame();
  assert(StudioGeometry::g_Entries.empty() && StudioGeometry::g_Bytes == 0);
  std::cout << "PASS: cached indexed material batches, flat normals, exact "
               "edit invalidation and cache lifetime\n";
}
