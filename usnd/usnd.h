#ifndef usnd_h
#define usnd_h

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "resource.h"

typedef struct CRTTIClass usnd_entry;
struct CRTTIClass {
  enum usnd_class type;
  /* CIdObj */
  struct {
    usnd_uuid uuid;
    union {
      struct CResData resource;
      struct CWaveFileIdObj wavefile;
    };
  };
};

#pragma mark - Soundbank

typedef struct usnd_soundbank usnd_soundbank;
struct usnd_soundbank {
  u32 num_entries;
  usnd_entry **entries;
  u32 index_version;
  usnd_arena *arena;
};

/* Query the loaded size of a soundbank from its original size.
 * Returns the size to allocate for the arena to usnd_soundbank_load. */
usnd_size usnd_soundbank_loaded_size(usnd_size original_size);
/* Load a soundbank into a pre-allocated arena. */
usnd_soundbank *usnd_soundbank_load(usnd_arena*, u8 *data, usnd_size original_size);

usnd_entry *usnd_soundbank_find(usnd_soundbank*, usnd_uuid);
int usnd_soundbank_add_entry(usnd_soundbank*, usnd_entry*);
int usnd_soundbank_remove_entry(usnd_soundbank*, usnd_uuid);


#ifdef __cplusplus
}
#endif

#endif /* usnd_h */
