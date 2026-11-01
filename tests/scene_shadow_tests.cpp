#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "../scene_shadows.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
void geometry() {
  glColor3f(.8, .8, .8);
  glNormal3f(0, 1, 0);
  glBegin(GL_QUADS);
  glVertex3f(-8, 0, -8);
  glVertex3f(-8, 0, 8);
  glVertex3f(8, 0, 8);
  glVertex3f(8, 0, -8);
  glEnd();
  glColor3f(.3, .6, .9);
  glBegin(GL_QUADS);
  glNormal3f(0, 0, 1);
  glVertex3f(-1, 0, 1);
  glVertex3f(1, 0, 1);
  glVertex3f(1, 2, 1);
  glVertex3f(-1, 2, 1);
  glNormal3f(0, 0, -1);
  glVertex3f(1, 0, -1);
  glVertex3f(-1, 0, -1);
  glVertex3f(-1, 2, -1);
  glVertex3f(1, 2, -1);
  glNormal3f(-1, 0, 0);
  glVertex3f(-1, 0, -1);
  glVertex3f(-1, 0, 1);
  glVertex3f(-1, 2, 1);
  glVertex3f(-1, 2, -1);
  glNormal3f(1, 0, 0);
  glVertex3f(1, 0, 1);
  glVertex3f(1, 0, -1);
  glVertex3f(1, 2, -1);
  glVertex3f(1, 2, 1);
  glNormal3f(0, 1, 0);
  glVertex3f(-1, 2, 1);
  glVertex3f(1, 2, 1);
  glVertex3f(1, 2, -1);
  glVertex3f(-1, 2, -1);
  glEnd();
}
int main() {
  SDL_SetMainReady();
  assert(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Shadowtest", 0, 0, 320, 240,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  auto c = SDL_GL_CreateContext(w);
  assert(c);
  std::cout << glGetString(GL_VERSION) << "\n";
  glViewport(0, 0, 320, 240);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-.1, .1, -.075, .075, .1, 100);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0, 0, -12);
  glRotatef(35, 1, 0, 0);
  glRotatef(-25, 0, 1, 0);
  glTranslatef(0, -1, 0);
  float camera[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, camera);
  GLint oldDraw = 0, oldRead = 0;
  glGetIntegerv(GL_DRAW_BUFFER, &oldDraw);
  glGetIntegerv(GL_READ_BUFFER, &oldRead);
  float target[3] = {0, 0, 0};
  bool ready = g_SceneShadows.BeginDepth(target, 12);
  if (!ready) {
    std::cerr << g_SceneShadows.Error << "\n";
    return 2;
  }
  geometry();
  g_SceneShadows.EndDepth();
  assert(!glGetError());
  GLint currentDraw = 0, currentRead = 0, currentProgram = 0,
        currentTexture = 0;
  glGetIntegerv(GL_DRAW_BUFFER, &currentDraw);
  glGetIntegerv(GL_READ_BUFFER, &currentRead);
  assert(currentDraw == oldDraw && currentRead == oldRead);
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  assert(viewport[2] == 320 && viewport[3] == 240);
  float restored[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, restored);
  assert(!memcmp(restored, camera, sizeof(camera)));
  auto draw = [&]() {
    glClearColor(.1, .1, .1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    assert(g_SceneShadows.BeginLighting(camera));
    g_SceneShadows.Material(false, .1, .5);
    geometry();
    g_SceneShadows.EndLighting();
    glFinish();
    std::vector<unsigned char> p(320 * 240 * 3);
    glReadPixels(0, 0, 320, 240, GL_RGB, GL_UNSIGNED_BYTE, p.data());
    return p;
  };
  auto shadow = draw();
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &currentTexture);
  assert(currentProgram == 0 && currentTexture == GL_TEXTURE0);
  // Replace the depth map with an empty light-space map; preserve the same
  // lighting shader.
  assert(g_SceneShadows.BeginDepth(target, 12));
  g_SceneShadows.EndDepth();
  auto unshadowed = draw();
  int darker = 0;
  for (size_t i = 0; i < shadow.size(); i += 3)
    if (int(unshadowed[i]) + int(unshadowed[i + 1]) + int(unshadowed[i + 2]) >
        int(shadow[i]) + int(shadow[i + 1]) + int(shadow[i + 2]) + 20)
      ++darker;
  std::cout << "Shadow pixels " << darker << "\n";
  assert(darker > 100);
  assert(!glGetError());
  // A sloped receiver must not become speckled by its own PCF samples.
  for (float tilt : {10.f, 25.f, 45.f}) {
    auto plane = [&]() {
      glPushMatrix();
      glRotatef(tilt, 0, 0, 1);
      glNormal3f(0, 1, 0);
      glColor3f(.7f, .7f, .7f);
      glBegin(GL_QUADS);
      glVertex3f(-5, 0, -5);
      glVertex3f(-5, 0, 5);
      glVertex3f(5, 0, 5);
      glVertex3f(5, 0, -5);
      glEnd();
      glPopMatrix();
    };
    assert(g_SceneShadows.BeginDepth(target, 12));
    plane();
    g_SceneShadows.EndDepth();
    auto receiver = [&]() {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      assert(g_SceneShadows.BeginLighting(camera));
      g_SceneShadows.Material(false, 0, .7f);
      plane();
      g_SceneShadows.EndLighting();
      glFinish();
      std::vector<unsigned char> image(320 * 240 * 3);
      glReadPixels(0, 0, 320, 240, GL_RGB, GL_UNSIGNED_BYTE, image.data());
      return image;
    };
    auto withMap = receiver();
    assert(g_SceneShadows.BeginDepth(target, 12));
    g_SceneShadows.EndDepth();
    auto emptyMap = receiver();
    size_t acne = 0;
    for (size_t i = 0; i < withMap.size(); i += 3)
      if (int(emptyMap[i]) - int(withMap[i]) > 10)
        ++acne;
    assert(acne < 10);
  }

  const GLuint oldTexture = g_SceneShadows.Depth,
               oldFramebuffer = g_SceneShadows.Framebuffer,
               oldProgram = g_SceneShadows.Program;
  assert(oldTexture && oldFramebuffer && oldProgram);
  g_SceneShadows.Disable();
  assert(!g_SceneShadows.Depth && !g_SceneShadows.Framebuffer &&
         !g_SceneShadows.Program);
  assert(!glIsTexture(oldTexture));
  assert(!glGetError());
  std::cout << "PASS: real directional PCF shadows, bounded light map, GL "
               "state restoration and Office cleanup\n";
  SDL_GL_DeleteContext(c);
  SDL_DestroyWindow(w);
  SDL_Quit();
}
