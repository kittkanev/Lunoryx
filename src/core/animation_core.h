#pragma once
/* Allocation-free C11 keyframe sampler. Times are seconds; rotations degrees. */
#include <math.h>
#include <stddef.h>
typedef struct { float Position[3], Rotation[3], Scale[3], Color[3]; } LnxPose;
typedef struct { float Time; LnxPose Pose; int Easing; } LnxAnimationKey;
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
  double t=(time-a->Time)/(b->Time-a->Time);
  if(a->Easing==1) t=t*t*(3-2*t);
  if(a->Easing==2) t=0;
  for(int k=0;k<3;++k) {
    out->Position[k]=(float)(a->Pose.Position[k]+(b->Pose.Position[k]-a->Pose.Position[k])*t);
    out->Scale[k]=(float)(a->Pose.Scale[k]+(b->Pose.Scale[k]-a->Pose.Scale[k])*t);
    out->Color[k]=(float)(a->Pose.Color[k]+(b->Pose.Color[k]-a->Pose.Color[k])*t);
    out->Rotation[k]=(float)(a->Pose.Rotation[k]+remainder((double)b->Pose.Rotation[k]-a->Pose.Rotation[k],360.0)*t);
  }
  return 1;
}
