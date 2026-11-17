#pragma once
/* Allocation-free C17 keyframe sampler. Times are seconds; rotations degrees. */
#include <math.h>
#include <stddef.h>
typedef struct { float Position[3], Rotation[3], Scale[3], Color[3]; } LnxPose;
typedef struct { float Time; LnxPose Pose; int Easing; } LnxAnimationKey;
typedef enum {
  LNX_EASE_LINEAR=0, LNX_EASE_SMOOTH=1, LNX_EASE_STEP=2,
  LNX_EASE_QUAD_IN=3, LNX_EASE_QUAD_OUT=4, LNX_EASE_QUAD_IN_OUT=5,
  LNX_EASE_CUBIC_IN=6, LNX_EASE_CUBIC_OUT=7, LNX_EASE_CUBIC_IN_OUT=8,
  LNX_EASE_SINE_IN_OUT=9, LNX_EASE_COUNT=10
} LnxEasing;
static inline int lnx_animation_valid_easing(int easing){return easing>=0&&easing<LNX_EASE_COUNT;}
static inline double lnx_animation_ease(int easing,double t){
  if(!isfinite(t)||!lnx_animation_valid_easing(easing))return 0;
  if(t<=0)return 0;
  if(t>=1)return 1;
  switch(easing){
    case LNX_EASE_SMOOTH:return t*t*(3-2*t);
    case LNX_EASE_STEP:return 0;
    case LNX_EASE_QUAD_IN:return t*t;
    case LNX_EASE_QUAD_OUT:return t*(2-t);
    case LNX_EASE_QUAD_IN_OUT:return t<.5?2*t*t:1-2*(1-t)*(1-t);
    case LNX_EASE_CUBIC_IN:return t*t*t;
    case LNX_EASE_CUBIC_OUT:return 1-(1-t)*(1-t)*(1-t);
    case LNX_EASE_CUBIC_IN_OUT:return t<.5?4*t*t*t:1-4*(1-t)*(1-t)*(1-t);
    case LNX_EASE_SINE_IN_OUT:return .5*(1-cos(3.14159265358979323846*t));
    default:return t;
  }
}
static inline double lnx_animation_time(double time, double duration, int loop) {
  if (!isfinite(time) || !isfinite(duration) || duration <= 0) return 0;
  if (loop) { time = fmod(time, duration); return time < 0 ? time + duration : time; }
  return time < 0 ? 0 : (time > duration ? duration : time);
}
static inline int lnx_animation_sample(const LnxAnimationKey *keys, size_t count,
                                       double time, LnxPose *out) {
  if (!keys || !count || !out || !isfinite(time)) return 0;
  if (time <= keys[0].Time) { *out = keys[0].Pose; return 1; }
  if (time >= keys[count-1].Time) { *out = keys[count-1].Pose; return 1; }
  size_t lo = 0, hi = count-1;
  while (hi-lo > 1) { size_t mid = lo+(hi-lo)/2; if (keys[mid].Time <= time) lo=mid; else hi=mid; }
  const LnxAnimationKey *a=keys+lo, *b=keys+hi;
  double span=(double)b->Time-a->Time;
  if(!isfinite(span)||span<=0||!lnx_animation_valid_easing(a->Easing))return 0;
  double t=lnx_animation_ease(a->Easing,(time-a->Time)/span);
  for(int k=0;k<3;++k) {
    out->Position[k]=(float)(a->Pose.Position[k]+(b->Pose.Position[k]-a->Pose.Position[k])*t);
    out->Scale[k]=(float)(a->Pose.Scale[k]+(b->Pose.Scale[k]-a->Pose.Scale[k])*t);
    out->Color[k]=(float)(a->Pose.Color[k]+(b->Pose.Color[k]-a->Pose.Color[k])*t);
    out->Rotation[k]=(float)(a->Pose.Rotation[k]+remainder((double)b->Pose.Rotation[k]-a->Pose.Rotation[k],360.0)*t);
  }
  return 1;
}
