#include <limits.h>

#define min(a,b) (((a)<(b)) ? (a):(b))
#define max(a,b) (((a)>(b)) ? (a):(b))
#define clamp(x,mi,ma) max(mi,min((x),(ma)))

#pragma mark - GC DSP ADPCM

#define DSP_BYTES_PER_FRAME 8
#define DSP_NIBBLES_PER_FRAME 16
#define DSP_SAMPLES_PER_FRAME 14
#define DSP_HEADER_SIZE 96
#define DSP_MAX_CHANNELS 2

struct GCADPCM_HEADER {
  u32 num_samples;
  u32 num_nibbles;
  u32 sample_rate;
  u16 loop_flag;
  u16 format;
  u32 loop_start;
  u32 loop_end;
  u32 ca_flag;
  s16 c[16];
  s16 gain;
  s16 ps;
  s16 hst1;
  s16 hst2;
  s16 loop_ps;
  s16 loop_hst1;
  s16 loop_hst2;
  u8 padding[22];
};

void S_GCADPCM_Header(struct usnd_flow *flow, struct GCADPCM_HEADER *hdr) {
  S_u32(flow, (void*)&hdr->num_samples);
  S_u32(flow, (void*)&hdr->num_nibbles);
  S_u32(flow, (void*)&hdr->sample_rate);
  S_u16(flow, (void*)&hdr->loop_flag);
  S_u16(flow, (void*)&hdr->format);
  S_u32(flow, (void*)&hdr->loop_start);
  S_u32(flow, (void*)&hdr->loop_end);
  S_u32(flow, (void*)&hdr->ca_flag);
  for (u32 i = 0; i < 16; i++)
    S_u16(flow, (void*)(hdr->c + i));
  S_u16(flow, (void*)&hdr->gain);
  S_u16(flow, (void*)&hdr->ps);
  S_u16(flow, (void*)&hdr->hst1);
  S_u16(flow, (void*)&hdr->hst2);
  S_u16(flow, (void*)&hdr->loop_ps);
  S_u16(flow, (void*)&hdr->loop_hst1);
  S_u16(flow, (void*)&hdr->loop_hst2);
  usnd_flow_advance(flow, sizeof hdr->padding);
}

struct GCADPCM
{
  
};

static usnd_size DSP_DecodedChannelSize(const struct GCADPCM_HEADER *hdr)
{
  usnd_size frames = hdr->num_samples / DSP_SAMPLES_PER_FRAME;
  if (hdr->num_samples % DSP_SAMPLES_PER_FRAME) frames++;
  return frames * DSP_SAMPLES_PER_FRAME * sizeof(usnd_sample);
}

static void DSP_DecodeChannels(const u8 *dsp,
  const struct GCADPCM_HEADER *chan,
  u32 num_channels, usnd_sample *pcm)
{
  assert(num_channels <= DSP_MAX_CHANNELS
  && "DSP_DecodeChannels(): invalid channel count");
  
  u32 max_frames = 0;
  u32 total_samples = 0;
  u32 hstb1[DSP_MAX_CHANNELS];
  u32 hstb2[DSP_MAX_CHANNELS];
  u32 remaining[DSP_MAX_CHANNELS];
  
  for (u8 c = 0; c < num_channels; c++) {
    hstb1[c] = chan[c].hst1;
    hstb2[c] = chan[c].hst2;
    remaining[c] = chan[c].num_samples;
    total_samples += chan[c].num_samples;
    
    u32 num_frames = (chan[c].num_samples +
      DSP_SAMPLES_PER_FRAME - 1) / DSP_SAMPLES_PER_FRAME;
    if (num_frames > max_frames)
      max_frames = num_frames;
  }
  
  for (u32 i = 0; i < max_frames; i++) {
    for (u32 c = 0; c < num_channels; c++) {
      if (remaining[c] <= 0)
        continue; /* done with this channel */
      
      s8  const ps = *dsp++;
      s32 const predictor = clamp((ps >> 4) & 0xF, 0, 7);
      s32 const scale = 1 << (ps & 0xF);
      s16 const c1 = chan[c].c[predictor * 2 + 0];
      s16 const c2 = chan[c].c[predictor * 2 + 1];

      s32 hst1 = hstb1[c];
      s32 hst2 = hstb2[c];
      s32 count = min(DSP_SAMPLES_PER_FRAME, remaining[c]);

      for (u32 s = 0; s < count; s++) {
        s32 sample = (s & 1) ? (*dsp++ & 0xF) : (*dsp >> 4) & 0xF;
        sample = (sample >= 8) ? (sample - 16) : sample;
        sample = (((scale * sample) << 11) + 1024 + (c1*hst1 + c2*hst2)) >> 11;
        sample = clamp(sample, SHRT_MIN, SHRT_MAX);
        hst2 = hst1;
        
        pcm[s * num_channels + c] = hst1 = sample;
      }

      hstb1[c] = hst1;
      hstb2[c] = hst2;
      remaining[c] -= count;
    }
    
    pcm += DSP_SAMPLES_PER_FRAME * num_channels;
  }
}

