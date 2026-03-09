#ifndef common_h
#define common_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

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

#ifdef __cplusplus
}
#endif

#endif /* common_h */
