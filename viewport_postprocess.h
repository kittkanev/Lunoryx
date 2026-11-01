#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <string>

// Compatibility GLSL 1.20. No extensions beyond the editor's OpenGL 2.1 base.
// This is an LDR antialiasing/colour pass; it does not claim an HDR pipeline.
struct ViewportPostProcess {
  using CreateShaderFn = GLuint(APIENTRY *)(GLenum);
  using ShaderSourceFn = void(APIENTRY *)(GLuint, GLsizei,
                                          const GLchar *const *, const GLint *);
  using ObjectFn = void(APIENTRY *)(GLuint);
  using GetObjectFn = void(APIENTRY *)(GLuint, GLenum, GLint *);
  using GetLogFn = void(APIENTRY *)(GLuint, GLsizei, GLsizei *, GLchar *);
  using CreateProgramFn = GLuint(APIENTRY *)();
  using AttachShaderFn = void(APIENTRY *)(GLuint, GLuint);
  using UniformLocationFn = GLint(APIENTRY *)(GLuint, const GLchar *);
  using Uniform1iFn = void(APIENTRY *)(GLint, GLint);
  using Uniform1fFn = void(APIENTRY *)(GLint, GLfloat);
  using Uniform2fFn = void(APIENTRY *)(GLint, GLfloat, GLfloat);
  using ActiveTextureFn = void(APIENTRY *)(GLenum);
  CreateShaderFn CreateShader = nullptr;
  ShaderSourceFn ShaderSource = nullptr;
  ObjectFn CompileShader = nullptr, DeleteShader = nullptr,
           LinkProgram = nullptr, DeleteProgram = nullptr, UseProgram = nullptr;
  GetObjectFn GetShaderiv = nullptr, GetProgramiv = nullptr;
  GetLogFn GetShaderInfoLog = nullptr, GetProgramInfoLog = nullptr;
  CreateProgramFn CreateProgram = nullptr;
  AttachShaderFn AttachShader = nullptr;
  UniformLocationFn GetUniformLocation = nullptr;
  Uniform1iFn Uniform1i = nullptr;
  Uniform1fFn Uniform1f = nullptr;
  Uniform2fFn Uniform2f = nullptr;
  ActiveTextureFn ActiveTexture = nullptr;
  GLuint Program = 0;
  GLint Image = -1, Texel = -1, Grade = -1, Exposure = -1;
  bool Attempted = false;
  std::string Error;

