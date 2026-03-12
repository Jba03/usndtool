#ifndef usnd_h
#define usnd_h

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "resource.h"

typedef u32 usnd_entry_flags;
#define USND_ENTRY_FLAG_FIX     (1 << 0) /* in fixed memory */
#define USND_ENTRY_FLAG_LEVEL   (1 << 1) /* in level memory */
#define USND_ENTRY_FLAG_TRANSIT (1 << 2) /* in transition memory */
#define USND_ENTRY_FLAG_SHARED  (1 << 3) /* shared across multiple banks */

typedef struct CRTTIClass usnd_entry;
struct CRTTIClass {
  enum usnd_class type;
  usnd_entry_flags flags;
  /* CIdObj */
  struct {
    usnd_uuid uuid;
    union {
      struct CResData resource;
      struct CWaveFileIdObj wavefile;
    };
  };
};

const char *const usnd_class_name(enum usnd_class);
enum usnd_class usnd_get_class(const char *name);
enum usnd_class usnd_parent_class(enum usnd_class);
enum usnd_class usnd_general_class(enum usnd_class);
int usnd_instance_of(enum usnd_class child, enum usnd_class parent);

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
usnd_size usnd_soundbank_loaded_size(u8 *data, usnd_size data_size);
/* Load a soundbank into a pre-allocated arena. */
usnd_soundbank *usnd_soundbank_load(usnd_arena*, u8 *data, usnd_size data_size);

usnd_entry *usnd_soundbank_find(usnd_soundbank*, usnd_uuid);
int usnd_soundbank_add_entry(usnd_soundbank*, usnd_entry*);
int usnd_soundbank_remove_entry(usnd_soundbank*, usnd_uuid);


#ifdef __cplusplus
}
#endif

#endif /* usnd_h */
