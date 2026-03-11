#ifndef audio_h
#define audio_h

#ifdef __cplusplus
extern "C" {
#endif

#include "resource.h"

#define USND_ROLLOFF_MAX_DISTANCE 127.0f

typedef s16 usnd_sample;

typedef struct usnd_audio_stream usnd_audio_stream;
struct usnd_audio_stream {
  usnd_audio_format format;
  u8 num_channels;
  u32 sample_rate;
  u32 size;
  usnd_sample *data;
};

/* Convert between audio formats (no sample rate conversion). Returns
 * the requested output stream size if the output stream is NULL. */
usnd_size usnd_audio_convert(const usnd_audio_stream*, usnd_audio_format, usnd_audio_stream*);
/* Get the attenuated volume for a sound source at a distance */
f32 usnd_rolloff_volume(const struct CRollOffParam*, f32 distance);

#ifdef __cplusplus
}
#endif

#endif /* audio_h */
