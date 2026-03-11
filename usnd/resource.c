#include "resource.h"
#include "usnd.h"

#include <stddef.h>
#include <string.h>

int S_CIdObj(usnd_flow *flow, usnd_entry *data) {
  if (!S_uuid(flow, &data->uuid)) return 0;
  return 1;
}

int S_CRefIdObj(usnd_flow *flow, usnd_entry *data) {
  if (!S_CIdObj(flow, data)) return 0;
  return 1;
}

int S_CResData(usnd_flow *flow, usnd_entry *data) {
  if (!S_CRefIdObj(flow, data)) return 0;
  if (!S_u32(flow, &data->resource.version)) return 0;
  if (flow->mode == USND_READ)
    data->resource.name = NULL;
  return 1;
}

int S_CEventResData(usnd_flow *flow, usnd_entry *data) {
  struct CEventResData *evt = &data->resource.event;
  if (!S_CResData(flow, data)) return 0;
  if (!S_string(flow, &data->resource.name)) return 0;
  if (!S_u32(flow, &evt->type)) return 0;
  if (!S_uuid(flow, &evt->link_uuid)) return 0;
  if (!S_f32(flow, &evt->coeff_a)) return 0;
  if (!S_f32(flow, &evt->coeff_b)) return 0;
  if (!S_f32(flow, &evt->coeff_c)) return 0;
  if (!S_f32(flow, &evt->coeff_d)) return 0;
  return 1;
}

int S_CRandomResData(usnd_flow *flow, usnd_entry *e) {
  
}

int S_CProgramResData(usnd_flow *flow, usnd_entry *e) {
  
}

int S_CSwitchResData(usnd_flow *flow, usnd_entry *e) {
  
}

int S_CActorResData(usnd_flow *flow, usnd_entry *e) {
  
}

int S_CWavResData(usnd_flow *flow, usnd_entry *e) {
  
}

int S_CWaveFileIdObj(usnd_flow *flow, usnd_entry *e) {
  
}

