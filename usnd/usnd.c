#include "usnd.h"
#include <string.h>

int S_CResData(usnd_flow*, usnd_entry*);
int S_CEventResData(usnd_flow*, usnd_entry*);
int S_CRandomResData(usnd_flow*, usnd_entry*);
int S_CProgramResData(usnd_flow*, usnd_entry*);
int S_CSwitchResData(usnd_flow*, usnd_entry*);
int S_CActorResData(usnd_flow*, usnd_entry*);
int S_CWavResData(usnd_flow*, usnd_entry*);
int S_CWaveFileIdObj(usnd_flow*, usnd_entry*);
int S_CIdObj(usnd_flow*, usnd_entry*);
int S_CRefIdObj(usnd_flow*, usnd_entry*);
int S_CRTTIClass(usnd_flow*, usnd_entry*);

#pragma mark -

static struct {
  const char* name;
  enum usnd_class base;
  int (*S)(usnd_flow*, usnd_entry*);
} class[] = {
  [CRTTIClass]        = {"CRTTIClass",        CRTTIClass,     S_CRTTIClass      },
  [CIdObj]            = {"CIdObj",            CRTTIClass,     S_CIdObj          },
  [CRefIdObj]         = {"CRefIdObj",         CIdObj,         S_CRefIdObj       },
  /* .......................................................................... */
  [CResData]          = {"CResData",          CRefIdObj,      S_CResData        },
  [CEventResData]     = {"CEventResData",     CResData,       S_CEventResData   },
  [CRandomResData]    = {"CRandomResData",    CResData,       S_CRandomResData  },
  [CProgramResData]   = {"CProgramResData",   CResData,       S_CProgramResData },
  [CSwitchResData]    = {"CSwitchResData",    CResData,       S_CSwitchResData  },
  [CActorResData]     = {"CActorResData",     CResData,       S_CActorResData   },
  /* .......................................................................... */
  [CWavResData]       = {"CWavResData",       CResData,       S_CWavResData     },
  [CPCWavResData]     = {"CPCWavResData",     CWavResData,    S_CWavResData     },
  [CPS2WavResData]    = {"CPS2WavResData",    CWavResData,    S_CWavResData     },
  [CGCWavResData]     = {"CGCWavResData",     CWavResData,    S_CWavResData     },
  /* .......................................................................... */
  [CWaveFileIdObj]    = {"CWaveFileIdObj",    CRefIdObj,      S_CWaveFileIdObj  },
  [CPCWaveFileIdObj]  = {"CPCWaveFileIdObj",  CWaveFileIdObj, S_CWaveFileIdObj  },
  [CPS2WaveFileIdObj] = {"CPS2WaveFileIdObj", CWaveFileIdObj, S_CWaveFileIdObj  },
  [CGCWaveFileIdObj]  = {"CGCWaveFileIdObj",  CWaveFileIdObj, S_CWaveFileIdObj  },
  /* .......................................................................... */
  [USND_CLASS_MAX]    = { NULL }
};

const char *const usnd_class_name(enum usnd_class c) {
  return class[c].name;
}

enum usnd_class usnd_get_class(const char *classname) {
  for (enum usnd_class i = CRTTIClass; i < USND_CLASS_MAX; i++)
    if (class[i].S && strcmp(classname, class[i].name) == 0) return i;
  return CRTTIClass;
}

enum usnd_class usnd_parent_class(enum usnd_class child) {
  return class[child].base;
}

enum usnd_class usnd_general_class(enum usnd_class c) {
  switch (c) {
    case CWavResData:
    case CGCWavResData:
    case CPS2WavResData:
    case CPCWavResData: return CWavResData;
    case CWaveFileIdObj:
    case CPCWaveFileIdObj:
    case CPS2WaveFileIdObj:
    case CGCWaveFileIdObj: return CWaveFileIdObj;
    default: return c;
  }
}

int usnd_instance_of(enum usnd_class child, enum usnd_class parent) {
  while (child != CRTTIClass || !(child == parent == CRTTIClass)) {
    if (child == parent) return 1;
    child = usnd_parent_class(child);
  }
  return 0;
}

#pragma mark - Index

