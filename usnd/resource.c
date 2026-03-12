#include "resource.h"
#include "usnd.h"
#include "vm.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#define S_vector(flow, count_ptr, bank_ptr, T, S) do {                    \
  if (!S_u32((flow), (count_ptr))) return 0;                              \
  if ((flow)->mode == USND_READ)                                          \
    (bank_ptr) = usnd_arena_push(flow->arena, sizeof(T) * (*count_ptr));  \
                                                                          \
  for (u32 _i = 0; _i < *(count_ptr); _i++) {                             \
    T _tmp;                                                               \
    u8 *_p = (u8*)(bank_ptr) + _i * sizeof(T);                            \
                                                                          \
    if ((flow)->mode == USND_READ) {                                      \
      if (!S((flow), &_tmp)) return 0;                                    \
      memcpy(_p, &_tmp, sizeof(T));                                       \
    } else {                                                              \
      memcpy(&_tmp, _p, sizeof(T));                                       \
      if (!S((flow), &_tmp)) return 0;                                    \
    }                                                                     \
  }                                                                       \
} while(0)

#define S_flag(flow, flags, x) do { \
  u32 tmp = ((*flags) & x) != 0; \
  if (!S_u32((flow), &tmp)) return 0; \
  if (tmp) (*flags) |= x; \
} while(0);

int S_CIdObj(usnd_flow *flow, usnd_entry *e) {
  if (!S_uuid(flow, &e->uuid)) return 0;
  return 1;
}

int S_CRefIdObj(usnd_flow *flow, usnd_entry *e) {
  if (!S_CIdObj(flow, e)) return 0;
  return 1;
}

int S_CResData(usnd_flow *flow, usnd_entry *e) {
  if (!S_CRefIdObj(flow, e)) return 0;
  if (!S_u32(flow, &e->resource.version)) return 0;
  if (flow->mode == USND_READ)
    e->resource.name = NULL;
  return 1;
}

int S_CEventResData(usnd_flow *flow, usnd_entry *e) {
  struct CEventResData *evt = &e->resource.event;
  if (!S_CResData(flow, e)) return 0;
  if (!S_string(flow, &e->resource.name)) return 0;
  if (!S_u32(flow, &evt->type)) return 0;
  if (!S_uuid(flow, &evt->link_uuid)) return 0;
  if (!S_f32(flow, &evt->coeff_a)) return 0;
  if (!S_f32(flow, &evt->coeff_b)) return 0;
  if (!S_f32(flow, &evt->coeff_c)) return 0;
  if (!S_f32(flow, &evt->coeff_d)) return 0;
  return 1;
}

static int S_CRandomElement(usnd_flow *flow, struct CRandomElement *element) {
  if (!S_f32(flow, &element->probability)) return 0;
  if (!S_uuid(flow, &element->uuid)) return 0;
  return 1;
}

int S_CRandomResData(usnd_flow *flow, usnd_entry *e) {
  struct CRandomResData *random = &e->resource.random;
  if (!S_CResData(flow, e)) return 0;
  if (!S_f32(flow, &random->volume)) return 0;
  if (!S_f32(flow, &random->fail_probability)) return 0;
  /* Elements */
  S_vector(flow, &random->num_elements, random->elements,
    struct CRandomElement, S_CRandomElement);
  assert(random->num_elements <= USND_MAX_LINKS);
  return 1;
}

static int S_CSwitchElement(usnd_flow *flow, struct CSwitchElement *e) {
  if (!S_u32(flow, &e->index)) return 0;
  if (!S_uuid(flow, &e->uuid)) return 0;
  return 1;
}

int S_CSwitchResData(usnd_flow *flow, usnd_entry *e) {
  struct CSwitchResData *_switch = &e->resource._switch;
  if (!S_CResData(flow, e)) return 0;
  if (!S_f32(flow, &_switch->volume)) return 0;
  if (!S_u32(flow, &_switch->type)) return 0;
  if (!S_u32(flow, &_switch->default_case)) return 0;
  S_vector(flow, &_switch->num_elements,
    _switch->elements, struct CSwitchElement, S_CSwitchElement);
  return 1;
}