static usnd_size DSP_Decode(const usnd_audio_stream *src, usnd_audio_stream *dst_pcm) {
  struct usnd_flow flow = {};
  flow.mode = USND_READ;
  flow.buf = (u8*)src->data;
  flow.endianness = USND_BIG_ENDIAN;
  
  usnd_size output_size = 0;
  /* read the channel headers first */
  struct GCADPCM_HEADER header[DSP_MAX_CHANNELS];
  for (u32 i = 0; i < src->num_channels; i++) {
    S_GCADPCM_Header(&flow, header+i);
    output_size += DSP_DecodedChannelSize(header+i);
  }
  
  if (!dst_pcm)
    return output_size;
  
  memset(dst_pcm->data, 0, output_size);
  /* decode each channel */
  const u8 *audio_start = (u8*)flow.buf + flow.pos;
  DSP_DecodeChannels(audio_start, header, src->num_channels, dst_pcm->data);
  return output_size;
}

#pragma mark - PSX-ADPCM

#define PSX_BYTES_PER_FRAME 16
#define PSX_SAMPLE_BYTES_PER_FRAME 14
#define PSX_SAMPLES_PER_FRAME 28
#define PSX_MAX_CHANNELS 2

static const float PSXADPCMCoefficients[16][2] = {
  { 0.0f        ,  0.0f        },
  { 0.9375f     ,  0.0f        },
  { 1.796875f   , -0.8125f     },
  { 1.53125f    , -0.859375f   },
  { 1.90625f    , -0.9375f     },
  { 0.46875f    , -0.0f        },
  { 0.8984375f  , -0.40625f    },
  { 0.765625f   , -0.4296875f  },
  { 0.953125f   , -0.46875f    },
  { 0.234375f   , -0.0f        },
  { 0.44921875f , -0.203125f   },
  { 0.3828125f  , -0.21484375f },
  { 0.4765625f  , -0.234375f   },
  { 0.5f        , -0.9375f     },
  { 0.234375f   , -0.9375f     },
  { 0.109375f   , -0.9375f     },
};

static usnd_size PSX_GetFrameCount(const usnd_audio_stream *s) {
  u32 num_samples = s->size / s->num_channels / PSX_BYTES_PER_FRAME * PSX_SAMPLES_PER_FRAME;
  return (num_samples + PSX_SAMPLES_PER_FRAME - 1) / PSX_SAMPLES_PER_FRAME;
}

static usnd_size PSX_DecodedSize(const usnd_audio_stream *s) {
  u32 frames = PSX_GetFrameCount(s);
  return frames * s->num_channels * PSX_SAMPLES_PER_FRAME * sizeof(usnd_sample);
}

