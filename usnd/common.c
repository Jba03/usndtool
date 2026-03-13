#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

u16 usnd_bswap16(u16 x) {
  u16 high = x << 8;
  u16 low = x >> 8;
  return high|low;
}

u32 usnd_bswap32(u32 x) {
  u32 high = (u32)usnd_bswap16(x) << 16;
  u32 low = (u32)usnd_bswap16(x >> 16);
  return high|low;
}

usnd_endian usnd_test_endian(const u32 *data, u32 maxval) {
  u32 tmp; memcpy(&tmp, data, sizeof(u32));
  return (tmp > maxval) ? !(USND_NATIVE_ENDIAN) : USND_NATIVE_ENDIAN;
}

#pragma mark - Memory

#define USND_ALIGN(size, x) ((size) + ((x) - (size) % (x)) % (x))
/* stdalign.h is not available in C99, so this will have to do */
#define alignof(type) offsetof(struct { char c; type t; }, t)
static const usnd_size USND_ALIGNMENT = alignof(uintmax_t);

void *usnd_arena_push(usnd_arena *arena, usnd_size size) {
  u32 aligned_size = USND_ALIGN(size, USND_ALIGNMENT);
  if (!(arena->flags & USND_ARENA_FLAGS_DUMMY))
    assert(arena->position + aligned_size <= arena->size && "arena out of space");
  void *p = arena->base + arena->position;
  arena->position += aligned_size;
  
  ++arena->counter;
  if (arena->flags & USND_ARENA_FLAGS_DUMMY) {
    usnd_arena_clear(arena);
    return arena->base;
  }
  
  return p;
}

void *usnd_arena_pop(usnd_arena *arena, usnd_size size) {
  u32 aligned_size = USND_ALIGN(size, USND_ALIGNMENT);
  if (!(arena->flags & USND_ARENA_FLAGS_DUMMY))
    assert((s64)arena->position - aligned_size >= 0 && "arena pos<0");
  arena->position -= aligned_size;
  
  --arena->counter;
  if (arena->flags & USND_ARENA_FLAGS_DUMMY)
    return arena->base;
  
  return arena->base + arena->position;
}

void usnd_arena_reset(usnd_arena *arena, usnd_offset pos) {
  arena->position = pos;
}

void usnd_arena_clear(usnd_arena *arena) {
  memset(arena->base, 0, arena->size);
  arena->position = 0;
}

#pragma mark - Serialization

#define bswap(sz, cond) if (flow->endianness != USND_NATIVE_ENDIAN && (cond)) \
{ \
  u##sz tmp; \
  memcpy(&tmp, data, sizeof(tmp)); \
  tmp = usnd_bswap##sz(tmp); \
  memcpy(data, &tmp, sizeof(tmp)); \
}

void usnd_flow_seek(usnd_flow *flow, usnd_offset pos) {
  flow->pos = pos;
}

void usnd_flow_advance(usnd_flow *flow, usnd_offset offset) {
  flow->pos += offset;
}

void usnd_flow_rw(usnd_flow *flow, void *data, usnd_size size) {
  if (flow->mode == USND_READ)
    memcpy(data, flow->buf + flow->pos, size);
  else if (flow->mode == USND_WRITE)
    memcpy(flow->buf + flow->pos, data, size);
  flow->pos += size;
}

int S_u8(usnd_flow *flow, u8 *data) {
  usnd_flow_rw(flow, data, 1);
  return 1;
}

int S_u16(usnd_flow *flow, u16 *data) {
  u16 tmp; bswap(16, flow->mode == USND_WRITE)
  usnd_flow_rw(flow, data, 2); bswap(16, 1)
  memcpy(&tmp, data, sizeof(tmp));
  return 1;
}

int S_u32(usnd_flow *flow, u32 *data) {
  u32 tmp; bswap(32, flow->mode == USND_WRITE)
  usnd_flow_rw(flow, data, 4); bswap(32, 1)
  memcpy(&tmp, data, sizeof(tmp));
  return 1;
}

int S_f32(usnd_flow *flow, f32 *data) {
  f32 tmp; bswap(32, flow->mode == USND_WRITE)
  usnd_flow_rw(flow, data, 4); bswap(32, 1)
  memcpy(&tmp, data, sizeof(tmp));
  return 1;
}

int S_string(usnd_flow *flow, char **s) {
  u32 len = 0u;
//  if (flow->mode == USND_WRITE)
//    len = (u32)usnd_strlen(s);
  
  S_u32(flow, &len);
  if (len == 0) {
    *s = NULL;
    return 1;
  }
  
  *s = usnd_arena_push(flow->arena, len+1);
  if (!*s)
    return 0;
  
//  if (flow->mode == USND_READ)
//    memset(*s, '\0', len+1);

  usnd_flow_rw(flow, *s, len);
  (*s)[len] = '\0';

  return 1;
}

int S_uuid(usnd_flow *flow, usnd_uuid *uuid) {
  u64 tmp;
  u32 low, high;
  memcpy(&tmp, uuid, sizeof(tmp));

  low  = (u32)(tmp & 0xFFFFFFFF);
  high = (u32)(tmp >> 32);

  S_u32(flow, &high);
  S_u32(flow, &low);

  tmp = ((u64)high << 32) | low;
  memcpy(uuid, &tmp, sizeof(tmp));
  return 1;
}

#pragma mark -

usnd_endian usnd_version_endianness(enum usnd_version version) {
  if (version == USND_VERSION_PC) return USND_LITTLE_ENDIAN;
  if (version == USND_VERSION_GC) return USND_BIG_ENDIAN;
  if (version == USND_VERSION_PS2) return USND_LITTLE_ENDIAN;
  if (version == USND_VERSION_PS3) return USND_BIG_ENDIAN;
  if (version == USND_VERSION_XBOX) return USND_LITTLE_ENDIAN;
  if (version == USND_VERSION_XBOX360) return USND_BIG_ENDIAN;
  return USND_LITTLE_ENDIAN;
}
