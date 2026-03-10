#ifndef resource_h
#define resource_h

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

typedef u64 usnd_uuid;
#define USND_INVALID_UUID 0
#define USND_UUID_LOW(u) ((u) & 0xFFFFFFFF)
#define USND_UUID_HIGH(u) ((u) >> 32)
/* Get the group of a UUID, which is the high part
 * of the UUID's low part, e.g. 00000001xxxx00A3 */
#define USND_UUID_GROUP(u) (USND_UUID_LOW(u)>>16)
#define USND_UUID_MAX_GROUPS 0xFFFF

/* Hardcoded UUIDs for ThemeProgram and ThemeActor */
#define USND_THEMEPROGRAM_UUID 0x2ACA41B9C9138EA1
#define USND_THEMEACTOR_UUID   0x2ACB403911EC579D

/* An event the game calls to start,
 * stop or modify playback of sounds. */
struct CEventResData {
  usnd_event_type type;
  f32 coeff_a;
  f32 coeff_b;
  f32 coeff_c;
  f32 coeff_d;
  usnd_uuid link_uuid;
};

struct CRandomElement {
  f32 probability; /* probability of playing */
  usnd_uuid uuid;
};

struct CRandomResData {
  f32 volume; /* 0.0dB */
  f32 fail_probability; /* probability of not playing */
  u32 num_elements;
  struct CRandomElement *elements;
};

struct CSwitchElement {
  u32 index;
  usnd_uuid uuid;
};

struct CSwitchResData {
  f32 volume; /* 0.0dB */
  u32 type;
  u32 default_case;
  u32 num_elements;
  struct CSwitchElement *elements;
};

#pragma mark - Program

struct CProgramHeader {
  u32 header_size;
  u32 num_sections;
  u32 main_stacksize;
  u32 init_stacksize;
  u32 destructor_stacksize;
  u32 num_handlers;
  u32 num_local_functions;
  u32 program_var_size;
  /* constant vars: array of CHandlerDescriptor */
  u32 constant_vars_pos;
  u32 constant_vars_size;
  /* global destructor */
  u32 destructor_code_pos;
  u32 destructor_code_size;
  /* global constructor */
  u32 init_code_pos;
  u32 init_code_size;
  /* logical entry point */
  u32 main_code_pos;
  u32 main_code_size;
  u32 fn_descriptor_pos;
  u32 fn_descriptor_size;
  /* private code: handler functions */
  u32 private_code_pos;
  u32 private_code_size;
  u32 var_descriptor_pos;
  u32 var_descriptor_size;
  u32 pre_destructor_code_pos;
  u32 pre_destructor_code_size;
  
  u32 num_object_vars;
  u32 flags;
};

/* A program resource, used for music themes. */
struct CProgramResData {
  struct CProgramHeader *header;
  /* The bytecode of the program. It contains the following sections:
   *  .constructor, .destructor, .pre_destructor, .main, .private_code
   * The .private_code handlers are called by a special program instance
   * called the theme program, which manages how individual music themes
   * are mixed. */
  u8 *code;
  /* Referenced object links, not necessarily in playback order. */
  usnd_uuid *links;
  u32 num_links;
  u32 extra_flags;
};

/* Descriptor of a program handler section */
struct CHandlerDescriptor {
  u32 offset; /* Offset from start of program code */
  u32 size;
  u32 stacksize;
  u32 name_offset; /* Name offset in `constant_vars` */
};

/* Distance roll-off parameters for a sound source */
struct CRollOffParam {
  /* Distance at which the roll-off stops and volume stabilizes */
  f32 stabilization_distance;
  /* Distance at which the sound reaches full volume */
  f32 saturation_distance;
  /* Volume after the stabilization distance */
  f32 stabilization_volume;
};

/* Audio object instance of an actor in the game. */
struct CActorResData {
  u32 num_vars;
  u32 num_programs;
  /* Variables for the actor, such as synchronization markers.
   * Markers are set by wavefile cuepoints and read by the
   * game scripts to do lipsync or various other effects. */
  struct CSndVar *vars;
  /* List of programs allowed to modify the variables */
  usnd_uuid *programs;
  
  struct CRollOffParam rolloff;
  u32 var_access;
};

struct CWavResLink {
  usnd_language language;
  usnd_uuid uuid;
};

/* Resource which references sound effects or voice clips */
struct CWavResData {
  f32 coeff_a; /* 0.0f */
  f32 coeff_b; /* 0.5f */
  usnd_uuid default_link;
  struct CWavResLink *links;
  f32 coeff_c; /* 1.0f */
  u32 flags;
  u32 num_links;
};

#pragma mark -

/* Bank resource struct */
struct CResData {
  u32 version;
  char* name;
  union {
    struct CEventResData event;
    struct CRandomResData random;
    struct CProgramResData program;
    struct CSwitchResData _switch;
    struct CActorResData actor;
    struct CWavResData wav;
  };
};

#pragma mark - Bank index

struct CRefObjectLanguage {
  usnd_language language;
  usnd_uuid uuid;
  u32 unknown;
};

struct CRefObjectCont {
  u32 num_references;
  u32 num_languages;
  /* FIXME: probably not great to have this on the stack */
  usnd_uuid references[USND_MAX_LINKS];
  struct CRefObjectLanguage languages[USND_MAX_LANGUAGES];
};

struct CIdObjInfo {
  enum usnd_class type;
  usnd_uuid uuid;
  u32 offset;
  u32 size;
  char *filename;
  struct CRefObjectCont refobj;
};

#pragma mark - Object

typedef u8 CWaveFileIdObjFlags;
#define CWaveFileIdObjFlags_ExternalStream (1 << 0)

/* Wavefile container object */
struct CWaveFileIdObj {
  u32 version;
  f32 volume; /* 0.0dB */
  char *filename; /* Filename relative the bank filepath */
  u8* wavefile; /* Pointer to the waveformat data */
  CWaveFileIdObjFlags flags;
};

struct CSndVar {
  char *name;
  /* CSndVarType */
  struct {
    /* Type of the variable */
    usnd_var_type type;
    /* Base type of the variable, if type == USND_VAR_TYPE_BASETYPE */
    usnd_var_basetype basetype;
    /* Name of the type, if type == USND_VAR_TYPE_OBJECT */
    char *type_name;
  };
  
  union {
    u8 b8;
    char ch;
    s32 s32;
    f32 f32;
    char *string;
    usnd_uuid uuid;
    void *array;
    void *structure;
    void *object;
  };
};

#ifdef __cplusplus
}
#endif

#endif /* resource_h */
