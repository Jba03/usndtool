#ifndef common_h
#define common_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define USND_LITTLE_ENDIAN  0
#define USND_BIG_ENDIAN     1
#define USND_NATIVE_ENDIAN (!*(u8*)&(u16){1})

/* Recommended limits */
#define USND_STRING_MAX 64 /* 34 max seen */
#define USND_MAX_LINKS 256 /* 240 max seen (PK, 00000003-002E-019A) */
#define USND_MAX_LANGUAGES 10 /* 10 language areas in CPA */
#define USND_MAX_ENTRIES 2500 /* ~2200 max seen */

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef float     f32;

typedef u8 usnd_endian;
typedef u32 usnd_offset;
typedef u32 usnd_size;

u16 usnd_bswap16(u16);
u32 usnd_bswap32(u32);
usnd_endian usnd_test_endian(const u32 *data, u32 maxval);

#pragma mark - Types

enum usnd_class {
  CRTTIClass,
  CIdObjInfo,
  CIdObj,
  CRefIdObj,
  /* ResData */
  CResData,
  CEventResData,
  CRandomResData,
  CProgramResData,
  CSwitchResData,
  CActorResData,
  /* WavResData */
  CWavResData,
  CPCWavResData,
  CGCWavResData,
  CPS2WavResData,
  /* WaveFileIdObj */
  CWaveFileIdObj,
  CPCWaveFileIdObj,
  CGCWaveFileIdObj,
  CPS2WaveFileIdObj,
  /* ResObj */
  CEventResObj,
  CRandomResObj,
  CProgramResObj,
  CActorResObj,
  CWavResObj,
  /* and over 100 more classes... */
  USND_CLASS_MAX
};

enum usnd_version {
  USND_VERSION_PC,
  USND_VERSION_GC,
  USND_VERSION_PS2,
  USND_VERSION_PS3,
  USND_VERSION_XBOX,
  USND_VERSION_XBOX360,
  USND_VERSION_UNKNOWN,
};

usnd_endian usnd_version_endianness(enum usnd_version);

typedef u64 usnd_uuid;
#define USND_INVALID_UUID 0
#define USND_UUID_LOW(u) ((u) & 0xFFFFFFFF)
#define USND_UUID_HIGH(u) ((u) >> 32)
/* Get the group of a UUID, which is the high part
 * of the UUID's low part, e.g. 00000001xxxx00A3 */
#define USND_UUID_GROUP(u) (USND_UUID_LOW(u) >> 16)
#define USND_UUID_MAX_GROUPS 0xFFFF
/* Hardcoded UUIDs for ThemeProgram and ThemeActor */
#define USND_THEMEPROGRAM_UUID 0x2ACA41B9C9138EA1
#define USND_THEMEACTOR_UUID   0x2ACB403911EC579D

typedef u32 usnd_event_type;
#define USND_EVENT_PLAY         0
#define USND_EVENT_STOP         1
#define USND_EVENT_PITCH        2
#define USND_EVENT_VOLUME       3
#define USND_EVENT_EXTRA        4
#define USND_EVENT_STOPALL      5
#define USND_EVENT_STOP_AND_GO  6

typedef u32 usnd_transition_type;
#define USND_TRANSITION_CROSSFADE   0 /* Fade to next theme */
#define USND_TRANSITION_END_OF_PART 1 /* End without delay */
#define USND_TRANSITION_OUTRO       2 /* Ending outro part */

typedef u32 usnd_var_basetype;
#define USND_VAR_BASETYPE_INT     0
#define USND_VAR_BASETYPE_FLOAT   1
#define USND_VAR_BASETYPE_STRING  2
#define USND_VAR_BASETYPE_BOOL    3
#define USND_VAR_BASETYPE_CHAR    4

typedef u32 usnd_var_type;
#define USND_VAR_TYPE_BASETYPE  0
#define USND_VAR_TYPE_ARRAY     1
#define USND_VAR_TYPE_STRUCT    2
#define USND_VAR_TYPE_OBJECT    3
#define USND_VAR_TYPE_RESID     4
#define USND_VAR_TYPE_ENUM      5

typedef u32 usnd_audio_format;
#define USND_AUDIO_FORMAT_PCM 0x01 /* PCM s16 */
#define USND_AUDIO_FORMAT_UBI 0x02 /* UBI ADPCM */
#define USND_AUDIO_FORMAT_PSX 0x03 /* PS ADPCM */
#define USND_AUDIO_FORMAT_DSP 0x04 /* GC 4-bit ADPCM */
#define USND_AUDIO_FORMAT_IMA 0x05 /* MS IMA ADPCM */
#define USND_AUDIO_FORMAT_MP3 0x55 /* MPEG-3 */

typedef u32 usnd_language;
#define USND_LANGUAGE_GERMAN  0x64652020 /* 'de  ' */
#define USND_LANGUAGE_ENGLISH 0x656E2020 /* 'en  ' */
#define USND_LANGUAGE_SPANISH 0x65732020 /* 'es  ' */
#define USND_LANGUAGE_FRENCH  0x66722020 /* 'fr  ' */
#define USND_LANGUAGE_ITALIAN 0x69742020 /* 'it  ' */

const char *const usnd_event_type_name(usnd_event_type);
const char *const usnd_audio_format_name(usnd_audio_format);
const char *const usnd_language_name(usnd_language);
const char *const usnd_version_name(enum usnd_version);

#pragma mark - Memory

typedef u32 usnd_arena_flags;
#define USND_ARENA_FLAGS_DUMMY (1 << 0)

typedef struct usnd_arena usnd_arena;
struct usnd_arena {
  usnd_arena_flags flags;
  u8 *base;
  usnd_size size;
  usnd_offset position;
  u32 counter;
};

#define usnd_arena_alloc(data, data_size, flag) \
  (usnd_arena){ .base = (data), .size = (data_size), .flags = (flag) }

void *usnd_arena_push(usnd_arena*, usnd_size);
void *usnd_arena_pop(usnd_arena*, usnd_size);
void usnd_arena_reset(usnd_arena*, usnd_offset);
void usnd_arena_clear(usnd_arena*);

#pragma mark - Serialization

enum usnd_rw_mode {
  USND_READ,
  USND_WRITE
};

typedef struct usnd_flow usnd_flow;
struct usnd_flow {
  enum usnd_rw_mode mode;
  usnd_size size;
  usnd_offset pos;
  u8 *buf;
  usnd_endian endianness;
  usnd_arena *arena;
};

void usnd_flow_seek(usnd_flow*, usnd_offset);
void usnd_flow_advance(usnd_flow*, usnd_offset);
void usnd_flow_rw(usnd_flow*, void*, usnd_size);

int S_u8(usnd_flow*, u8*);
int S_u16(usnd_flow*, u16*);
int S_u32(usnd_flow*, u32*);
int S_f32(usnd_flow*, f32*);
int S_string(usnd_flow*, char**);
int S_uuid(usnd_flow*, usnd_uuid*);

#pragma mark - Utils

#define USND_FOURCC(S) (u32)((S[3]<<24) | (S[2]<<16) | (S[1]<<8) | S[0])

#ifdef __cplusplus
}
#endif

#endif /* common_h */
