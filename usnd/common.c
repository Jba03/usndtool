#include "common.h"

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

#pragma mark - Memory

void *usnd_arena_push(usnd_arena *arena, usnd_size size) {
  
}

void usnd_arena_reset(usnd_arena *arena, usnd_offset pos) {
  
}

void usnd_arena_clear(usnd_arena *arena) {
  
}

#pragma mark - Serialization

void usnd_flow_seek(usnd_flow *flow, usnd_offset pos) {
  flow->pos = pos;
  return 1;
}

void usnd_flow_advance(usnd_flow *flow, usnd_offset offset) {
  flow->pos += offset;
  return 1;
}

void usnd_flow_rw(usnd_flow *flow, void *data, usnd_size size) {
  if (flow->mode == USND_READ)
    memcpy(data, flow->buf + flow->pos, size);
  else if (flow->mode == USND_WRITE)
    memcpy(flow->buf + flow->pos, data, size);
  flow->pos += size;
}

int S_u8(usnd_flow *flow, u8 *data) {
  
}

int S_u16(usnd_flow *flow, u16 *data) {
  
}

int S_u32(usnd_flow *flow, u32 *data) {
  
}

int S_f32(usnd_flow *flow, f32 *data) {
  
}

int S_string(usnd_flow *flow, char **data) {
  
}