static int S_CProgramHeader(usnd_flow *flow,
  struct CProgramHeader *hdr, u8 version)
{
  if (!S_u32(flow, &hdr->header_size)) return 0;
  if (!S_u32(flow, &hdr->num_sections)) return 0;
  if (!S_u32(flow, &hdr->main_stacksize)) return 0;
  if (!S_u32(flow, &hdr->init_stacksize)) return 0;
  if (!S_u32(flow, &hdr->destructor_stacksize)) return 0;
  if (!S_u32(flow, &hdr->num_handlers)) return 0;
  if (!S_u32(flow, &hdr->num_local_functions)) return 0;
  if (!S_u32(flow, &hdr->program_var_size)) return 0;
  if (!S_u32(flow, &hdr->constant_vars_pos)) return 0;
  if (!S_u32(flow, &hdr->constant_vars_size)) return 0;
  if (!S_u32(flow, &hdr->destructor_code_pos)) return 0;
  if (!S_u32(flow, &hdr->destructor_code_size)) return 0;
  if (!S_u32(flow, &hdr->init_code_pos)) return 0;
  if (!S_u32(flow, &hdr->init_code_size)) return 0;
  if (!S_u32(flow, &hdr->main_code_pos)) return 0;
  if (!S_u32(flow, &hdr->main_code_size)) return 0;
  if (!S_u32(flow, &hdr->fn_descriptor_pos)) return 0;
  if (!S_u32(flow, &hdr->fn_descriptor_size)) return 0;
  if (!S_u32(flow, &hdr->private_code_pos)) return 0;
  if (!S_u32(flow, &hdr->private_code_size)) return 0;
  if (!S_u32(flow, &hdr->var_descriptor_pos)) return 0;
  if (!S_u32(flow, &hdr->var_descriptor_size)) return 0;
  if (!S_u32(flow, &hdr->pre_destructor_code_pos)) return 0;
  if (!S_u32(flow, &hdr->pre_destructor_code_size)) return 0;
  
  if (version >= 8)
    if (!S_u32(flow, &hdr->num_object_vars)) return 0;
  
  if (version >= 9)
    if (!S_u32(flow, &hdr->flags)) return 0;
    
  return 1;
}

int S_CProgramResData(usnd_flow *flow, usnd_entry *e) {
  struct CProgramResData *program = &e->resource.program;
  if (!S_CResData(flow, e)) return 0;
  assert(e->resource.version >= 5);
  
  u32 header_size = 88;
  if (!S_u32(flow, &header_size)) return 0;
  assert(header_size == 88);
  
  /* header */
  struct CProgramHeader *header = program->header;
  if (flow->mode == USND_READ)
    header = usnd_arena_push(flow->arena, sizeof(struct CProgramHeader));
  if (!S_CProgramHeader(flow, header, e->resource.version))
    return 0;
  
  program->header = header;
  
  /* code */
  u32 code_size = usnd_program_size(header);
  if (flow->mode == USND_READ)
    program->code = usnd_arena_push(flow->arena, code_size);
  
  if (flow->arena->flags & USND_ARENA_FLAGS_DUMMY)
    usnd_flow_advance(flow, code_size);
  else
    usnd_flow_rw(flow, program->code, code_size);
  
  /* small hack here (object vars?) */
  if (e->uuid == USND_THEMEPROGRAM_UUID)
    usnd_flow_advance(flow, 8);
  
  /* Links */
  S_vector(flow, &program->num_links, program->links, usnd_uuid, S_uuid);
  
  if (flow->mode == USND_READ && e->uuid == USND_THEMEPROGRAM_UUID)
    e->resource.name = "Themeprogram";
  
  return 1;
}

static int S_CRollOffParam(usnd_flow *flow, struct CRollOffParam *param) {
  if (!S_f32(flow, &param->stabilization_distance)) return 0;
  if (!S_f32(flow, &param->saturation_distance)) return 0;
  if (!S_f32(flow, &param->stabilization_volume)) return 0;
  return 1;
}

static int S_CSndVarType(usnd_flow *flow, struct CSndVar *var) {
  if (!S_u32(flow, &var->type)) return 0;
  if (var->type == USND_VAR_TYPE_OBJECT) {
    if (!S_string(flow, &var->type_name)) return 0;
  } else if (var->type == USND_VAR_TYPE_BASETYPE) {
    if (!S_u32(flow, &var->basetype)) return 0;
  }
  return 1;
}

static int S_CSndVarData(usnd_flow *flow, struct CSndVar *var) {
  if (var->type == USND_VAR_TYPE_OBJECT) {
    if (!S_uuid(flow, &var->uuid)) return 0;
  } else if (var->type == USND_VAR_TYPE_BASETYPE) {
    if (var->basetype == USND_VAR_BASETYPE_STRING) {
      if (!S_string(flow, &var->string)) return 0;
    }
  }
  return 1;
}

static int S_CSndVar(usnd_flow *flow, struct CSndVar *var) {
  if (!S_string(flow, &var->name)) return 0;
  if (!S_CSndVarType(flow, var)) return 0;
  if (!S_CSndVarData(flow, var)) return 0;
  return 1;
}