static usnd_size PSX_Decode(const usnd_audio_stream *src, usnd_audio_stream *dst) {
  if (!dst)
    return PSX_DecodedSize(src);
  
  u32 history[PSX_MAX_CHANNELS][2];
  memset(history, 0, sizeof(history));
  u32 num_frames = PSX_GetFrameCount(src);
  
  const s8 *psx = (s8*)src->data;
  usnd_sample *pcm = dst->data;
  for (u32 i = 0; i < num_frames; i++) {
    for (u32 c = 0; c < dst->num_channels; c++) {
      u8 const predict = (*psx >> 4) & 0xF;
      u8 const shift = (*psx++ >> 0) & 0xF;
      u8 const flags = (*psx++); (void)flags; /*todo?*/
      
      if (predict > 4)
        return 0;
      
      s32 hst1 = history[c][0];
      s32 hst2 = history[c][1];
      s32 expanded[PSX_SAMPLES_PER_FRAME];
      for (u32 y = 0; y < PSX_SAMPLE_BYTES_PER_FRAME; y++) {
        expanded[y * 2 + 0] = (psx[y] & 0xF);
        expanded[y * 2 + 1] = (psx[y] & 0xF0) >> 4;
      }
      
      for (u32 s = 0; s < PSX_SAMPLES_PER_FRAME; s++) {
        s32 sample = expanded[s] << 12;
        if ((sample & 0x8000) != 0)
          sample = (sample | 0xFFFF0000);
        
        float output = (sample >> shift);
        output += hst1 * PSXADPCMCoefficients[predict][0];
        output += hst2 * PSXADPCMCoefficients[predict][1];
        
        hst2 = hst1;
        hst1 = clamp(output, SHRT_MIN, SHRT_MAX);
        pcm[s * dst->num_channels + c] = hst1;
      }
      
      history[c][0] = hst1;
      history[c][1] = hst2;
      psx += PSX_SAMPLE_BYTES_PER_FRAME;
    }
    
    pcm += PSX_SAMPLES_PER_FRAME * dst->num_channels;
  }
  
  return 1;
}

#pragma mark - UBI ADPCM
/* 4/6-bit ADPCM, similar to IMA ADPCM. Frames are split up into blocks */
/* which contain the compressed sample data inputs. Every frame begins with */
/* the state for each of the channels. Frames = {Block[[ch][input]]...} */

#define UBI_MAX_CHANNELS 2
#define UBI_MAX_BLOCKS_PER_FRAME 2
#define UBI_MAX_BLOCK_SIZE 1536
#define UBI_MAX_FRAME_SIZE (0x34 * UBI_MAX_CHANNELS + ())

//typedef struct {
//  u32 magic;              /* must be 8 */
//  u32 channels;           /* 1,2,4,6 */
//  u32 block_samples;      /* samples per block */
//  u32 sample_rate;
//  u32 bits_per_sample;    /* usually 4 */
//  u32 samples_per_block;
//  u32 total_samples;
//  u32 loop_start;
//  u32 loop_end;
//  u32 reserved[3];
//} UBI_ADPCM_Header;
//
///* per-channel predictor state (0x34 bytes) */
//typedef struct {
//  s32 predictor;
//  s32 step_index;   /* initialized to 5 */
//  s32 coeff1;
//  s32 coeff2;
//  s16 hist1;
//  s16 hist2;
//  s16 pad0[8];
//  s16 loop_hist1;
//  s16 loop_hist2;
//  s16 loop_step;
//  s16 pad1[5];
//} UBI_ChannelState;

/* TAdpcmHeader */
struct UBIADPCM_HEADER {
  u32 signature;
  u32 total_samples; 
  u32 total_subframes; 
  u32 codes_in_last_subframe; 
  u32 codes_per_subframe; 
  u32 subframes_per_frame; 
  u32 unknown1; 
  u32 unknown2; 
  u32 unknown3; 
  u32 bits_per_sample; 
  u32 unknown4; 
  u32 num_channels;
};

/* TAdpcmChannel */
struct UBIADPCM_CHANNEL {
  u32 signature;
  s32 step1;
  s32 next1;
  s32 next2;
  s16 coef1;
  s16 coef2;
  s16 unused1;
  s16 unused2;
  s16 mod1;
  s16 mod2;
  s16 mod3;
  s16 mod4;
  s16 hist1;
  s16 hist2;
  s16 unused3;
  s16 unused4;
  s16 delta1;
  s16 delta2;
  s16 delta3;
  s16 delta4;
  s16 delta5;
  s16 unused5;
};

/* TAdpcm */
struct UBIADPCM {
  /* @0x00: (CADPCMDataProvider*) */
  /* @0x04: TAdpcmHeader */
  struct UBIADPCM_HEADER header;
  /* @0x30: Decoder state */
  uint32_t decoder_state;
  /* @0x34: Sample buffer (uint32_t*, 0x20) */
  uint32_t unknown_sample_buffer[0x20];
  /* @0x38: (TAdpcmChannel*) */
  struct UBIADPCM_CHANNEL channel[UBI_MAX_CHANNELS];
  /* @0x3C: Some boolean flag */
  u8 b1;
  
