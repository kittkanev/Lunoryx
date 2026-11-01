#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

// Optional OpenGL 2.1 directional shadow pass. One 1024-square depth map,
// manual 3x3 PCF and a compatibility lighting shader. The map follows a rigid
// editor camera; geometry beyond the bounded map remains unshadowed.
struct SceneShadows {
  using GenFn = void(APIENTRY *)(GLsizei, GLuint *);
  using DeleteFn = void(APIENTRY *)(GLsizei, const GLuint *);
  using BindFn = void(APIENTRY *)(GLenum, GLuint);
  using AttachFn = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
  using CheckFn = GLenum(APIENTRY *)(GLenum);
  using CreateShaderFn = GLuint(APIENTRY *)(GLenum);
  using ShaderSourceFn = void(APIENTRY *)(GLuint, GLsizei,
                                          const GLchar *const *, const GLint *);
  using ObjectFn = void(APIENTRY *)(GLuint);
  using GetObjectFn = void(APIENTRY *)(GLuint, GLenum, GLint *);
  using LogFn = void(APIENTRY *)(GLuint, GLsizei, GLsizei *, GLchar *);
  using CreateProgramFn = GLuint(APIENTRY *)();
  using AttachShaderFn = void(APIENTRY *)(GLuint, GLuint);
  using LocationFn = GLint(APIENTRY *)(GLuint, const GLchar *);
  using IntFn = void(APIENTRY *)(GLint, GLint);
  using FloatFn = void(APIENTRY *)(GLint, GLfloat);
  using VecFn = void(APIENTRY *)(GLint, GLfloat, GLfloat, GLfloat);
  using ActiveFn = void(APIENTRY *)(GLenum);
  GenFn GenFramebuffer = nullptr;
  DeleteFn DeleteFramebuffer = nullptr;
  BindFn BindFramebuffer = nullptr;
  AttachFn AttachTexture = nullptr;
  CheckFn CheckFramebuffer = nullptr;
  CreateShaderFn CreateShader = nullptr;
  ShaderSourceFn ShaderSource = nullptr;
  ObjectFn CompileShader = nullptr, DeleteShader = nullptr,
           LinkProgram = nullptr, DeleteProgram = nullptr, UseProgram = nullptr;
  GetObjectFn GetShader = nullptr, GetProgram = nullptr;
  LogFn ShaderLog = nullptr, ProgramLog = nullptr;
  CreateProgramFn CreateProgram = nullptr;
  AttachShaderFn AttachShader = nullptr;
  LocationFn Location = nullptr;
  IntFn UniformInt = nullptr;
  FloatFn UniformFloat = nullptr;
  VecFn UniformVec = nullptr;
  ActiveFn ActiveTexture = nullptr;
  GLuint Framebuffer = 0, Depth = 0, Program = 0;
  GLint TextureLocation = -1, MetallicLocation = -1, RoughnessLocation = -1,
        SunLocation = -1, UpLocation = -1;
  bool Attempted = false, DepthActive = false, LightingActive = false,
       MapReady = false;
  std::string Error;
  static const int Resolution = 1024;
  float LightProjection[16] = {}, LightView[16] = {}, WorldToShadow[16] = {};
  float Sun[3] = {-.2981424f, .5962848f, .7453560f};
  GLint SavedFramebuffer = 0, SavedProgram = 0,
        SavedActiveTexture = GL_TEXTURE0, SavedMatrixMode = GL_MODELVIEW;
  GLint LightingProgram = 0, LightingTexture = GL_TEXTURE0,
        LightingMatrixMode = GL_MODELVIEW;

