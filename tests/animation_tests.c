#include "src/core/animation_core.h"
#include <assert.h>
#include <stdio.h>
int main(void){
  LnxAnimationKey keys[2]={0};LnxPose pose;
  keys[0].Time=0;keys[1].Time=2;keys[0].Pose.Rotation[1]=350;keys[1].Pose.Rotation[1]=10;
  keys[0].Pose.Scale[0]=1;keys[1].Pose.Scale[0]=3;keys[1].Pose.Position[0]=10;
  assert(!lnx_animation_sample(NULL,0,0,&pose));
  assert(lnx_animation_sample(keys,2,1,&pose)&&pose.Position[0]==5&&pose.Scale[0]==2&&pose.Rotation[1]==360);
  keys[0].Easing=2;lnx_animation_sample(keys,2,1,&pose);assert(pose.Position[0]==0);
  keys[0].Easing=1;lnx_animation_sample(keys,2,.5,&pose);assert(fabs(pose.Position[0]-1.5625)<.00001);
  lnx_animation_sample(keys,2,20,&pose);assert(pose.Position[0]==10);
  assert(lnx_animation_time(2,2,1)==0&&lnx_animation_time(-.5,2,1)==1.5&&lnx_animation_time(8,2,0)==2);
  const double quarter[LNX_EASE_COUNT]={.25,.15625,0,.0625,.4375,.125,.015625,.578125,.0625,.14644660940672624};
  for(int curve=0;curve<LNX_EASE_COUNT;++curve){
    assert(fabs(lnx_animation_ease(curve,.25)-quarter[curve])<1e-12);
    assert(lnx_animation_ease(curve,-1)==0&&lnx_animation_ease(curve,2)==1);
    assert(lnx_animation_ease(curve,0)==0&&lnx_animation_ease(curve,1)==1);
    assert(lnx_animation_ease(curve,NAN)==0&&lnx_animation_ease(curve,INFINITY)==0);
    double previous=0;
    for(int step=0;step<=1000;++step){double v=lnx_animation_ease(curve,step/1000.0);assert(isfinite(v)&&v>=previous&&v>=0&&v<=1);previous=v;}
    keys[0].Easing=curve;assert(lnx_animation_sample(keys,2,.5,&pose));assert(fabs(pose.Position[0]-10*quarter[curve])<1e-5);
  }
  assert(lnx_animation_ease(-1,.5)==0&&lnx_animation_ease(LNX_EASE_COUNT,.5)==0);
  keys[0].Easing=LNX_EASE_COUNT;assert(!lnx_animation_sample(keys,2,1,&pose));
  keys[0].Easing=0;keys[1].Time=keys[0].Time;
  assert(lnx_animation_sample(keys,2,0,&pose)&&pose.Position[0]==0);
  assert(lnx_animation_sample(keys,2,1,&pose)&&pose.Position[0]==10);
  assert(!lnx_animation_sample(keys,2,NAN,&pose));
  puts("PASS: C17 animation interpolation, shortest rotation, easing, loop and endpoint");
  puts("PASS: ten easing curves, golden samples, finite bounds, monotonicity and invalid input");
}