  /* @0x54: Output format */
  u16 output_format;
  /* @0x56: Output num channels */
  u16 output_channel_count;
  
  /* @0x78: Header initialized? */
  u8 header_initialized;
};

static void S_UBIADPCM_HEADER(struct usnd_flow *flow, struct UBIADPCM_HEADER *hdr) {
  S_u32(flow, &hdr->signature);
  S_u32(flow, &hdr->total_samples);
  S_u32(flow, &hdr->total_subframes);
  S_u32(flow, &hdr->codes_in_last_subframe);
  S_u32(flow, &hdr->codes_per_subframe);
  S_u32(flow, &hdr->subframes_per_frame);
  S_u32(flow, &hdr->unknown1);
  S_u32(flow, &hdr->unknown2);
  S_u32(flow, &hdr->unknown3);
  S_u32(flow, &hdr->bits_per_sample);
  S_u32(flow, &hdr->unknown4);
  S_u32(flow, &hdr->num_channels);
}

static void S_UBIADPCM_CHANNEL(struct usnd_flow *flow, struct UBIADPCM_CHANNEL *ch) {
  S_u32(flow, (void*)&ch->signature);
  S_u32(flow, (void*)&ch->step1);
  S_u32(flow, (void*)&ch->next1);
  S_u32(flow, (void*)&ch->next2);
  S_u16(flow, (void*)&ch->coef1);
  S_u16(flow, (void*)&ch->coef2);
  S_u16(flow, (void*)&ch->unused1);
  S_u16(flow, (void*)&ch->unused2);
  S_u16(flow, (void*)&ch->mod1);
  S_u16(flow, (void*)&ch->mod2);
  S_u16(flow, (void*)&ch->mod3);
  S_u16(flow, (void*)&ch->mod4);
  S_u16(flow, (void*)&ch->hist1);
  S_u16(flow, (void*)&ch->hist2);
  S_u16(flow, (void*)&ch->unused3);
  S_u16(flow, (void*)&ch->unused4);
  S_u16(flow, (void*)&ch->delta1);
  S_u16(flow, (void*)&ch->delta2);
  S_u16(flow, (void*)&ch->delta3);
  S_u16(flow, (void*)&ch->delta4);
  S_u16(flow, (void*)&ch->delta5);
  S_u16(flow, (void*)&ch->unused5);
}



#define sign(x) ((x)<0?-1:0)

static const s32 UBI_StepAdjust[] = {
  -1536, 2314, 5243, 8192, 14336, 25354, 45445, 143626,
};

static const s32 UBI_StepBias[] = {
  -100000000, 8, 269, 425, 545, 645, 745, 850
};

static const s32 UBI_DeltaTable[] = {
  1024, 1031, 1053, 1076, 1099, 1123,
  1148, 1172, 1198, 1224, 1251, 1278,
  1306, 1334, 1363, 1393, 1423, 1454,
  1485, 1518, 1551, 1584, 1619, 1654,
  1690, 1726, 1764, 1802, 1841, 1881,
  1922, 1964, 2007,
};