  template <class T>
  static T Load(const char *core, const char *extension = nullptr) {
    void *p = SDL_GL_GetProcAddress(core);
    if (!p && extension)
      p = SDL_GL_GetProcAddress(extension);
    return reinterpret_cast<T>(p);
  }
  static void Identity(float m[16]) {
    std::fill(m, m + 16, 0.f);
    m[0] = m[5] = m[10] = m[15] = 1;
  }
  static void Multiply(const float a[16], const float b[16], float out[16]) {
    float result[16] = {};
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 4; ++k)
          result[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    std::copy(result, result + 16, out);
  }
  GLuint Shader(GLenum type, const char *source) {
    GLuint shader = CreateShader(type);
    if (!shader)
      return 0;
    ShaderSource(shader, 1, &source, nullptr);
    CompileShader(shader);
    GLint ok = 0;
    GetShader(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char message[1024] = {};
      ShaderLog(shader, sizeof(message) - 1, nullptr, message);
      Error = std::string("Shadows shader unavailable: ") + message;
      DeleteShader(shader);
      return 0;
    }
    return shader;
  }
  bool Initialize() {
    if (Attempted)
      return Program && Depth && Framebuffer;
    Attempted = true;
    GenFramebuffer = Load<GenFn>("glGenFramebuffers", "glGenFramebuffersEXT");
    DeleteFramebuffer =
        Load<DeleteFn>("glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    BindFramebuffer = Load<BindFn>("glBindFramebuffer", "glBindFramebufferEXT");
    AttachTexture =
        Load<AttachFn>("glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    CheckFramebuffer = Load<CheckFn>("glCheckFramebufferStatus",
                                     "glCheckFramebufferStatusEXT");
    CreateShader = Load<CreateShaderFn>("glCreateShader");
    ShaderSource = Load<ShaderSourceFn>("glShaderSource");
    CompileShader = Load<ObjectFn>("glCompileShader");
    DeleteShader = Load<ObjectFn>("glDeleteShader");
    LinkProgram = Load<ObjectFn>("glLinkProgram");
    DeleteProgram = Load<ObjectFn>("glDeleteProgram");
    UseProgram = Load<ObjectFn>("glUseProgram");
    GetShader = Load<GetObjectFn>("glGetShaderiv");
    GetProgram = Load<GetObjectFn>("glGetProgramiv");
    ShaderLog = Load<LogFn>("glGetShaderInfoLog");
    ProgramLog = Load<LogFn>("glGetProgramInfoLog");
    CreateProgram = Load<CreateProgramFn>("glCreateProgram");
    AttachShader = Load<AttachShaderFn>("glAttachShader");
    Location = Load<LocationFn>("glGetUniformLocation");
    UniformInt = Load<IntFn>("glUniform1i");
    UniformFloat = Load<FloatFn>("glUniform1f");
    UniformVec = Load<VecFn>("glUniform3f");
    ActiveTexture = Load<ActiveFn>("glActiveTexture");
    if (!GenFramebuffer || !DeleteFramebuffer || !BindFramebuffer ||
        !AttachTexture || !CheckFramebuffer || !CreateShader || !ShaderSource ||
        !CompileShader || !DeleteShader || !LinkProgram || !DeleteProgram ||
        !UseProgram || !GetShader || !GetProgram || !ShaderLog || !ProgramLog ||
        !CreateProgram || !AttachShader || !Location || !UniformInt ||
        !UniformFloat || !UniformVec || !ActiveTexture) {
      Error = "Shadows unavailable: required OpenGL entry points are missing";
      return false;
    }
    GLint units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units);
    if (units < 2) {
      Error = "Shadows require two texture units";
      return false;
    }
    const char *vertex = R"GLSL(#version 120
varying vec3 eyePosition;
varying vec3 normal;
varying vec4 tint;
varying vec2 uv;
varying vec4 shadowPosition;
void main() {
    vec4 eye = gl_ModelViewMatrix * gl_Vertex;
    eyePosition = eye.xyz;
    normal = gl_NormalMatrix * gl_Normal;
    tint = gl_Color;
    uv = gl_MultiTexCoord0.xy;
    shadowPosition = gl_TextureMatrix[1] * eye;
    gl_Position = gl_ProjectionMatrix * eye;
}
)GLSL";
    const char *fragment = R"GLSL(#version 120
uniform sampler2D baseImage;
uniform sampler2D shadowImage;
uniform int textured;
uniform float metallic;
uniform float roughness;
uniform vec3 sunEye;
uniform vec3 upEye;
varying vec3 eyePosition;
varying vec3 normal;
varying vec4 tint;
varying vec2 uv;
varying vec4 shadowPosition;
float visibility(float cosine) {
    vec3 p = shadowPosition.xyz / shadowPosition.w;
    if (p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0 || p.z <= 0.0 || p.z >= 1.0) return 1.0;
    // Compare each PCF tap against the receiver depth at that texel, rather
    // than the center depth. This prevents sloped surfaces shadowing themselves.
    vec3 dx = dFdx(p), dy = dFdy(p);
    float determinant = dx.x * dy.y - dx.y * dy.x;
    vec2 gradient = vec2(0.0);
    if (abs(determinant) > 0.0000000001)
        gradient = vec2(dx.z * dy.y - dy.z * dx.y, dx.x * dy.z - dy.x * dx.z) / determinant;
    gradient = clamp(gradient, vec2(-4.0), vec2(4.0));
    float bias = max(0.0002, 0.0006 * (1.0 - cosine));
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        vec2 uv = (floor(p.xy * 1024.0) + vec2(float(x), float(y)) + vec2(0.5)) / 1024.0;
        float d = texture2D(shadowImage, uv).r;
        float receiver = p.z + dot(gradient, uv - p.xy);
        lit += receiver - bias <= d ? 1.0 : 0.0;
    }
    return lit / 9.0;
}
void main() {
    vec3 base = tint.rgb;
    if (textured != 0) base *= texture2D(baseImage, uv).rgb;
    vec3 n = normalize(normal);
    if (!gl_FrontFacing) n = -n;
    float diffuse = max(dot(n, sunEye), 0.0);
    float shadow = visibility(diffuse);
    float ambient = 0.24 + 0.08 * dot(n, upEye);
    vec3 light = base * (ambient + 0.78 * diffuse * shadow);
    vec3 halfDirection = normalize(sunEye + normalize(-eyePosition));
    float highlight = pow(max(dot(n, halfDirection), 0.0), mix(100.0, 3.0, roughness));
    vec3 specular = mix(vec3(0.04), base * 0.7, metallic);
    light += specular * highlight * diffuse * shadow;
    gl_FragColor = vec4(light, 1.0);
}
)GLSL";
    GLuint vs = Shader(GL_VERTEX_SHADER, vertex),
           fs = Shader(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {
      if (vs)
        DeleteShader(vs);
      if (fs)
        DeleteShader(fs);
      return false;
    }
    Program = CreateProgram();
    if (!Program) {
      DeleteShader(vs);
      DeleteShader(fs);
      Error = "Shadows program allocation failed";
      return false;
    }
    AttachShader(Program, vs);
    AttachShader(Program, fs);
    LinkProgram(Program);
    DeleteShader(vs);
    DeleteShader(fs);
    GLint linked = 0;
    GetProgram(Program, GL_LINK_STATUS, &linked);
    if (!linked) {
      char message[1024] = {};
      ProgramLog(Program, sizeof(message) - 1, nullptr, message);
      Error = std::string("Shadows program unavailable: ") + message;
      DeleteProgram(Program);
      Program = 0;
      return false;
    }
    TextureLocation = Location(Program, "textured");
    MetallicLocation = Location(Program, "metallic");
    RoughnessLocation = Location(Program, "roughness");
    SunLocation = Location(Program, "sunEye");
    UpLocation = Location(Program, "upEye");
    GLint framebuffer = 0, active = 0, program = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    ActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &Depth);
    glBindTexture(GL_TEXTURE_2D, Depth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Resolution, Resolution,
                 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    GenFramebuffer(1, &Framebuffer);
    BindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
    AttachTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, Depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    bool valid = CheckFramebuffer(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    BindFramebuffer(GL_FRAMEBUFFER, GLuint(framebuffer));
    glPopAttrib();
    ActiveTexture(GLenum(active));
    UseProgram(GLuint(program));
    if (!valid) {
      Error =
          "Shadows depth framebuffer unavailable; using unshadowed lighting";
      DeleteFramebuffer(1, &Framebuffer);
      Framebuffer = 0;
      glDeleteTextures(1, &Depth);
      Depth = 0;
      DeleteProgram(Program);
      Program = 0;
      return false;
    }
    Error.clear();
    return true;
  }
  bool BeginDepth(const float target[3], float radius = 25.f) {
    if (DepthActive || LightingActive || !Initialize())
      return false;
    if (!std::isfinite(radius) || radius < 1 || radius > 1000)
      return false;
    for (int k = 0; k < 3; ++k)
      if (!std::isfinite(target[k]))
        return false;
    const float length =
        std::sqrt(Sun[0] * Sun[0] + Sun[1] * Sun[1] + Sun[2] * Sun[2]);
    float sun[3] = {Sun[0] / length, Sun[1] / length, Sun[2] / length};
    float side[3] = {sun[2], 0, -sun[0]};
    float s = std::sqrt(side[0] * side[0] + side[2] * side[2]);
    side[0] /= s;
    side[2] /= s;
    float up[3] = {sun[1] * side[2], sun[2] * side[0] - sun[0] * side[2],
                   -sun[1] * side[0]};
    float center[3] = {target[0], target[1], target[2]};
    float sx = 0, sy = 0;
    for (int k = 0; k < 3; ++k) {
      sx += center[k] * side[k];
      sy += center[k] * up[k];
    }
    const float step = 2 * radius / Resolution;
    float dx = std::floor(sx / step + .5f) * step - sx,
          dy = std::floor(sy / step + .5f) * step - sy;
    for (int k = 0; k < 3; ++k)
      center[k] += side[k] * dx + up[k] * dy;
    float eye[3];
    for (int k = 0; k < 3; ++k)
      eye[k] = center[k] + sun[k] * radius * 2;
    Identity(LightView);
    Identity(LightProjection);
    for (int k = 0; k < 3; ++k) {
      LightView[k * 4] = side[k];
      LightView[k * 4 + 1] = up[k];
      LightView[k * 4 + 2] = sun[k];
    }
    for (int k = 0; k < 3; ++k) {
      LightView[12] -= side[k] * eye[k];
      LightView[13] -= up[k] * eye[k];
      LightView[14] -= sun[k] * eye[k];
    }
    LightProjection[0] = LightProjection[5] = 1 / radius;
    const float nearPlane = .1f, farPlane = radius * 4;
    LightProjection[10] = -2 / (farPlane - nearPlane);
    LightProjection[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    float bias[16];
    Identity(bias);
    bias[0] = bias[5] = bias[10] = .5f;
    bias[12] = bias[13] = bias[14] = .5f;
    float projectionView[16];
    Multiply(LightProjection, LightView, projectionView);
    Multiply(bias, projectionView, WorldToShadow);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &SavedFramebuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &SavedProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &SavedActiveTexture);
    glGetIntegerv(GL_MATRIX_MODE, &SavedMatrixMode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    UseProgram(0);
    ActiveTexture(GL_TEXTURE0);
    BindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
    glViewport(0, 0, Resolution, Resolution);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 3.f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(LightProjection);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(LightView);
    MapReady = false;
    DepthActive = true;
    return true;
  }
  void EndDepth() {
    if (!DepthActive)
      return;
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    BindFramebuffer(GL_FRAMEBUFFER, GLuint(SavedFramebuffer));
    glPopAttrib();
    UseProgram(GLuint(SavedProgram));
    ActiveTexture(GLenum(SavedActiveTexture));
    glMatrixMode(GLenum(SavedMatrixMode));
    DepthActive = false;
    MapReady = true;
  }
  bool BeginLighting(const float view[16]) {
    if (DepthActive || LightingActive || !Program || !Depth || !MapReady)
      return false;
    for (int k = 0; k < 16; ++k)
      if (!std::isfinite(view[k]))
        return false;
    float inverse[16];
    Identity(inverse);
    for (int c = 0; c < 3; ++c)
      for (int r = 0; r < 3; ++r)
        inverse[c * 4 + r] = view[r * 4 + c];
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        inverse[12 + r] -= inverse[c * 4 + r] * view[12 + c];
    float matrix[16];
    Multiply(WorldToShadow, inverse, matrix);
    glGetIntegerv(GL_CURRENT_PROGRAM, &LightingProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &LightingTexture);
    glGetIntegerv(GL_MATRIX_MODE, &LightingMatrixMode);
    glPushAttrib(GL_TEXTURE_BIT);
    ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, Depth);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadMatrixf(matrix);
    ActiveTexture(GL_TEXTURE0);
    glMatrixMode(GLenum(LightingMatrixMode));
    UseProgram(Program);
    UniformInt(Location(Program, "baseImage"), 0);
    UniformInt(Location(Program, "shadowImage"), 1);
    float light[3] = {}, up[3] = {view[4], view[5], view[6]};
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        light[r] += view[c * 4 + r] * Sun[c];
    const float length = std::sqrt(light[0] * light[0] + light[1] * light[1] +
                                   light[2] * light[2]);
    UniformVec(SunLocation, light[0] / length, light[1] / length,
               light[2] / length);
    UniformVec(UpLocation, up[0], up[1], up[2]);
    LightingActive = true;
    Material(false, .1f, .5f);
    return true;
  }
  void Material(bool texture, float metallic, float roughness) {
    if (!LightingActive)
      return;
    UniformInt(TextureLocation, texture ? 1 : 0);
    UniformFloat(MetallicLocation, std::max(0.f, std::min(metallic, 1.f)));
    UniformFloat(RoughnessLocation, std::max(0.f, std::min(roughness, 1.f)));
  }
  void EndLighting() {
    if (!LightingActive)
      return;
    ActiveTexture(GL_TEXTURE1);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glPopAttrib();
    UseProgram(GLuint(LightingProgram));
    ActiveTexture(GLenum(LightingTexture));
    glMatrixMode(GLenum(LightingMatrixMode));
    LightingActive = false;
  }
  void Destroy() {
    EndLighting();
    EndDepth();
    if (Framebuffer && DeleteFramebuffer)
      DeleteFramebuffer(1, &Framebuffer);
    if (Depth)
      glDeleteTextures(1, &Depth);
    if (Program && DeleteProgram)
      DeleteProgram(Program);
    Framebuffer = Depth = Program = 0;
    Attempted = MapReady = false;
  }
  void Disable() {
    if (Attempted || Program || Framebuffer || Depth)
      Destroy();
  }
};
static SceneShadows g_SceneShadows;
