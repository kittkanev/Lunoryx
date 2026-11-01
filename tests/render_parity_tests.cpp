// Real OpenGL 2.1 parity and renderer submission benchmark. This intentionally
// isolates mesh submission from UI, shadows and post-processing. Timings
// include glFinish and FFI/driver work; CSV stdout is consumed by release
// reports.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "../scene_tools.h"
#include "../mesh_modeling.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
struct TestImage {
  int width = 2, height = 2;
  std::vector<unsigned char> image = {255, 0, 0,   255, 0,   255, 0,   255,
                                      0,   0, 255, 255, 255, 255, 255, 255};
};
static TestImage DecodeMaterialImage(const StudioMaterial &) {
  return TestImage();
}
static void AppendConsole(const std::string &text) { std::cerr << text; }
struct ShadowStub {
  void Material(bool, float, float) {}
};
static ShadowStub g_SceneShadows;
#define LUNORYX_RENDER_PARITY_TEST
#include "../material_render.h"
#include "render_cache_regressions.h"

static void Draw(const GameObject &object, bool cached) {
  BeginStudioRenderFrame();
  glViewport(0, 0, 256, 256);
  glClearColor(.1f, .2f, .3f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.3, 1.3, -1.3, 1.3, -10, 10);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glRotatef(30, 1, 0, 0);
  glRotatef(20, 0, 1, 0);
  if (cached)
    RenderLitMesh(object);
  else
    RenderLitMeshLegacy(object);
  glFinish();
  EndStudioRenderFrame();
  assert(glGetError() == GL_NO_ERROR);
}
static std::vector<unsigned char> Pixels() {
  std::vector<unsigned char> result(256 * 256 * 4);
  glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  return result;
}
static void RequireParity(const std::vector<unsigned char> &expected,
                          const std::vector<unsigned char> &actual) {
  assert(expected.size() == actual.size());
  int maximum = 0;
  for (size_t i = 0; i < expected.size(); ++i)
    maximum = std::max(maximum, std::abs(int(expected[i]) - int(actual[i])));
  if (maximum > 1)
    std::cerr << "Renderer parity failed: maximum channel difference "
              << maximum << "\n";
  assert(maximum <= 1);
}
static void ForceClientArrays() {
  ClearStudioGeometry();
  BeginStudioRenderFrame();
  StudioGeometry::g_Functions.Gen = nullptr;
  EndStudioRenderFrame();
}
static void ParityTests() {
  GameObject object("Parity");
  auto mesh = Model::FromObject(object);
  Model::Polygonize(mesh);
  Model::Triangulate(mesh);
  mesh.UV.assign(mesh.Positions.size() / 3 * 2, .5f);
  mesh.FaceMaterials.assign(mesh.Faces.size(), 0);
  for (size_t i = 0; i < mesh.FaceMaterials.size(); i += 2)
    mesh.FaceMaterials[i] = 1;
  StudioMaterial a, b;
  a.Color[0] = .5f;
  b.Color[1] = .2f;
  object.Materials = {a, b};
  for (int scenario = 0; scenario < 4; ++scenario) {
    if (scenario == 1)
      mesh.Normals.clear();
    if (scenario == 2)
      object.Materials[0].Texture = "synthetic-checker";
    if (scenario == 3) {
      mesh.Positions[0] += .2f;
      object.Color[1] = .3f;
    }
    Model::ToObject(object, mesh);
    ClearStudioGeometry();
    Draw(object, false);
    auto expected = Pixels();
    Draw(object, true);
    RequireParity(expected, Pixels());
    assert(g_StudioRenderStats.DrawCalls == 2);
    ForceClientArrays();
    Draw(object, true);
    RequireParity(expected, Pixels());
    assert(!g_StudioRenderStats.VBO && g_StudioRenderStats.DrawCalls == 2);
    ClearStudioGeometry();
  }
  // Actual GL buffer destruction, context-independent CPU edits and cache
  // reuse.
  Draw(object, true);
  assert(g_StudioRenderStats.CacheMisses == 1);
  Draw(object, true);
  assert(g_StudioRenderStats.CacheHits == 1);
  object.Position[0] = 3;
  Draw(object, true);
  assert(g_StudioRenderStats.CacheHits == 1);
  object.Vertices[0] += .2f;
  Draw(object, true);
  assert(g_StudioRenderStats.CacheMisses == 1);
  ClearStudioGeometry();
  assert(StudioGeometry::g_Entries.empty());
  std::cerr << "PASS: OpenGL legacy/VBO/client-array pixel parity, textured "
               "flat/smooth materials and in-place edits\n";
}
static GameObject Grid() {
  StudioMesh mesh;
  const uint32_t side = 96;
  for (uint32_t y = 0; y < side; ++y)
    for (uint32_t x = 0; x < side; ++x) {
      mesh.Positions.insert(
          mesh.Positions.end(),
          {float(x) * 2 / (side - 1) - 1, float(y) * 2 / (side - 1) - 1, 0});
      mesh.UV.insert(mesh.UV.end(),
                     {float(x) / (side - 1), float(y) / (side - 1)});
    }
  for (uint32_t y = 0; y + 1 < side; ++y)
    for (uint32_t x = 0; x + 1 < side; ++x) {
      uint32_t i = y * side + x;
      mesh.Faces.push_back({i, i + 1, i + side + 1, i + side});
    }
  Model::Triangulate(mesh);
  GameObject object("Submission benchmark");
  Model::ToObject(object, mesh);
  object.Materials.push_back(StudioMaterial());
  return object;
}
static void Measure(const GameObject &object, const char *name, bool cached,
                    bool cold) {
  std::vector<double> samples;
  for (int run = 0; run < 5; ++run) {
    if (cold)
      ClearStudioGeometry();
    if (!cold)
      Draw(object, cached); // warm-up and driver cache excluded
    auto start = std::chrono::steady_clock::now();
    const int repeats = cold ? 1 : 8;
    for (int i = 0; i < repeats; ++i)
      Draw(object, cached);
    auto end = std::chrono::steady_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::milli>(end - start).count() /
        repeats);
  }
  std::sort(samples.begin(), samples.end());
  std::cout << "mesh_submission," << name << "," << object.Indices.size() / 3
            << "," << g_StudioRenderStats.DrawCalls << "," << std::fixed
            << std::setprecision(6) << samples[2] << ","
            << g_StudioRenderStats.CacheBytes << ","
            << int(cached && g_StudioRenderStats.VBO) << "\n";
}
int main() {
  // This helper prints PASS to cout; keep all diagnostics away from CSV stdout.
  std::ostringstream diagnostics;
  auto *old = std::cout.rdbuf(diagnostics.rdbuf());
  CheckRenderCacheRegressions();
  std::cout.rdbuf(old);
  std::cerr << diagnostics.str();
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << SDL_GetError() << "\n";
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Render parity", 0, 0, 256, 256,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  if (!window) {
    std::cerr << SDL_GetError() << "\n";
    return 1;
  }
  auto context = SDL_GL_CreateContext(window);
  if (!context) {
    std::cerr << SDL_GetError() << "\n";
    return 1;
  }
  SDL_GL_SetSwapInterval(0);
  const auto *renderer = glGetString(GL_RENDERER);
  std::cerr << "OpenGL renderer: "
            << (renderer ? reinterpret_cast<const char *>(renderer) : "unknown")
            << "\n";
  ParityTests();
  auto grid = Grid();
  std::cout << "scenario,implementation,triangles,draw_calls,median_ms,cache_"
               "bytes,vbo\n";
  ClearStudioGeometry();
  Measure(grid, "cpp_1.0.1_immediate", false, false);
  ClearStudioGeometry();
  Measure(grid, "cached_vbo_warm", true, false);
  Measure(grid, "cached_vbo_cold", true, true);
  ForceClientArrays();
  Measure(grid, "cached_client_arrays_warm", true, false);
  ClearStudioTextures();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}
