#pragma once
#include "viewport_postprocess.h"
#include <cmath>
// OpenGL 2.1 + EXT_framebuffer_object, with core-entry-point fallbacks.
struct ViewportTarget {
  // Color is always the stable display texture referenced by ImGui draw lists.
  GLuint Framebuffer = 0, Color = 0, Depth = 0;
  GLuint SceneColor = 0, PresentFramebuffer = 0;
  int Width = 0, Height = 0, PostQuality = 0;
  float PostExposure = 1;
  bool AttachedPost = false, ForceOffscreen = false;
  ViewportPostProcess Post;
  using GenFn = void(APIENTRY *)(GLsizei, GLuint *);
  using DeleteFn = void(APIENTRY *)(GLsizei, const GLuint *);
  using BindFn = void(APIENTRY *)(GLenum, GLuint);
  using TextureFn = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
  using StorageFn = void(APIENTRY *)(GLenum, GLenum, GLsizei, GLsizei);
  using AttachFn = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint);
  using CheckFn = GLenum(APIENTRY *)(GLenum);
  GenFn GenFramebuffer = nullptr, GenRenderbuffer = nullptr;
  DeleteFn DeleteFramebuffer = nullptr, DeleteRenderbuffer = nullptr;
  BindFn BindFramebuffer = nullptr, BindRenderbuffer = nullptr;
  TextureFn AttachTexture = nullptr;
  StorageFn Storage = nullptr;
  AttachFn AttachRenderbuffer = nullptr;
  CheckFn Check = nullptr;
  template <class Function>
  static Function Load(const char *core, const char *extension) {
    void *pointer = SDL_GL_GetProcAddress(core);
    if (!pointer)
      pointer = SDL_GL_GetProcAddress(extension);
    return reinterpret_cast<Function>(pointer);
  }
  bool Initialize() {
    GenFramebuffer = Load<GenFn>("glGenFramebuffers", "glGenFramebuffersEXT");
    GenRenderbuffer =
        Load<GenFn>("glGenRenderbuffers", "glGenRenderbuffersEXT");
    DeleteFramebuffer =
        Load<DeleteFn>("glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    DeleteRenderbuffer =
        Load<DeleteFn>("glDeleteRenderbuffers", "glDeleteRenderbuffersEXT");
    BindFramebuffer = Load<BindFn>("glBindFramebuffer", "glBindFramebufferEXT");
    BindRenderbuffer =
        Load<BindFn>("glBindRenderbuffer", "glBindRenderbufferEXT");
    AttachTexture =
        Load<TextureFn>("glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    Storage =
        Load<StorageFn>("glRenderbufferStorage", "glRenderbufferStorageEXT");
    AttachRenderbuffer = Load<AttachFn>("glFramebufferRenderbuffer",
                                        "glFramebufferRenderbufferEXT");
    Check = Load<CheckFn>("glCheckFramebufferStatus",
                          "glCheckFramebufferStatusEXT");
    if (!GenFramebuffer || !GenRenderbuffer || !DeleteFramebuffer ||
        !DeleteRenderbuffer || !BindFramebuffer || !BindRenderbuffer ||
        !AttachTexture || !Storage || !AttachRenderbuffer || !Check)
      return false;
    GenFramebuffer(1, &Framebuffer);
    GenRenderbuffer(1, &Depth);
    glGenTextures(1, &Color);
    return Resize(1, 1);
  }
  void ConfigurePostProcess(int quality, float exposure = 1,
                            bool forceOffscreen = false) {
    ForceOffscreen = forceOffscreen;
    PostQuality = std::max(0, std::min(quality, 2));
    PostExposure =
        std::isfinite(exposure) ? std::max(.25f, std::min(exposure, 4.f)) : 1.f;
    if (PostQuality > 0)
      Post.Initialize();
    else if (Post.Program)
      Post.Destroy();
  }
  bool PostProcessEnabled() const {
    return PostQuality > 0 && Post.Program != 0;
  }
  bool PostProcessAvailable() const { return Post.Program != 0; }
  const std::string &PostProcessError() const { return Post.Error; }
  GLuint SceneFramebuffer(bool player) const {
    return player && !PostProcessEnabled() && !ForceOffscreen ? 0 : Framebuffer;
  }
  void AllocateTexture(GLuint texture, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
  }
  bool Resize(int width, int height) {
    bool post = PostProcessEnabled();
    if (width == Width && height == Height && post == AttachedPost)
      return true;
    if (width < 1 || height < 1 || width > 8192 || height > 8192 ||
        !Framebuffer)
      return false;
    GLint oldTexture = 0, oldFramebuffer = 0, oldRenderbuffer = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRenderbuffer);
    AllocateTexture(Color, width, height);
    BindRenderbuffer(GL_RENDERBUFFER, Depth);
    Storage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    if (post) {
      if (!SceneColor)
        glGenTextures(1, &SceneColor);
      if (!PresentFramebuffer)
        GenFramebuffer(1, &PresentFramebuffer);
      AllocateTexture(SceneColor, width, height);
      BindFramebuffer(GL_FRAMEBUFFER, PresentFramebuffer);
      AttachTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Color,
                    0);
      if (Check(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Post.Error =
            "Post processing framebuffer unavailable; using direct rendering";
        if (Post.Program && Post.DeleteProgram)
          Post.DeleteProgram(Post.Program);
        Post.Program = 0;
        post = false;
      }
    }
    BindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
    AttachTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                  post ? SceneColor : Color, 0);
    AttachRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                       Depth);
    bool valid = Check(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!valid && post) {
      Post.Error = "Post processing target unavailable; using direct rendering";
      if (Post.Program && Post.DeleteProgram)
        Post.DeleteProgram(Post.Program);
      Post.Program = 0;
      post = false;
      AttachTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Color,
                    0);
      valid = Check(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    if (!post) {
      if (PresentFramebuffer) {
        if (GLuint(oldFramebuffer) == PresentFramebuffer)
          oldFramebuffer = 0;
        DeleteFramebuffer(1, &PresentFramebuffer);
        PresentFramebuffer = 0;
      }
      if (SceneColor) {
        if (GLuint(oldTexture) == SceneColor)
          oldTexture = 0;
        glDeleteTextures(1, &SceneColor);
        SceneColor = 0;
      }
    }
    BindFramebuffer(GL_FRAMEBUFFER, GLuint(oldFramebuffer));
    BindRenderbuffer(GL_RENDERBUFFER, GLuint(oldRenderbuffer));
    glBindTexture(GL_TEXTURE_2D, GLuint(oldTexture));
    if (valid) {
      Width = width;
      Height = height;
      AttachedPost = post;
    }
    return valid;
  }
  void FinishScene(bool player, int outputWidth = 0, int outputHeight = 0) {
    const bool post = PostProcessEnabled() && AttachedPost;
    if (!post && !(player && ForceOffscreen))
      return;
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    BindFramebuffer(GL_FRAMEBUFFER, player ? 0 : PresentFramebuffer);
    if (post)
      Post.Draw(SceneColor, Width, Height, PostQuality, PostExposure,
                player ? outputWidth : 0, player ? outputHeight : 0);
    else
      Post.Copy(Color, outputWidth > 0 ? outputWidth : Width,
                outputHeight > 0 ? outputHeight : Height);
    BindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
  }
  void Destroy() {
    Post.Destroy();
    if (PresentFramebuffer && DeleteFramebuffer)
      DeleteFramebuffer(1, &PresentFramebuffer);
    if (SceneColor)
      glDeleteTextures(1, &SceneColor);
    if (Framebuffer && DeleteFramebuffer)
      DeleteFramebuffer(1, &Framebuffer);
    if (Depth && DeleteRenderbuffer)
      DeleteRenderbuffer(1, &Depth);
    if (Color)
      glDeleteTextures(1, &Color);
    Framebuffer = Color = Depth = SceneColor = PresentFramebuffer = 0;
    Width = Height = 0;
    AttachedPost = ForceOffscreen = false;
  }
};
static ViewportTarget g_ViewportTarget;
