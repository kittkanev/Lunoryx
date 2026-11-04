#pragma once
#include "editor_transform.h"
struct StudioProjected { float X=0,Y=0,Depth=0; bool Valid=false; };
inline StudioProjected StudioProjectPoint(const StudioMatrix& matrix,Vec3 p,float x,float y,float width,float height){
  float c[4]={};for(int r=0;r<4;++r)c[r]=matrix.M[r]*p.x+matrix.M[4+r]*p.y+matrix.M[8+r]*p.z+matrix.M[12+r];
  StudioProjected out;if(!std::isfinite(c[3])||c[3]<=.001f)return out;
  out.X=x+(c[0]/c[3]+1)*.5f*width;out.Y=y+(1-c[1]/c[3])*.5f*height;out.Depth=c[2]/c[3];
  out.Valid=std::isfinite(out.X)&&std::isfinite(out.Y)&&out.Depth>=-1&&out.Depth<=1;return out;
}
inline bool StudioSelectionContains(float x,float y,float ax,float ay,float bx,float by,bool circle,float radius){
  if(circle)return (x-bx)*(x-bx)+(y-by)*(y-by)<=radius*radius;
  return x>=std::min(ax,bx)&&x<=std::max(ax,bx)&&y>=std::min(ay,by)&&y<=std::max(ay,by);
}
inline float StudioSegmentDistance2(float x,float y,float ax,float ay,float bx,float by){
  float dx=bx-ax,dy=by-ay,n=dx*dx+dy*dy,t=n>1e-9f?std::max(0.f,std::min(1.f,((x-ax)*dx+(y-ay)*dy)/n)):0;
  dx=x-ax-t*dx;dy=y-ay-t*dy;return dx*dx+dy*dy;
}
struct ViewportSelectionState {
  int Tool=0;bool XRay=false,Dragging=false,Paint=false,Replace=false,Remove=false;
  float Radius=25;ImVec2 Start,End;
};
static ViewportSelectionState g_Selection;
