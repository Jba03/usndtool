#include "audio.h"

#include <math.h>

f32 usnd_rolloff_volume(const struct CRollOffParam *r, f32 distance) {
  if (distance <= r->saturation_distance)
    return 0.0f;
  
  if (distance >= r->stabilization_distance)
    return r->stabilization_volume;
  
  float db = 20.0f * log10f(distance / USND_ROLLOFF_MAX_DISTANCE);
  
  if (db < -100.0f)
    db = -100.0f;
  
  if (db < r->stabilization_volume)
    db = r->stabilization_volume;
  
  return db;
}
