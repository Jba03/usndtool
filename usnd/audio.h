#ifndef audio_h
#define audio_h

#ifdef __cplusplus
extern "C" {
#endif

#include "resource.h"

#define USND_ROLLOFF_MAX_DISTANCE 127.0f

f32 usnd_rolloff_volume(const struct CRollOffParam*, f32 distance);

#ifdef __cplusplus
}
#endif

#endif /* audio_h */
