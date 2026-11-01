#include "../viewport_target.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
static ViewportTarget::CheckFn NativeCheck = nullptr;
static int FailAfter = 0;
static GLenum APIENTRY InjectFramebufferFailure(GLenum target) {
  if (--FailAfter == 0)
    return GL_FRAMEBUFFER_UNSUPPORTED;
  return NativeCheck(target);
}

static void Scene(ViewportTarget &target) {
  target.BindFramebuffer(GL_FRAMEBUFFER, target.SceneFramebuffer(false));
  glViewport(0, 0, target.Width, target.Height);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DITHER);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glColor3f(1, 1, 1);
  glBegin(GL_TRIANGLES);
  glVertex2f(-.9f, -.8f);
  glVertex2f(.8f, -.5f);
  glVertex2f(.4f, .9f);
  glEnd();
  target.FinishScene(false);
}
static std::vector<unsigned char> Read(ViewportTarget &target) {
  target.BindFramebuffer(GL_FRAMEBUFFER, target.PostProcessEnabled()
                                             ? target.PresentFramebuffer
                                             : target.Framebuffer);
  std::vector<unsigned char> out(size_t(target.Width) * target.Height * 4);
  glReadPixels(0, 0, target.Width, target.Height, GL_RGBA, GL_UNSIGNED_BYTE,
               out.data());
  return out;
}
int main(int argc, char **argv) {
  SDL_SetMainReady();
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("post test", 0, 0, 128, 128,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  auto context = SDL_GL_CreateContext(window);
  assert(context);
  assert(g_ViewportTarget.Initialize());
  auto &target = g_ViewportTarget;
  assert(target.Resize(128, 128));
  GLuint stable = target.Color;
  Scene(target);
  auto before = Read(target);
  assert(target.SceneColor == 0 && target.PresentFramebuffer == 0);
  target.ConfigurePostProcess(1);
  if (!target.PostProcessEnabled()) {
    std::cerr << target.PostProcessError() << "\n";
    return 1;
  }
  assert(target.Resize(128, 128));
  assert(target.Color == stable && target.SceneColor &&
         target.PresentFramebuffer);
  target.BindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(3, 5, 20, 21);
  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glDepthMask(GL_TRUE);
  Scene(target);
  auto after = Read(target);
  size_t difference = 0;
  for (size_t i = 0; i < before.size(); ++i)
    difference += before[i] != after[i];
  assert(difference > 0);
  // Effect must preserve all matrices and render state around its full-screen
  // pass.
  target.BindFramebuffer(GL_FRAMEBUFFER, target.Framebuffer);
  glViewport(3, 5, 20, 21);
  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glDepthMask(GL_TRUE);
  GLfloat matrix[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  target.FinishScene(false);
  GLint viewport[4], framebuffer = 0;
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  assert(viewport[0] == 3 && viewport[1] == 5 && viewport[2] == 20 &&
         viewport[3] == 21 && GLuint(framebuffer) == target.Framebuffer);
  assert(glIsEnabled(GL_BLEND) && glIsEnabled(GL_SCISSOR_TEST));
  GLboolean depth = GL_FALSE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth);
  assert(depth);
  GLfloat afterMatrix[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, afterMatrix);
  for (int i = 0; i < 16; ++i)
    assert(matrix[i] == afterMatrix[i]);
  target.ConfigurePostProcess(2, 1.1f);
  assert(target.Resize(64, 96));
  assert(target.Color == stable);
  glDisable(GL_SCISSOR_TEST);
  Scene(target);
  target.ConfigurePostProcess(0);
  assert(target.Resize(64, 96));
  assert(!target.SceneColor && !target.PresentFramebuffer &&
         !target.Post.Program);
  assert(target.SceneFramebuffer(true) == 0);
  target.ConfigurePostProcess(0, 1, true);
  assert(target.SceneFramebuffer(true) == target.Framebuffer);
  assert(target.Resize(64, 64));
  glDisable(GL_SCISSOR_TEST);
  Scene(target);
  target.FinishScene(true, 128, 128);
  target.BindFramebuffer(GL_FRAMEBUFFER, 0);
  unsigned char expanded[4] = {};
  glReadPixels(70, 80, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, expanded);
  assert(expanded[0] > 200 && expanded[1] > 200 && expanded[2] > 200);
  assert(!target.SceneColor && !target.PresentFramebuffer &&
         !target.Post.Program);

  // Both the presentation and raw-scene FBO failure paths must return to
  // direct rendering, preserving the public ImGui texture and freeing extras.
  for (int failAt : {1, 2}) {
    target.BindFramebuffer(GL_FRAMEBUFFER, 0);
    target.Post.Destroy();
    target.ConfigurePostProcess(1);
    NativeCheck = target.Check;
    FailAfter = failAt;
    target.Check = InjectFramebufferFailure;
    assert(target.Resize(64, 64));
    target.Check = NativeCheck;
    assert(!target.PostProcessEnabled() && !target.PostProcessError().empty());
    assert(target.Color == stable && !target.SceneColor &&
           !target.PresentFramebuffer);
    target.ConfigurePostProcess(0);
  }
  target.Post.Destroy();
  assert(glGetError() == GL_NO_ERROR);
  if (argc > 1 && std::strcmp(argv[1], "--bench") == 0) {
    std::cout << "postprocess_mode,width,height,median_frame_ms\n";
    for (int mode = 0; mode <= 2; ++mode) {
      target.ConfigurePostProcess(mode, 1);
      assert(target.Resize(1280, 720));
      std::vector<double> times;
      for (int sample = 0; sample < 5; ++sample) {
        auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < 10; ++frame) {
          Scene(target);
          glFinish();
        }
        auto end = std::chrono::steady_clock::now();
        times.push_back(
            std::chrono::duration<double, std::milli>(end - start).count() /
            10);
      }
      std::sort(times.begin(), times.end());
      std::cout << mode << ",1280,720," << times[2] << "\n";
    }
  }
  target.Destroy();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  std::cerr << "PASS: GL2.1 FXAA, LDR pass, stable ImGui texture, resize, "
               "state restoration and Office resource release ("
            << difference << " changed channels)\n";
}
