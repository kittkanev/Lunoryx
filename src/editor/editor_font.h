#pragma once
#include "assets/font_data.h"
#include "assets/code_font_data.h"
static float g_FontPixels=0,g_FontDensity=0;
static void UpdateStudioFont(SDL_Window* window) {
  int width=0,height=0,pixels=0,pixelHeight=0;
  SDL_GetWindowSize(window,&width,&height);SDL_GL_GetDrawableSize(window,&pixels,&pixelHeight);
  float density=width>0?float(pixels)/width:1;density=std::max(1.f,std::min(density,3.f));
  float dpi=96;int display=SDL_GetWindowDisplayIndex(window);if(display>=0&&SDL_GetDisplayDPI(display,&dpi,nullptr,nullptr)!=0)dpi=96;
  float uiScale=std::max(1.f,std::min(dpi/96.f/density,2.f));
  float logical=std::round(g_Preferences.FontSize*uiScale);
  if(std::abs(logical-g_FontPixels)<.1f&&std::abs(density-g_FontDensity)<.01f)return;
  auto& io=ImGui::GetIO();io.Fonts->Clear();
  ImFontConfig config;config.FontDataOwnedByAtlas=false;config.OversampleH=3;config.OversampleV=2;config.PixelSnapH=false;
  io.Fonts->AddFontFromMemoryTTF((void*)kStudioFont,sizeof(kStudioFont),logical*density,&config,io.Fonts->GetGlyphRangesCyrillic());
  g_CodeFont=io.Fonts->AddFontFromMemoryTTF((void*)kCodeFont,sizeof(kCodeFont),logical*density,&config,io.Fonts->GetGlyphRangesCyrillic());
  io.FontGlobalScale=1/density;io.Fonts->Build();g_FontPixels=logical;g_FontDensity=density;
}