int S_CRefObjectLanguage(usnd_flow *flow, struct CRefObjectLanguage *lang) {
  if (!S_u32(flow, &lang->language)) return 0;
  if (!S_u32(flow, &lang->unknown)) return 0;
  if (!S_uuid(flow, &lang->uuid)) return 0;
  return 1;
}

int S_CRefObjectCont(usnd_flow *flow, struct CRefObjectCont *refobj) {
  if (!S_u32(flow, &refobj->num_references)) return 0;
  for (u32 i = 0; i < refobj->num_references; i++)
    if (!S_uuid(flow, &refobj->references[i])) return 0;
  
  if (!S_u32(flow, &refobj->num_languages)) return 0;
  for (u32 i = 0; i < refobj->num_languages; i++)
    if (!S_CRefObjectLanguage(flow, &refobj->languages[i])) return 0;
  
  return 1;
}

int S_CIdObjInfo(usnd_flow *flow, struct CIdObjInfo *info) {
  char *classname;
  if (!S_string(flow, &classname))
    return 0;
  
  info->type = usnd_get_class(classname);
  if (!S_uuid(flow, &info->uuid)) return 0;
  if (!S_u32(flow, &info->offset)) return 0;
  if (!S_u32(flow, &info->size)) return 0;
  if (!S_string(flow, &info->filename)) return 0;
  if (!S_CRefObjectCont(flow, &info->refobj)) return 0;
  return 1;
}

int S_CRTTIClass(usnd_flow *flow, struct CRTTIClass *e) {
  char *classname = (char*)usnd_class_name(e->type);
  if (!S_string(flow, &classname))
    return 0;
  
  e->type = usnd_get_class(classname);
  return class[e->type].S(flow, e);
}

#pragma mark - Soundbank

/* Factor to account for extra alignment. */
#define USND_ALLOCATION_FACTOR 1.075f

usnd_size usnd_soundbank_loaded_size(usnd_size original_size) {
  usnd_size reserved_size = 0;
  reserved_size += sizeof(usnd_soundbank);
  reserved_size += USND_MAX_ENTRIES * sizeof(usnd_entry*);
  return (reserved_size + original_size) * USND_ALLOCATION_FACTOR;
}

usnd_soundbank *usnd_soundbank_load(usnd_arena *arena, u8 *data, usnd_size size) {
  usnd_flow flow = {};
  flow.mode = USND_READ;
  flow.buf = data;
  flow.size = size;
  flow.endianness = usnd_test_endian((const u32*)data, size);
  flow.arena = arena;
  
  usnd_arena_clear(arena);
  usnd_soundbank *bank = usnd_arena_push(arena, sizeof(usnd_soundbank));
  if (!bank)
    return NULL;
  
  bank->arena = arena;
  
  u32 index_offset = 0;
  u32 index_code = 0;
  S_u32(&flow, &index_offset);
  
  usnd_flow_seek(&flow, index_offset);
  S_u32(&flow, &index_code);
  S_u32(&flow, &bank->index_version);
  S_u32(&flow, &bank->num_entries);
  
  if (index_code != USND_FOURCC("INDX"))
    return NULL;
  
  usnd_size entry_list_size = USND_MAX_ENTRIES * sizeof(usnd_entry*);
  bank->entries = usnd_arena_push(arena, entry_list_size);
  if (!bank->entries)
    return NULL;
  
  for (u32 i = 0; i < bank->num_entries; i++) {
    struct CIdObjInfo index_entry = {};
    if (!S_CIdObjInfo(&flow, &index_entry))
      return NULL;
    
    usnd_offset index_next = flow.pos;
    usnd_flow_seek(&flow, index_entry.offset);
    
    usnd_entry *entry = usnd_arena_push(arena, sizeof(usnd_entry));
    if (!entry)
      return NULL;
    
    if (!S_CRTTIClass(&flow, entry))
      return NULL;
    
    bank->entries[i] = entry;
    usnd_flow_seek(&flow, index_next);
  }
  
  return bank;
}

usnd_entry *usnd_soundbank_find(usnd_soundbank *bank, usnd_uuid uuid) {
  
}

int usnd_soundbank_add_entry(usnd_soundbank *bank, usnd_entry *e) {
  
}

int usnd_soundbank_remove_entry(usnd_soundbank *bank, usnd_uuid uuid) {
  
}
