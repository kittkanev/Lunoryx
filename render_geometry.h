#pragma once
// Fixed-function OpenGL 2.1 batching. Source snapshots make in-place editing,
// undo and restored Play geometry invalidate reliably without changing
// projects.
namespace StudioGeometry {
struct Vertex {
  float Position[3], Normal[3], Color[3], UV[2];
};
struct Batch {
  uint32_t Material;
  size_t Offset, Count;
};
struct Entry {
  std::vector<float> Positions, Normals, Colors, UV, MaterialColors;
  std::vector<uint32_t> Indices, FaceMaterials, FaceSizes;
  float ObjectColor[3];
  Bounds LocalBounds;
  bool HasBounds = false;
  std::vector<Vertex> Vertices;
  std::vector<uint32_t> Elements;
  std::vector<Batch> Batches;
  GLuint VertexBuffer = 0, IndexBuffer = 0;
  size_t Bytes = 0;
  uint64_t Seen = 0;
};
struct Functions {
  typedef void(APIENTRY *GenFn)(GLsizei, GLuint *);
  typedef void(APIENTRY *BindFn)(GLenum, GLuint);
  typedef void(APIENTRY *DataFn)(GLenum, std::ptrdiff_t, const void *, GLenum);
  typedef void(APIENTRY *DeleteFn)(GLsizei, const GLuint *);
  typedef void(APIENTRY *ClientTextureFn)(GLenum);
  typedef void(APIENTRY *GetFn)(GLenum, GLenum, GLint *);
  SDL_GLContext Context = nullptr;
  GenFn Gen = nullptr;
  BindFn Bind = nullptr;
  DataFn Data = nullptr;
  DeleteFn Delete = nullptr;
  ClientTextureFn ClientTexture = nullptr;
  GetFn Get = nullptr;
  bool Available() const { return Gen && Bind && Data && Delete && Get; }
};
static Functions g_Functions;
static std::map<uint64_t, Entry> g_Entries;
static uint64_t g_Frame = 0;
static size_t g_Bytes = 0;
static const size_t Budget = 128 * 1024 * 1024;
static bool g_FrustumEnabled = false;
static float g_ViewProjection[16];
static bool Visible(const Entry &entry, const GameObject &object) {
  if (!g_FrustumEnabled || !entry.HasBounds)
    return true;
  // The cached local bounds need only eight transforms; the physics helper
  // computes rotation coefficients once. Its world AABB is conservative for
  // rotated/nonuniform/mirrored objects, so plane support points cannot falsely
  // reject visible geometry.
  Bounds world;
  if (!WorldBoundsFromLocal(object, entry.LocalBounds.Min,
                            entry.LocalBounds.Max, world))
    return true;
  for (int axis = 0; axis < 3; ++axis)
    for (int sign = -1; sign <= 1; sign += 2) {
      double plane[4];
      for (int c = 0; c < 4; ++c)
        plane[c] = double(g_ViewProjection[c * 4 + 3]) +
                   sign * double(g_ViewProjection[c * 4 + axis]);
      const double terms[4] = {
          plane[0] * (plane[0] >= 0 ? world.Max.x : world.Min.x),
          plane[1] * (plane[1] >= 0 ? world.Max.y : world.Min.y),
          plane[2] * (plane[2] >= 0 ? world.Max.z : world.Min.z), plane[3]};
      const double distance = terms[0] + terms[1] + terms[2] + terms[3];
      const double margin =
          1e-4 * (std::abs(terms[0]) + std::abs(terms[1]) + std::abs(terms[2]) +
                  std::abs(terms[3]) + 1);
      if (std::isfinite(distance) && distance < -margin)
        return false;
    }
  return true;
}

static void Release(Entry &entry) {
  if (g_Functions.Available()) {
    if (entry.VertexBuffer)
      g_Functions.Delete(1, &entry.VertexBuffer);
    if (entry.IndexBuffer)
      g_Functions.Delete(1, &entry.IndexBuffer);
  }
  entry.VertexBuffer = entry.IndexBuffer = 0;
}
static void Clear() {
  for (auto &entry : g_Entries)
    Release(entry.second);
  g_Entries.clear();
  g_Bytes = 0;
}
static void Initialize() {
  SDL_GLContext context = SDL_GL_GetCurrentContext();
  if (context == g_Functions.Context)
    return;
  // A new context cannot delete buffers owned by an already lost context.
  g_Entries.clear();
  g_Bytes = 0;
  g_Functions = Functions();
  g_Functions.Context = context;
  if (!context)
    return;
  g_Functions.Gen =
      reinterpret_cast<Functions::GenFn>(SDL_GL_GetProcAddress("glGenBuffers"));
  g_Functions.Bind = reinterpret_cast<Functions::BindFn>(
      SDL_GL_GetProcAddress("glBindBuffer"));
  g_Functions.Data = reinterpret_cast<Functions::DataFn>(
      SDL_GL_GetProcAddress("glBufferData"));
  g_Functions.Delete = reinterpret_cast<Functions::DeleteFn>(
      SDL_GL_GetProcAddress("glDeleteBuffers"));
  g_Functions.Get = reinterpret_cast<Functions::GetFn>(
      SDL_GL_GetProcAddress("glGetBufferParameteriv"));
  g_Functions.ClientTexture = reinterpret_cast<Functions::ClientTextureFn>(
      SDL_GL_GetProcAddress("glClientActiveTexture"));
}
template <class T>
static bool Same(const std::vector<T> &a, const std::vector<T> &b) {
  return a.size() == b.size() &&
         (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(T)));
}
// No mesh copy is made in the draw path. The references below expose the same
// vectors used by the old renderer while keeping the cache independent of their
// addresses (vector reallocation is not a reliable revision marker).
struct MeshView {
  const std::vector<float> &Positions, &Normals, &Colors, &UV;
  const std::vector<uint32_t> &Indices, &FaceMaterials;
  const std::vector<std::vector<uint32_t>> &Faces;
};
static bool Matches(const Entry &e, const GameObject &o, const MeshView &m) {
  if (!Same(e.Positions, m.Positions) || !Same(e.Indices, m.Indices) ||
      !Same(e.Normals, m.Normals) || !Same(e.Colors, m.Colors) ||
      !Same(e.UV, m.UV) || !Same(e.FaceMaterials, m.FaceMaterials) ||
      e.FaceSizes.size() != m.Faces.size() ||
      e.MaterialColors.size() != o.Materials.size() * 3 ||
      std::memcmp(e.ObjectColor, o.Color, sizeof(e.ObjectColor)))
    return false;
  for (size_t i = 0; i < m.Faces.size(); ++i)
    if (e.FaceSizes[i] != m.Faces[i].size())
      return false;
  for (size_t i = 0; i < o.Materials.size(); ++i)
    if (std::memcmp(e.MaterialColors.data() + i * 3, o.Materials[i].Color,
                    3 * sizeof(float)))
      return false;
  return true;
}
static void Build(Entry &entry, const GameObject &object,
                  const MeshView &mesh) {
  if (mesh.Positions.size() % 3 || mesh.Indices.size() % 3 ||
      (!mesh.Normals.empty() && mesh.Normals.size() != mesh.Positions.size()) ||
      (!mesh.Colors.empty() && mesh.Colors.size() != mesh.Positions.size()) ||
      (!mesh.UV.empty() && mesh.UV.size() != mesh.Positions.size() / 3 * 2) ||
      (!mesh.FaceMaterials.empty() &&
       mesh.FaceMaterials.size() != mesh.Faces.size()))
    throw std::runtime_error("Invalid cached mesh attributes");
  if (!mesh.Positions.empty()) {
    entry.LocalBounds.Min = entry.LocalBounds.Max = {
        mesh.Positions[0], mesh.Positions[1], mesh.Positions[2]};
    for (size_t i = 3; i < mesh.Positions.size(); i += 3) {
      entry.LocalBounds.Min.x =
          std::min(entry.LocalBounds.Min.x, mesh.Positions[i]);
      entry.LocalBounds.Max.x =
          std::max(entry.LocalBounds.Max.x, mesh.Positions[i]);
      entry.LocalBounds.Min.y =
          std::min(entry.LocalBounds.Min.y, mesh.Positions[i + 1]);
      entry.LocalBounds.Max.y =
          std::max(entry.LocalBounds.Max.y, mesh.Positions[i + 1]);
      entry.LocalBounds.Min.z =
          std::min(entry.LocalBounds.Min.z, mesh.Positions[i + 2]);
      entry.LocalBounds.Max.z =
          std::max(entry.LocalBounds.Max.z, mesh.Positions[i + 2]);
    }
    entry.HasBounds = true;
  }
  entry.Positions = mesh.Positions;
  entry.Indices = mesh.Indices;
  entry.Normals = mesh.Normals;
  entry.Colors = mesh.Colors;
  entry.UV = mesh.UV;
  entry.FaceMaterials = mesh.FaceMaterials;
  std::copy(object.Color, object.Color + 3, entry.ObjectColor);
  for (const auto &material : object.Materials)
    entry.MaterialColors.insert(entry.MaterialColors.end(), material.Color,
                                material.Color + 3);
  for (const auto &face : mesh.Faces)
    entry.FaceSizes.push_back(uint32_t(face.size()));
  std::map<uint32_t, std::vector<uint32_t>> groups;
  size_t triangle = 0, faces = mesh.Faces.empty() ? mesh.Indices.size() / 3
                                                  : mesh.Faces.size();
  for (size_t face = 0; face < faces; ++face) {
    uint32_t slot = mesh.FaceMaterials.empty() ? 0 : mesh.FaceMaterials[face];
    size_t triangles =
        mesh.Faces.empty()
            ? 1
            : (mesh.Faces[face].size() >= 3 ? mesh.Faces[face].size() - 2 : 0);
    if (triangles > mesh.Indices.size() / 3 - triangle)
      throw std::runtime_error("Invalid cached polygon triangles");
    auto &group = groups[slot];
    for (size_t t = 0; t < triangles; ++t, ++triangle)
      for (int k = 0; k < 3; ++k)
        group.push_back(mesh.Indices[triangle * 3 + k]);
  }
  if (triangle != mesh.Indices.size() / 3)
    throw std::runtime_error("Invalid cached polygon count");
  const bool smooth = mesh.Normals.size() == mesh.Positions.size();
  for (const auto &group : groups) {
    Batch batch = {group.first, entry.Elements.size(), group.second.size()};
    std::vector<uint32_t> remap(smooth ? mesh.Positions.size() / 3 : 0,
                                UINT32_MAX);
    for (size_t i = 0; i < group.second.size(); ++i) {
      uint32_t source = group.second[i];
      if (source >= mesh.Positions.size() / 3)
        throw std::runtime_error("Invalid cached vertex index");
      if (smooth && remap[source] != UINT32_MAX) {
        entry.Elements.push_back(remap[source]);
        continue;
      }
      Vertex vertex;
      std::copy(mesh.Positions.data() + source * 3,
                mesh.Positions.data() + source * 3 + 3, vertex.Position);
      Vec3 normal = {0, 1, 0};
      if (smooth)
        std::copy(mesh.Normals.data() + source * 3,
                  mesh.Normals.data() + source * 3 + 3, vertex.Normal);
      else {
        size_t base = i - i % 3;
        uint32_t a = group.second[base], b = group.second[base + 1],
                 c = group.second[base + 2];
        if (a >= mesh.Positions.size() / 3 || b >= mesh.Positions.size() / 3 ||
            c >= mesh.Positions.size() / 3)
          throw std::runtime_error("Invalid cached normal index");
        normal = Model::Unit(
            Cross({mesh.Positions[b * 3] - mesh.Positions[a * 3],
                   mesh.Positions[b * 3 + 1] - mesh.Positions[a * 3 + 1],
                   mesh.Positions[b * 3 + 2] - mesh.Positions[a * 3 + 2]},
                  {mesh.Positions[c * 3] - mesh.Positions[a * 3],
                   mesh.Positions[c * 3 + 1] - mesh.Positions[a * 3 + 1],
                   mesh.Positions[c * 3 + 2] - mesh.Positions[a * 3 + 2]}));
        vertex.Normal[0] = normal.x;
        vertex.Normal[1] = normal.y;
        vertex.Normal[2] = normal.z;
      }
      for (int k = 0; k < 3; ++k) {
        const float material = group.first < object.Materials.size()
                                   ? object.Materials[group.first].Color[k]
                                   : object.Color[k];
        vertex.Color[k] =
            material * (object.Materials.empty() ? 1 : object.Color[k]) *
            (mesh.Colors.empty() ? 1 : mesh.Colors[source * 3 + k]);
      }
      vertex.UV[0] = mesh.UV.empty() ? 0 : mesh.UV[source * 2];
      vertex.UV[1] = mesh.UV.empty() ? 0 : mesh.UV[source * 2 + 1];
      uint32_t index = uint32_t(entry.Vertices.size());
      entry.Vertices.push_back(vertex);
      entry.Elements.push_back(index);
      if (smooth)
        remap[source] = index;
    }
    entry.Batches.push_back(batch);
  }
  const size_t sourceBytes =
      (entry.Positions.capacity() + entry.Normals.capacity() +
       entry.Colors.capacity() + entry.UV.capacity() +
       entry.MaterialColors.capacity()) *
          sizeof(float) +
      (entry.Indices.capacity() + entry.FaceMaterials.capacity() +
       entry.FaceSizes.capacity()) *
          sizeof(uint32_t) +
      entry.Batches.capacity() * sizeof(Batch);
  entry.Bytes = sourceBytes + entry.Vertices.capacity() * sizeof(Vertex) +
                entry.Elements.capacity() * sizeof(uint32_t);
}
static Entry *Get(const GameObject &object, const MeshView &mesh) {
  auto existing = g_Entries.find(object.Id);
  if (existing != g_Entries.end() && Matches(existing->second, object, mesh)) {
    existing->second.Seen = g_Frame;
    ++g_StudioRenderStats.CacheHits;
    return &existing->second;
  }
  ++g_StudioRenderStats.CacheMisses;
  if (existing != g_Entries.end()) {
    g_Bytes -= existing->second.Bytes;
    Release(existing->second);
    g_Entries.erase(existing);
  }
  Entry candidate;
  Build(candidate, object, mesh);
  candidate.Seen = g_Frame;
  if (candidate.Bytes > Budget)
    return nullptr;
  while (g_Bytes + candidate.Bytes > Budget && !g_Entries.empty()) {
    auto oldest = g_Entries.begin();
    for (auto it = g_Entries.begin(); it != g_Entries.end(); ++it)
      if (it->second.Seen < oldest->second.Seen)
        oldest = it;
    g_Bytes -= oldest->second.Bytes;
    Release(oldest->second);
    g_Entries.erase(oldest);
  }
  if (g_Functions.Available()) {
    g_Functions.Gen(1, &candidate.VertexBuffer);
    g_Functions.Gen(1, &candidate.IndexBuffer);
    if (candidate.VertexBuffer && candidate.IndexBuffer) {
      g_Functions.Bind(GL_ARRAY_BUFFER, candidate.VertexBuffer);
      g_Functions.Data(
          GL_ARRAY_BUFFER,
          std::ptrdiff_t(candidate.Vertices.size() * sizeof(Vertex)),
          candidate.Vertices.data(), GL_STATIC_DRAW);
      g_Functions.Bind(GL_ELEMENT_ARRAY_BUFFER, candidate.IndexBuffer);
      g_Functions.Data(
          GL_ELEMENT_ARRAY_BUFFER,
          std::ptrdiff_t(candidate.Elements.size() * sizeof(uint32_t)),
          candidate.Elements.data(), GL_STATIC_DRAW);
      GLint vertexBytes = 0, indexBytes = 0;
      g_Functions.Get(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vertexBytes);
      g_Functions.Get(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexBytes);
      g_Functions.Bind(GL_ARRAY_BUFFER, 0);
      g_Functions.Bind(GL_ELEMENT_ARRAY_BUFFER, 0);
      if (size_t(vertexBytes) == candidate.Vertices.size() * sizeof(Vertex) &&
          size_t(indexBytes) == candidate.Elements.size() * sizeof(uint32_t)) {
        g_StudioRenderStats.UploadedBytes +=
            candidate.Vertices.size() * sizeof(Vertex) +
            candidate.Elements.size() * sizeof(uint32_t);
        std::vector<Vertex>().swap(candidate.Vertices);
        std::vector<uint32_t>().swap(candidate.Elements);
      } else
        Release(candidate);
    } else
      Release(candidate);
  }
  try {
    auto inserted = g_Entries.emplace(object.Id, std::move(candidate));
    g_Bytes += inserted.first->second.Bytes;
    return &inserted.first->second;
  } catch (...) {
    Release(candidate);
    throw;
  }
}
} // namespace StudioGeometry