int S_CActorResData(usnd_flow *flow, usnd_entry *e) {
  struct CActorResData *actor = &e->resource.actor;
  if (!S_CResData(flow, e)) return 0;
  if (!S_string(flow, &e->resource.name)) return 0;
  /* Program links */
  S_vector(flow, &actor->num_programs,
    actor->programs, usnd_uuid, S_uuid);
  
  /* Program vars */
  S_vector(flow, &actor->num_vars,
    actor->vars, struct CSndVar, S_CSndVar);
  
  if (!S_CRollOffParam(flow, &actor->rolloff)) return 0;
  if (!S_u32(flow, &actor->var_access)) return 0;
  return 1;
}

static int S_CWavResLink(usnd_flow *flow, struct CWavResLink *link) {
  if (!S_u32(flow, &link->language)) return 0;
  if (!S_uuid(flow, &link->uuid)) return 0;
  return 1;
}

int S_CWavResData(usnd_flow *flow, usnd_entry *e) {
  struct CWavResData *wav = &e->resource.wav;
  if (!S_CResData(flow, e))
    return 0;
  
  if (flow->mode == USND_READ) {
    wav->flags = 0;
    wav->num_links = 0;
  }
  
  if (e->resource.version >= 2) {
    if (flow->mode == USND_WRITE && (e->type != CPCWavResData)) {
      /* on most platforms, names aren't written */
      u32 zero = 0;
      if (!S_u32(flow, &zero))
        return 0;
    } else {
      if (!S_string(flow, &e->resource.name))
        return 0;
    }
  }
  
  if (!S_f32(flow, &wav->coeff_a)) return 0;
  if (!S_f32(flow, &wav->coeff_b)) return 0;
  if (!S_f32(flow, &wav->coeff_c)) return 0;
  
  if (e->resource.version < 3) {
    u32 unknown = 0;
    if (!S_u32(flow, &unknown))
      return 0;
    
    if (e->resource.version < 2) {
      u32 unknown = 0;
      if (!S_u32(flow, &unknown)) return 0;
      if (!S_u32(flow, &unknown)) return 0;
      if (!S_u32(flow, &unknown)) return 0;
      if (!S_u32(flow, &unknown)) return 0;
      if (!S_u32(flow, &unknown)) return 0;
    } else {
      S_flag(flow, &wav->flags, CWavResDataFlags_0)
      S_flag(flow, &wav->flags, CWavResDataFlags_1)
    }
  } else {
    if (e->resource.version == 4) {
      S_flag(flow, &wav->flags, CWavResDataFlags_0)
      S_flag(flow, &wav->flags, CWavResDataFlags_1)
      S_flag(flow, &wav->flags, CWavResDataFlags_2)
    } else {
      u8 flags = wav->flags;
      if (!S_u8(flow, &flags)) return 0;
      wav->flags = flags;
    }
  }
  
  if (!S_uuid(flow, &wav->default_link))
    return 0;
  
  if (wav->flags & 2) {
    S_vector(flow, &wav->num_links, wav->links,
      struct CWavResLink, S_CWavResLink);
  }
  
  if (e->resource.version == 4 && !(wav->flags & CWavResDataFlags_1)) {
    u32 zero = 0;
    if (!S_u32(flow, &zero))
      return 0;
  }
  
  return 1;
}

int S_CWaveFileIdObj(usnd_flow *flow, usnd_entry *e) {
  struct CWaveFileIdObj *wavefile_obj = &e->wavefile;
  if (!S_CRefIdObj(flow, e)) return 0;
  if (!S_u32(flow, &wavefile_obj->version)) return 0;
  if (!S_f32(flow, &wavefile_obj->volume)) return 0;
  
  if (flow->mode == USND_READ) {
    wavefile_obj->filename = NULL;
    wavefile_obj->flags = 0;
  }
  
  if (e->wavefile.version < 3 || e->type == CGCWaveFileIdObj) {
    S_flag(flow, &wavefile_obj->flags, CWaveFileIdObjFlags_External);
    S_flag(flow, &wavefile_obj->flags, CWaveFileIdObjFlags_Filetype);
  } else {
    if (!S_u8(flow, &wavefile_obj->flags))
      return 0;
  }
  
  if (e->wavefile.flags & CWaveFileIdObjFlags_External) {
    if (!S_string(flow, &wavefile_obj->filename))
      return 0;
  }
  
  if (flow->mode == USND_READ) {
    /* TODO: Read wavefile */
  } else if (flow->mode == USND_WRITE) {
    /* TODO: Write wavefile */
  }
  
  return 1;
}