  template <class T> static T Load(const char *name) {
    return reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
  }
  bool Initialize() {
    if (Attempted)
      return Program != 0;
    Attempted = true;
    CreateShader = Load<CreateShaderFn>("glCreateShader");
    ShaderSource = Load<ShaderSourceFn>("glShaderSource");
    CompileShader = Load<ObjectFn>("glCompileShader");
    DeleteShader = Load<ObjectFn>("glDeleteShader");
    GetShaderiv = Load<GetObjectFn>("glGetShaderiv");
    GetShaderInfoLog = Load<GetLogFn>("glGetShaderInfoLog");
    CreateProgram = Load<CreateProgramFn>("glCreateProgram");
    AttachShader = Load<AttachShaderFn>("glAttachShader");
    LinkProgram = Load<ObjectFn>("glLinkProgram");
    DeleteProgram = Load<ObjectFn>("glDeleteProgram");
    UseProgram = Load<ObjectFn>("glUseProgram");
    GetProgramiv = Load<GetObjectFn>("glGetProgramiv");
    GetProgramInfoLog = Load<GetLogFn>("glGetProgramInfoLog");
    GetUniformLocation = Load<UniformLocationFn>("glGetUniformLocation");
    Uniform1i = Load<Uniform1iFn>("glUniform1i");
    Uniform1f = Load<Uniform1fFn>("glUniform1f");
    Uniform2f = Load<Uniform2fFn>("glUniform2f");
    ActiveTexture = Load<ActiveTextureFn>("glActiveTexture");
    if (!CreateShader || !ShaderSource || !CompileShader || !DeleteShader ||
        !GetShaderiv || !GetShaderInfoLog || !CreateProgram || !AttachShader ||
        !LinkProgram || !DeleteProgram || !UseProgram || !GetProgramiv ||
        !GetProgramInfoLog || !GetUniformLocation || !Uniform1i || !Uniform1f ||
        !Uniform2f || !ActiveTexture) {
      Error = "Post processing unavailable: shader entry points are missing";
      return false;
    }
    const char *vertex = R"GLSL(#version 120
varying vec2 uv;
void main() { gl_Position=gl_Vertex; uv=gl_MultiTexCoord0.xy; }
)GLSL";
    const char *fragment = R"GLSL(#version 120
uniform sampler2D image;
uniform vec2 texel;
uniform int grade;
uniform float exposure;
varying vec2 uv;
float luminance(vec3 c) { return dot(c,vec3(0.299,0.587,0.114)); }
vec3 antialias() {
    vec3 middle=texture2D(image,uv).rgb;
    vec3 nw=texture2D(image,uv+vec2(-1.0,1.0)*texel).rgb;
    vec3 ne=texture2D(image,uv+vec2(1.0,1.0)*texel).rgb;
    vec3 sw=texture2D(image,uv+vec2(-1.0,-1.0)*texel).rgb;
    vec3 se=texture2D(image,uv+vec2(1.0,-1.0)*texel).rgb;
    float m=luminance(middle),a=luminance(nw),b=luminance(ne),c=luminance(sw),d=luminance(se);
    float lo=min(m,min(min(a,b),min(c,d))),hi=max(m,max(max(a,b),max(c,d)));
    if(hi-lo<max(0.0312,hi*0.125))return middle;
    vec2 direction=vec2(-((a+b)-(c+d)),(a+c)-(b+d));
    float reduce=max((a+b+c+d)*(0.25*0.125),0.0078125);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),vec2(-8.0),vec2(8.0))*texel;
    vec3 first=0.5*(texture2D(image,uv+direction*(1.0/3.0-0.5)).rgb+texture2D(image,uv+direction*(2.0/3.0-0.5)).rgb);
    vec3 second=first*0.5+0.25*(texture2D(image,uv-direction*0.5).rgb+texture2D(image,uv+direction*0.5).rgb);
    float l=luminance(second);
    return l<lo||l>hi?first:second;
}
vec3 filmic(vec3 colour) {
    // Display-referred LDR adjustment. Preserve the white point and avoid
    // pretending that already-clipped source highlights can be recovered.
    vec3 x=pow(max(colour,vec3(0.0)),vec3(2.2))*exposure;
    vec3 mapped=(x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14);
    float white=(2.51+0.03)/(2.43+0.59+0.14);
    return pow(clamp(mapped/white,vec3(0.0),vec3(1.0)),vec3(1.0/2.2));
}
void main() {
    vec3 colour=antialias();
    if(grade!=0)colour=filmic(colour);
    gl_FragColor=vec4(colour,1.0);
}
)GLSL";
    auto compile = [&](GLenum kind, const char *source) -> GLuint {
      GLuint shader = CreateShader(kind);
      if (!shader) {
        Error = "Cannot create post processing shader";
        return 0;
      }
      ShaderSource(shader, 1, &source, nullptr);
      CompileShader(shader);
      GLint ok = 0;
      GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
      if (!ok) {
        char log[1024] = {};
        GetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
        Error = std::string("Post processing shader failed: ") + log;
        DeleteShader(shader);
        return 0;
      }
      return shader;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, vertex);
    if (!vs)
      return false;
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
    if (!fs) {
      DeleteShader(vs);
      return false;
    }
    Program = CreateProgram();
    if (Program) {
      AttachShader(Program, vs);
      AttachShader(Program, fs);
      LinkProgram(Program);
    }
    DeleteShader(vs);
    DeleteShader(fs);
    GLint ok = 0;
    if (Program)
      GetProgramiv(Program, GL_LINK_STATUS, &ok);
    if (!ok) {
      char log[1024] = {};
      if (Program)
        GetProgramInfoLog(Program, sizeof(log) - 1, nullptr, log);
      Error = std::string("Post processing link failed: ") + log;
      if (Program)
        DeleteProgram(Program);
      Program = 0;
      return false;
    }
    Image = GetUniformLocation(Program, "image");
    Texel = GetUniformLocation(Program, "texel");
    Grade = GetUniformLocation(Program, "grade");
    Exposure = GetUniformLocation(Program, "exposure");
    Error.clear();
    return true;
  }
  void Draw(GLuint texture, int width, int height, int quality, float exposure,
            int outputWidth = 0, int outputHeight = 0) {
    if (!Program || !texture || width < 1 || height < 1)
      return;
    GLint oldProgram = 0, oldActive = 0, oldTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    ActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_POLYGON_BIT | GL_VIEWPORT_BIT | GL_CURRENT_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, outputWidth > 0 ? outputWidth : width,
               outputHeight > 0 ? outputHeight : height);
    glBindTexture(GL_TEXTURE_2D, texture);
    UseProgram(Program);
    Uniform1i(Image, 0);
    Uniform2f(Texel, 1.0f / width, 1.0f / height);
    Uniform1i(Grade, quality >= 2 ? 1 : 0);
    Uniform1f(Exposure, std::max(.25f, std::min(exposure, 4.0f)));
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2f(1, 0);
    glVertex2f(1, -1);
    glTexCoord2f(1, 1);
    glVertex2f(1, 1);
    glTexCoord2f(0, 1);
    glVertex2f(-1, 1);
    glEnd();
    UseProgram(GLuint(oldProgram));
    glBindTexture(GL_TEXTURE_2D, GLuint(oldTexture));
    glPopAttrib();
    ActiveTexture(GLenum(oldActive));
  }
  // Cheap scaled presentation for Office mode. It uses the existing scene
  // colour texture and the fixed pipeline, not another FBO or shader program.
  void Copy(GLuint texture, int outputWidth, int outputHeight) {
    if (!texture || outputWidth < 1 || outputHeight < 1)
      return;
    if (!ActiveTexture)
      ActiveTexture = Load<ActiveTextureFn>("glActiveTexture");
    if (!UseProgram)
      UseProgram = Load<ObjectFn>("glUseProgram");
    GLint oldProgram = 0, oldActive = GL_TEXTURE0;
    if (UseProgram)
      glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    if (ActiveTexture)
      glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    if (UseProgram)
      UseProgram(0);
    if (ActiveTexture) {
      GLint units = 1;
      glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
      for (GLint unit = 1; unit < units; ++unit) {
        ActiveTexture(GL_TEXTURE0 + unit);
        glDisable(GL_TEXTURE_2D);
      }
      ActiveTexture(GL_TEXTURE0);
    }
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, outputWidth, outputHeight);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2f(1, 0);
    glVertex2f(1, -1);
    glTexCoord2f(1, 1);
    glVertex2f(1, 1);
    glTexCoord2f(0, 1);
    glVertex2f(-1, 1);
    glEnd();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
    if (UseProgram)
      UseProgram(GLuint(oldProgram));
    if (ActiveTexture)
      ActiveTexture(GLenum(oldActive));
  }
  void Destroy() {
    if (Program && DeleteProgram)
      DeleteProgram(Program);
    Program = 0;
    Attempted = false;
    Error.clear();
  }
};
