#include "../animation_core.h"
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
  puts("PASS: C11 animation interpolation, shortest rotation, easing, loop and endpoint");
}