static s16 UBI_Expand4BitCode(u8 code, struct UBIADPCM_CHANNEL* ch) {
  s32 code_signed = (s32)code - 7;
  s32 abs_code = abs(code_signed);
  /* step size update */
  s32 step_next = ch->step1 + UBI_StepBias[abs_code];

  s32 step = ((ch->step1 * 246) + UBI_StepAdjust[abs_code]) >> 8;
  step = clamp(step, 271, 2560);
  
  ch->step1 = step;
  
  /* delta */
  s32 delta = 0;
  if ((step_next >> 8) >= 1) {
    s32 shift = (step_next >> 8) & 0xFF;
    s32 index = (step_next >> 3) & 0x1F;
    delta = ((sign(code_signed) * UBI_DeltaTable[index]) << shift) >> 10;
  }
  
  /* predictor */
  s32 next = (ch->mod1 * ch->delta1 + ch->mod2 * ch->delta2 +
              ch->mod3 * ch->delta3 + ch->mod4 * ch->delta4) >> 10;
  s32 sample = delta + next + ((ch->coef1 * ch->hist1 + ch->coef2 * ch->hist2) >> 10);
  
  /* history update */
  ch->hist2 = ch->hist1;
  ch->hist1 = (s16)sample;
  
  /* coefficient adaptation */
  if (delta + next == 0) {
    ch->coef1 = (ch->coef1 * 255) >> 8;
    ch->coef2 = (ch->coef2 * 254) >> 8;
  } else {
    s32 s0 = sign(delta + next);
    s32 s1 = sign(ch->delta2 + ch->next1);
    s32 s2 = sign(ch->delta3 + ch->next2);
    ch->coef1 = (ch->coef1 * 255 + s0 * s1 * 0xC00) >> 8;
    ch->coef2 = (ch->coef2 * 254 +(s0 * s2 * 0x400 - s1 * ch->coef1) * 2) >> 8;
  }
  
  /* clamp */
  ch->coef2 = clamp(ch->coef2, -0x300, 0x300);
  s32 max_c1 = 0x3C0 - abs(ch->coef2);
  ch->coef1 = clamp(ch->coef1, -max_c1, max_c1);
  
  /* delta update */
  ch->delta5 = ch->delta4;
  ch->delta4 = ch->delta3;
  ch->delta3 = ch->delta2;
  ch->delta2 = ch->delta1;
  ch->delta1 = (int16_t)delta;
  
  ch->next2 = ch->next1;
  ch->next1 = next;
  
  /* modulator */
  s8 sign_d1 = sign(ch->delta1);
  ch->mod1 = clamp(ch->mod1 * 255 + 2048 * sign_d1 * sign(ch->delta2), SHRT_MIN, SHRT_MAX) >> 8;
  ch->mod2 = clamp(ch->mod2 * 255 + 2048 * sign_d1 * sign(ch->delta3), SHRT_MIN, SHRT_MAX) >> 8;
  ch->mod3 = clamp(ch->mod3 * 255 + 2048 * sign_d1 * sign(ch->delta4), SHRT_MIN, SHRT_MAX) >> 8;
  ch->mod4 = clamp(ch->mod4 * 255 + 2048 * sign_d1 * sign(ch->delta5), SHRT_MIN, SHRT_MAX) >> 8;
  
  return (s16)sample;
}

static usnd_size UBI_UnpackInput(const u8 *input, s32 *output, u32 code_count, u32 bps) {
  assert(code_count <= UBI_MAX_BLOCK_SIZE);
  assert(bps == 4 || bps == 6);
  
  uint64_t bit_buffer = 0;
  uint32_t bits_available = 0;
  uint32_t mask = (1u << bps) - 1;
  const u8 *start = input;
  while (code_count--) {
    /* ensure enough bits are in the buffer */
    if (bits_available < bps) {
      bit_buffer = (bit_buffer << 8) | *input++;
      bits_available += 8;
    }
    /* extract next code */
    bits_available -= bps;
    *output++ = (s32)((bit_buffer >> bits_available) & mask);
  }
  
  return (usnd_size)(input - start);
}