static void ClearStudioGeometry() {
  StudioGeometry::Clear();
  StudioGeometry::g_Functions = StudioGeometry::Functions();
}
inline void SetStudioRenderFrustum(const float viewProjection[16]) {
  StudioGeometry::g_FrustumEnabled = false;
  if (!viewProjection)
    return;
  for (int i = 0; i < 16; ++i)
    if (!std::isfinite(viewProjection[i]))
      return;
  std::copy(viewProjection, viewProjection + 16,
            StudioGeometry::g_ViewProjection);
  StudioGeometry::g_FrustumEnabled = true;
}
inline void BeginStudioRenderFrame() {
  StudioGeometry::g_FrustumEnabled = false;
  StudioGeometry::Initialize();
  ++StudioGeometry::g_Frame;
  g_StudioRenderStats = StudioRenderStats();
  g_StudioRenderStats.VBO = StudioGeometry::g_Functions.Available();
}
inline void EndStudioRenderFrame() {
  using namespace StudioGeometry;
  for (auto it = g_Entries.begin(); it != g_Entries.end();) {
    if (it->second.Seen != g_Frame) {
      g_Bytes -= it->second.Bytes;
      Release(it->second);
      it = g_Entries.erase(it);
    } else
      ++it;
  }
  g_StudioRenderStats.CacheBytes = g_Bytes;
}
