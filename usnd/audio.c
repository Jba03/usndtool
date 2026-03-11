#include "audio.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

#pragma mark - Audio format

#include "adpcm.c"

usnd_size usnd_audio_convert(const usnd_audio_stream *src,
  usnd_audio_format fmt, usnd_audio_stream *dst)
{
  assert(src && src->data && "invalid source audio stream");
  
  /* Copy */
  if (src->format == USND_AUDIO_FORMAT_PCM && fmt == USND_AUDIO_FORMAT_PCM) {
    if (dst == NULL)
      return src->size;
    dst->size = src->size;
    dst->num_channels = src->num_channels;
    dst->sample_rate = src->sample_rate;
    memcpy(dst->data, src->data, dst->size);
    return 1;
  }
  
  /* Decode X -> PCM */
  if (src->format != USND_AUDIO_FORMAT_PCM && fmt == USND_AUDIO_FORMAT_PCM) {
    if (src->format == USND_AUDIO_FORMAT_DSP) return DSP_Decode(src, dst);
    if (src->format == USND_AUDIO_FORMAT_PSX) return PSX_Decode(src, dst);
    if (src->format == USND_AUDIO_FORMAT_UBI) return UBI_Decode(src, dst);
    return 0;
  }
  
  /* Encode PCM -> X */
  if (src->format == USND_AUDIO_FORMAT_PCM && fmt != USND_AUDIO_FORMAT_PCM) {
//    if (dst->format == USND_AUDIO_FORMAT_DSP) return DSP_Encode(src, dst);
    return 0;
  }
  
  /* TODO: Indirect conversion */
  return 0;
}