static usnd_size UBI_DecodeBlock(usnd_flow *flow, struct UBIADPCM_CHANNEL *ch,
  u32 num_channels, u32 num_codes, usnd_sample *pcm, u32 bits_per_sample)
{
  s32 codes[UBI_MAX_BLOCK_SIZE];
  /* only 4-bit for now */
  assert(bits_per_sample == 4);
  
  usnd_size read = UBI_UnpackInput(flow->buf + flow->pos,
    codes, num_codes, bits_per_sample);

  usnd_flow_advance(flow, read);
  usnd_flow_advance(flow, 1); /* padding */
  
  /* mono */
  if (num_channels == 1) {
    for (u32 i = 0; i < num_codes; i++)
      pcm[i] = UBI_Expand4BitCode((u8)codes[i], &ch[0]);
    return num_codes;
  }

  /* stereo */
  for (u32 i = 0; i < num_codes; i += 8) {
    s16 tmp[8];
    tmp[0] = UBI_Expand4BitCode(codes[i+0], &ch[0]);
    tmp[1] = UBI_Expand4BitCode(codes[i+2], &ch[0]);
    tmp[2] = UBI_Expand4BitCode(codes[i+4], &ch[0]);
    tmp[3] = UBI_Expand4BitCode(codes[i+6], &ch[0]);
    
    tmp[4] = UBI_Expand4BitCode(codes[i+1], &ch[1]);
    tmp[5] = UBI_Expand4BitCode(codes[i+3], &ch[1]);
    tmp[6] = UBI_Expand4BitCode(codes[i+5], &ch[1]);
    tmp[7] = UBI_Expand4BitCode(codes[i+7], &ch[1]);
    
    /* joint stereo transform */
    pcm[0] = clamp(tmp[0] + tmp[4], SHRT_MIN, SHRT_MAX);
    pcm[1] = clamp(tmp[0] - tmp[4], SHRT_MIN, SHRT_MAX);

    pcm[2] = clamp(tmp[1] + tmp[5], SHRT_MIN, SHRT_MAX);
    pcm[3] = clamp(tmp[1] - tmp[5], SHRT_MIN, SHRT_MAX);

    pcm[4] = clamp(tmp[2] + tmp[6], SHRT_MIN, SHRT_MAX);
    pcm[5] = clamp(tmp[2] - tmp[6], SHRT_MIN, SHRT_MAX);

    pcm[6] = clamp(tmp[3] + tmp[7], SHRT_MIN, SHRT_MAX);
    pcm[7] = clamp(tmp[3] - tmp[7], SHRT_MIN, SHRT_MAX);

    pcm += 8;
  }

  return num_codes;
}

static usnd_size UBI_DecodeFrame(usnd_flow *flow,
  const struct UBIADPCM_HEADER *hdr, u32 *subframe_n, usnd_sample *pcm)
{
  struct UBIADPCM_CHANNEL channel[UBI_MAX_CHANNELS];
  for (u32 i = 0; i < hdr->num_channels; i++) {
    S_UBIADPCM_CHANNEL(flow, &channel[i]);
    assert(channel[i].signature == 2);
  }

  u32 code_count_a, code_count_b;
  if (*subframe_n + 1 == hdr->total_subframes) {
    code_count_a = hdr->codes_in_last_subframe;
    code_count_b = 0;
  } else if (*subframe_n + 2 == hdr->total_subframes) {
    code_count_a = hdr->codes_per_subframe;
    code_count_b = hdr->codes_in_last_subframe;
  } else {
    code_count_a = hdr->codes_per_subframe;
    code_count_b = hdr->codes_per_subframe;
  }

  usnd_size produced = 0;

  /* Subframe A */
  if (code_count_a) {
    produced += UBI_DecodeBlock(flow, channel, hdr->num_channels,
      code_count_a, pcm + produced, hdr->bits_per_sample);
  }
  
  /* Subframe B */
  if (code_count_b) {
    produced += UBI_DecodeBlock(flow, channel, hdr->num_channels,
      code_count_b, pcm + produced, hdr->bits_per_sample);
  }
    
  *subframe_n += 2;
  return produced;
}

static usnd_size UBI_Decode(const usnd_audio_stream *src, usnd_audio_stream *dst) {
  usnd_flow flow = {};
  flow.mode = USND_READ;
  flow.buf = (u8*)src->data;
  flow.size = src->size;
  flow.endianness = USND_LITTLE_ENDIAN;

  struct UBIADPCM_HEADER hdr = {0};
  S_UBIADPCM_HEADER(&flow, &hdr);

  assert(hdr.signature == 8);
  assert(hdr.num_channels <= 2);

  usnd_size total_samples = hdr.total_samples / hdr.num_channels;
  usnd_size output_size = total_samples * hdr.num_channels * sizeof(usnd_sample);

  if (!dst)
    return output_size;

  dst->num_channels = hdr.num_channels;
  dst->sample_rate = src->sample_rate;
  dst->format = USND_AUDIO_FORMAT_PCM;
  dst->size = output_size;

  usnd_sample *pcm = (usnd_sample*)dst->data;
  u32 subframe_n = 0;

  while (subframe_n < hdr.total_subframes && flow.pos < flow.size)
    pcm += UBI_DecodeFrame(&flow, &hdr, &subframe_n, pcm);
  
  return output_size;
}
