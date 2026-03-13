/* usndtool: Tool for editing 'HarmonX' soundbank files */
/* Created by Jba03 initially on 2024-07-02 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define CIMGUI_USE_SDL3
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimgui/cimgui_impl.h"

#if !defined(USNDTOOL_VERSION_STRING)
# define USNDTOOL_VERSION_STRING "unknown_version"
#endif

#include "usnd/usnd.h"
#include "usnd/audio.h"

typedef char String[USND_STRING_MAX];

#define PATH_MAX 1024
typedef char Path[PATH_MAX];

static bool Quit = false;
static SDL_Window *Window = NULL;
static SDL_Renderer *Renderer = NULL;
static struct Project *Project = NULL;

/* General options */
static Path Opt_ProjectsDir;
static bool Opt_DrawVisualizer = true;
static bool Opt_DrawSyncMarkers = true;
static bool Opt_DrawDebugger = false;
static bool Opt_DrawTimeline = false;
static bool Opt_CompactUUIDs = false;

/* Audio options */
static float Opt_AudioMasterGain = 1.0f;
static float Opt_AudioMasterPan = 0.0f;
static bool  Opt_AudioRepeat = false;
static bool  Opt_AudioAllowRandomness = true;
static u32   Opt_AudioDefaultLanguage = USND_LANGUAGE_ENGLISH;

#pragma mark - UUID

/* Default prefix for completely random UUIDs */
#define UUID_DEFAULT_PREFIX 0xA0
/* Default UUID group for new non-wavefile entries */
#define UUID_DEFAULT_GROUP 0x0A10

static void UUID_ToString(usnd_uuid uuid, String s) {
  SDL_snprintf(s, USND_STRING_MAX, "%016llX", uuid);
}

static bool UUID_FromString(const String s, usnd_uuid *uuid) {
  if (SDL_sscanf(s, "%016llX", uuid) != 1)
    return false;
  return true;
}

static usnd_uuid UUID_GetRandom(void) {
  u64 hi = (u64)SDL_rand_bits() << 32;
  u64 lo = (u64)SDL_rand_bits();
  return hi^lo;
}

#pragma mark - Common functions

static const char *const GetEventTypeName(usnd_event_type type) {
  if (type == USND_EVENT_PLAY) return "Play";
  if (type == USND_EVENT_STOP) return "Stop";
  if (type == USND_EVENT_PITCH) return "Pitch";
  if (type == USND_EVENT_VOLUME) return "Volume";
  if (type == USND_EVENT_EXTRA) return "Extra";
  if (type == USND_EVENT_STOPALL) return "StopAll";
  if (type == USND_EVENT_STOP_AND_GO) return "StopAndGo";
  return "UNKNOWN_EVENT";
}

static const char *const GetAudioFormatName(usnd_audio_format fmt) {
  if (fmt == USND_AUDIO_FORMAT_PCM) return "pcm-s16";
  if (fmt == USND_AUDIO_FORMAT_UBI) return "ubi-adpcm";
  if (fmt == USND_AUDIO_FORMAT_PSX) return "psx-adpcm";
  if (fmt == USND_AUDIO_FORMAT_DSP) return "dsp-adpcm";
  if (fmt == USND_AUDIO_FORMAT_IMA) return "ima-adpcm";
  if (fmt == USND_AUDIO_FORMAT_MP3) return "mp3";
  return "UNKNOWN_FORMAT";
}

static const char *const GetLanguageName(usnd_language lang) {
  if (lang == USND_LANGUAGE_GERMAN) return "German";
  if (lang == USND_LANGUAGE_ENGLISH) return "English";
  if (lang == USND_LANGUAGE_SPANISH) return "Spanish";
  if (lang == USND_LANGUAGE_FRENCH) return "French";
  if (lang == USND_LANGUAGE_ITALIAN) return "Italian";
  return "UNKNOWN_LANGUAGE";
}

static usnd_language GetLanguageCode(const String s) {
  if (!SDL_strcmp(s, "German")) return USND_LANGUAGE_GERMAN;
  if (!SDL_strcmp(s, "English")) return USND_LANGUAGE_ENGLISH;
  if (!SDL_strcmp(s, "Spanish")) return USND_LANGUAGE_SPANISH;
  if (!SDL_strcmp(s, "French")) return USND_LANGUAGE_FRENCH;
  if (!SDL_strcmp(s, "Italian")) return USND_LANGUAGE_ITALIAN;
  return USND_LANGUAGE_ENGLISH;
}

static const char *const GetVersionName(enum usnd_version version) {
  if (version == USND_VERSION_PC) return "PC";
  if (version == USND_VERSION_GC) return "GC";
  if (version == USND_VERSION_PS2) return "PS2";
  if (version == USND_VERSION_PS3) return "PS3";
  if (version == USND_VERSION_XBOX) return "XBOX";
  if (version == USND_VERSION_XBOX360) return "XBOX 360";
  return "UNKNOWN_VERSION";
}


static void GetEntryUUID(const usnd_entry *obj, char *string) {
  const u32 lo = USND_UUID_LOW(obj->uuid);
  const u32 hi = USND_UUID_HIGH(obj->uuid);
  const u16 group = USND_UUID_GROUP(obj->uuid);
  if (Opt_CompactUUIDs)
    UUID_ToString(obj->uuid, string);
  else
    SDL_snprintf(string, USND_STRING_MAX, "%08X-%04X-%04X", hi, group, lo & 0xFFFF);
}

static void GetEntryName(const usnd_entry *obj, char *name) {
  if (usnd_instance_of(obj->type, CResData) && obj->resource.name)
    SDL_strlcpy(name, obj->resource.name, USND_STRING_MAX);
  else
    GetEntryUUID(obj, name);
}

static int GetEntryVersion(const usnd_entry *entry) {
  if (usnd_instance_of(entry->type, CResData))
    return entry->resource.version;
  if (usnd_instance_of(entry->type, CWaveFileIdObj))
    return entry->wavefile.version;
  return -1;
}

static bool IsPlayable(const usnd_entry *e) {
  if (e->type == CActorResData) return false;
  if (e->uuid == USND_THEMEPROGRAM_UUID) return false;
  return true;
}

#pragma mark - Path

#if !defined(WIN32)
# define PATH_DELIMITER '/'
#else
# define PATH_DELIMITER '\\'
#endif

static void Path_Copy(Path dst, const Path src) {
  SDL_strlcpy(dst, src, PATH_MAX);
}

static void Path_Append(Path dst, const Path src) {
  Path tmp = {};
  Path_Copy(tmp, dst);
  SDL_snprintf(dst, PATH_MAX, "%s%c%s", tmp, PATH_DELIMITER, src);
}

static bool Path_GetExtension(const Path path, Path ext) {
  const char *p = SDL_strrchr(path, '.');
  if (p == NULL)
    return false;
  SDL_strlcpy(ext, p, PATH_MAX);
  return true;
}

static bool Path_RemoveExtension(Path path) {
  char *p = SDL_strrchr(path, '.');
  if (!p) return false;
  *p = '\0';
  return true;
}

static bool Path_GetFilename(const Path path, String name) {
  const char *p0 = SDL_strrchr(path, PATH_DELIMITER);
  const char *p1 = SDL_strrchr(path, '.');
  if (!p0 || !p1)
    return false;
  SDL_strlcpy(name, p0+1, SDL_max(p1-p0-1, USND_STRING_MAX-1));
  return true;
}

static void Path_RemoveFilename(Path path) {
  char* p = SDL_strrchr(path, PATH_DELIMITER);
  if (p && SDL_strlen(p) > 1) *(p+1) = '\0';
}

static bool Path_GetParentPath(const Path path, Path out) {
  SDL_strlcpy(out, path, PATH_MAX);
  Path_RemoveFilename(out);
  char* p = SDL_strrchr(out, PATH_DELIMITER);
  if (!p) return false;
  *(p+1) = '\0';
  return true;
}

static void Path_FixUp(Path path) {
  for (u32 i = 0; i < PATH_MAX; i++) {
    if (path[i] == '\0') break;
    if (path[i] == '\\') path[i] = PATH_DELIMITER;
  }
}

static void Path_ShowInFileExplorer(const Path path) {
  Path folder_url = {};
  SDL_snprintf(folder_url, PATH_MAX, "file://%s", path);
  SDL_OpenURL(folder_url);
}

#pragma mark - File utils

enum FileType {
  FILETYPE_UNKNOWN,
  FILETYPE_SOUNDBANK,
  FILETYPE_HST,
  FILETYPE_HOS,
  FILETYPE_WAV,
};

static enum FileType GetFileType(const Path path) {
  String ext;
  if (!Path_GetExtension(path, ext))
    return false;
  
  SDL_strlwr(ext);
  if (SDL_strcmp(ext, ".hxd") == 0) return FILETYPE_SOUNDBANK;
  if (SDL_strcmp(ext, ".hxc") == 0) return FILETYPE_SOUNDBANK;
  if (SDL_strcmp(ext, ".hxg") == 0) return FILETYPE_SOUNDBANK;
  if (SDL_strcmp(ext, ".hx2") == 0) return FILETYPE_SOUNDBANK;
  if (SDL_strcmp(ext, ".hx3") == 0) return FILETYPE_SOUNDBANK;
  if (SDL_strcmp(ext, ".hxx") == 0) return FILETYPE_SOUNDBANK;
  if (SDL_strcmp(ext, ".hst") == 0) return FILETYPE_HST;
  if (SDL_strcmp(ext, ".hos") == 0) return FILETYPE_HOS;
  if (SDL_strcmp(ext, ".wav") == 0) return FILETYPE_WAV;
  if (SDL_strcmp(ext, ".wave") == 0) return FILETYPE_WAV;
  return FILETYPE_UNKNOWN;
}

#pragma mark - Soundbank -

typedef struct SoundBank SoundBank;
struct SoundBank {
  usnd_arena arena;
  usnd_soundbank *handle;
  Path relative_path;
  Path absolute_path;
  String filename;
};

static SoundBank *SoundBank_Alloc(void) {
  return SDL_calloc(1, sizeof(SoundBank));
}

static usnd_entry *SoundBank_Find(SoundBank *bank, usnd_uuid uuid) {
  return usnd_soundbank_find(bank->handle, uuid);
}

static SoundBank* SoundBank_Load(const Path filepath) {
  size_t size = 0;
  void *data = NULL;
  SoundBank *bank = NULL;
  
  if (!(data = SDL_LoadFile(filepath, &size))) {
    SDL_Log("Failed to load file %s: %s\n", filepath, SDL_GetError());
    goto fail;
  }
  
  if (!(bank = SoundBank_Alloc())) {
    SDL_Log("SDL_calloc failed: %s\n", SDL_GetError());
    goto fail;
  }
  
  usnd_size original_size = (usnd_size)(size);
  usnd_size required_size = usnd_soundbank_loaded_size(data, original_size);
  bank->arena = usnd_arena_alloc(SDL_malloc(required_size), required_size, 0);
  
  if (!(bank->handle = usnd_soundbank_load(&bank->arena, data, original_size))) {
    SDL_Log("Failed to read bank %s\n", filepath);
    goto fail;
  }
  
  SDL_free(data);
  
  Path_Copy(bank->absolute_path, filepath);
  Path_GetFilename(filepath, bank->filename);
  
  printf("%s (%u -> %u, used = %u)\n", bank->filename, original_size, required_size, bank->arena.position);
  
  return bank;
  
fail:
  if (data) SDL_free(data);
  if (bank) SDL_free(bank);
  if (bank->arena.base) SDL_free(bank->arena.base);
  return NULL;
}

static void SoundBank_Destroy(SoundBank *bank) {
  SDL_free(bank->arena.base);
  SDL_free(bank);
}

#pragma mark - Project -

#define PROJECT_MAX_BANKS 100
#define PROJECT_MAX_ENTRIES \
  (PROJECT_MAX_BANKS * USND_MAX_ENTRIES)

struct ProjectEntry {
  SoundBank *bank;
  u32 index;
};

struct Project {
  String name;
  Path origin_path;
  Path current_path;
  
  struct ProjectEntry entries[PROJECT_MAX_ENTRIES];
  struct SoundBank *banks[PROJECT_MAX_BANKS];
  u32 num_entries;
  u32 num_banks;
};

static struct Project *Project_Alloc(void) {
  struct Project *project = SDL_calloc(1, sizeof(*project));
  return project;
}

inline static usnd_entry *Project_GetEntry(struct Project *p, u32 idx) {
  if (idx >= p->num_entries) return NULL;
  SoundBank *bank = p->entries[idx].bank;
  u32 index = p->entries[idx].index;
  usnd_soundbank *h = bank->handle;
  return h->entries[index];
}

static int Project_BinarySearch(struct Project *p, usnd_uuid uuid) {
  int lo = 0, hi = p->num_entries - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    usnd_entry *entry = Project_GetEntry(p, mid);
    SDL_assert(entry);
    if (entry->uuid == uuid)
      return mid;
    if (entry->uuid < uuid) lo = mid + 1;
    else hi = mid - 1;
  }
  return ~lo;
}

static int Project_PutEntry(struct Project *p, SoundBank *bank, u32 entry_index) {
  usnd_soundbank *h = bank->handle;
  if (entry_index >= h->num_entries || p->num_entries + 1 >= PROJECT_MAX_ENTRIES)
    return -1;
  
  usnd_uuid uuid = h->entries[entry_index]->uuid;
  int position = Project_BinarySearch(p, uuid);
  if (position >= 0) {
    /* replace the current reference */
    p->entries[position].bank = bank;
    p->entries[position].index = entry_index;
    return 1;
  }
  
  /* get position to insert the entry at */
  position = ~position;
  memmove(&p->entries[position + 1], &p->entries[position],
    (p->num_entries - (u32)position) * sizeof(struct ProjectEntry));
  
  p->entries[position].bank = bank;
  p->entries[position].index = entry_index;
  p->num_entries++;
  return 0;
}

static usnd_entry* Project_Find(struct Project *p, usnd_uuid uuid) {
  int idx = Project_BinarySearch(p, uuid);
  if (idx >= 0) return Project_GetEntry(p, idx);
  return NULL;
}

#define Project_ForEach(p, entry) \
  for (u32 _i = 0; (p) && _i < (p)->num_entries && \
  (entry = ((p)->entries[_i].bank->handle->entries[(p)->entries[_i].index])); _i++)

static int Project_AddBank(struct Project *p, SoundBank *bank) {
  if (p->num_banks + 1 >= PROJECT_MAX_BANKS) {
    SDL_Log("Failed to add bank to project: maximum number of banks reached");
    return 0;
  }
  
  p->banks[p->num_banks++] = bank;
  for (u32 i = 0; i < bank->handle->num_entries; i++) {
    usnd_entry *entry = bank->handle->entries[i];
    /* If the entry already exists in the project,
     * mark it as shared across multiple banks. */
    usnd_entry *found = Project_Find(p, entry->uuid);
    if (found) {
      entry->flags |= USND_ENTRY_FLAG_SHARED;
      found->flags |= USND_ENTRY_FLAG_SHARED;
    }
    
    Project_PutEntry(p, bank, i);
  }
  
  return 1;
}

static struct Project* Project_LoadFromSoundFolder(const Path dir) {
  int num_files = 0;
  char** files = SDL_GlobDirectory(dir, NULL, SDL_GLOB_CASEINSENSITIVE, &num_files);
  if (!files || num_files == 0)
    return NULL;
    
  struct Project *project = Project_Alloc();
  if (!project)
    return NULL;
  
  Path_Copy(project->origin_path, dir);
  
  for (u32 i = 0; i < num_files; i++) {
    Path relative_path = {};
    SDL_strlcpy(relative_path, files[i], PATH_MAX);
    
    enum FileType filetype = GetFileType(relative_path);
    if (filetype != FILETYPE_SOUNDBANK)
      continue;
    
    Path absolute_path = {};
    SDL_snprintf(absolute_path, PATH_MAX, "%s/%s", dir, relative_path);
    
    SoundBank *bank = SoundBank_Load(absolute_path);
    if (!bank)
      continue;
    
    Path_Copy(bank->relative_path, relative_path);
    Project_AddBank(project, bank);
  }
  
  SDL_free(files);
  SDL_strlcpy(project->name, "DefaultProject", USND_STRING_MAX);
  
  return project;
}

#pragma mark - Project search

#define SEARCH_MAX_RESULTS 20000

enum SearchMode {
  SEARCH_MODE_ALL_ENTRIES,
  SEARCH_MODE_SHARED_ENTRIES,
  SEARCH_MODE_UNIQUE_ENTRIES,
};

typedef u16 SearchGroup;
#define SEARCH_GROUP_ANY (u16)(-1)

struct SearchContext {
  enum SearchMode mode;
  u64 class_flags;
  SoundBank *bank;
  SearchGroup group;
  String query;
  usnd_entry *results[SEARCH_MAX_RESULTS];
  u32 num_results;
};

SDL_COMPILE_TIME_ASSERT(MaxClassFlags, USND_CLASS_MAX < 64);

static bool SearchFilter(struct SearchContext *s, const usnd_entry *e) {
  for (enum usnd_class c = 0; c < USND_CLASS_MAX; c++) {
    if ((s->class_flags & (1 << c)) && usnd_instance_of(e->type, c)) {
      /* Filter by bank */
      if (s->bank && !SoundBank_Find(s->bank, e->uuid)) return false;
      /* Filter by group */
      if (s->group != SEARCH_GROUP_ANY && USND_UUID_GROUP(e->uuid) != s->group) return false;
      /* Filter by uniqueness */
      bool is_shared = (e->flags & USND_ENTRY_FLAG_SHARED);
      if (s->mode == SEARCH_MODE_SHARED_ENTRIES && !is_shared) return false;
      if (s->mode == SEARCH_MODE_UNIQUE_ENTRIES && is_shared) return false;
      return true;
    }
  }
  return false;
}

static bool SearchString(const String string, const String query) {
  size_t len = SDL_strlen(query);
  for (u32 n = 0; n < SDL_strlen(string); n++)
    if (!SDL_strncmp(string+n, query, len)) return true;
  return false;
}

void SearchContext_Clear(struct SearchContext *s) {
  SDL_memset(s, 0, sizeof(*s));
  s->mode = SEARCH_MODE_ALL_ENTRIES;
  s->group = SEARCH_GROUP_ANY;
  s->class_flags = 1 << CRTTIClass;
}

static bool SearchContext_DoSearch(struct SearchContext *s) {
  String query = {};
  SDL_strlcpy(query, s->query, USND_STRING_MAX);
  SDL_strlwr(query);
  
  usnd_entry *entry = NULL;
  s->num_results = 0;
  
  Project_ForEach(Project, entry) {
    if (SearchFilter(s, entry)) {
      /* Test name + default and split uuid formats */
      String name = {}, uuid = {}, split_uuid = {};
      GetEntryName(entry, name);
      GetEntryUUID(entry, split_uuid);
      UUID_ToString(entry->uuid, uuid);
      
      SDL_strlwr(name);
      SDL_strlwr(uuid);
      SDL_strlwr(split_uuid);
      
      if (s->num_results+1 >= SEARCH_MAX_RESULTS)
        return false;
      
      if (SearchString(name, query))
        s->results[s->num_results++] = entry;
      else if (SearchString(uuid, query))
        s->results[s->num_results++] = entry;
      else if (SearchString(split_uuid, query))
        s->results[s->num_results++] = entry;
    }
  }
  
  return true;
}

#pragma mark -

static bool GetBanksForEntry(usnd_uuid uuid, SoundBank *banks[PROJECT_MAX_BANKS], u32 *count) {
  for (u32 i = 0; i < Project->num_banks; i++)
    if (SoundBank_Find(Project->banks[i], uuid))
      banks[(*count)++] = Project->banks[i];
  return true;
}


#pragma mark - UI -

#define ImVec2(x,y)       (ImVec2){x,y}
#define ImVec4(x,y,z,w)   (ImVec4){x,y,z,w}
#define ImColor(x,y,z,w)  (ImVec4){x/255.0f,y/255.0f,z/255.0f,w/255.0f}

#define UI_MAX_SELECTIONS 32

static usnd_entry *UI_Selection[UI_MAX_SELECTIONS];
static usnd_entry *UI_ContextMenuTarget;
static usnd_entry *UI_PopupTarget;

static struct SearchContext UI_FixedSearch;
static struct SearchContext UI_PopupSearch;

static ImGuiID UI_MainDockspaceID;
static bool UI_WantsLayout = true;

/* Global shortcuts */
static const ImGuiKeyChord UI_UndoKeyChord = ImGuiMod_Ctrl | ImGuiKey_Z;
static const ImGuiKeyChord UI_RedoKeyChord = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z;
static const ImGuiKeyChord UI_AudioPauseKeyChord = ImGuiKey_Space;
static const ImGuiKeyChord UI_QuickSearchKeyChord = ImGuiMod_Ctrl | ImGuiKey_F;
static const ImGuiKeyChord UI_QueueResetKeyChord = ImGuiMod_Ctrl | ImGuiKey_R;

#pragma mark - UI common

static usnd_entry *UI_GetPrimarySelection(void) {
  u32 n = UI_MAX_SELECTIONS;
  while (--n)
  if (UI_Selection[n])
    return UI_Selection[n];
  return *UI_Selection;
}

static ImVec4 UI_GetEntryColor(const usnd_entry *e) {
  switch (usnd_general_class(e->type)) {
    case CEventResData: {
      /* Visually, this looks good enough. Probably preferable */
      /* to choose similar colors based on the uuid group though. */
      const struct CResData *res = &e->resource;
      if (res->name && SDL_strlen(res->name) > 7) {
        float r = 4.0f * (res->name[5] / 255.0f);
        float b = 3.0f * (res->name[7] / 255.0f);
        return ImVec4(0.5f, 0.5f*r, 1.0f*b, 1.0f);
      }
      break;
    }
    
    case CRandomResData:
      return ImVec4(0.4f, 0.7f, 0.8f, 1.0f);

    case CProgramResData:
      if (e->uuid == USND_THEMEPROGRAM_UUID)
        return ImVec4(0.66f, 0.4f, 1.0f, 1.0f);
      else return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    case CWavResData:
      return ImVec4(0.7f, 0.8f, 1.0f, 0.9f);

    case CWaveFileIdObj:
      return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

    case CActorResData:
      return ImVec4(0.5f, 0.35f, 1.0f, 1.0f);

    default:
      break;
  }
  
  return ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
}

#pragma mark - UI Elements

enum UI_ButtonType {
  UI_ButtonType_Play,
  UI_ButtonType_Pause,
  UI_ButtonType_Stop,
  UI_ButtonType_Modify
};

static bool UI_DrawButtonBase(ImVec2 *size, ImVec4 color, ImVec4 bg, bool arrow) {
  if (size->x == 0 && size->y == 0) *size = ImVec2(15, 15);
  igPushStyleColor_Vec4(ImGuiCol_Text, arrow ? color : ImVec4(0,0,0,0));
  igPushStyleColor_Vec4(ImGuiCol_Button, bg);
  bool pressed = igArrowButtonEx("##", ImGuiDir_Right, *size, 0);
  igPopStyleColor(2);
  if (igIsItemHovered(ImGuiHoveredFlags_None))
    igSetMouseCursor(ImGuiMouseCursor_Hand);
  return pressed;
}

static bool UI_DrawPlayButton(ImVec2 size) {
  ImVec4 color = ImVec4(0.25f, 1.0f, 0.43f, 0.75f);
  ImVec4 bg_color = ImVec4(color.x, color.y, color.z, color.w * 0.2f);
  return UI_DrawButtonBase(&size, color, bg_color, true);
}

static bool UI_DrawPauseButton(ImVec2 size) {
  ImVec4 color = ImVec4(1.0f, 0.75f, 0.25f, 0.75f);
  ImVec4 bg_color = ImVec4(color.x, color.y, color.z, color.w * 0.2f);
  ImVec2 cursor = igGetCursorScreenPos();
  bool pressed = UI_DrawButtonBase(&size, color, bg_color, false);
  
  ImDrawList *drawlist = igGetWindowDrawList();
  ImVec2 p0 = ImVec2(cursor.x + size.x/4.0f + 1.0f, cursor.y + size.y/4.0f);
  ImVec2 p1 = ImVec2(p0.x, p0.y + size.y/2.0f);
  ImDrawList_AddLine(drawlist, p0, p1, igGetColorU32_Vec4(color), 2);
  p0.x = cursor.x + size.x - size.x / 3.0-1;
  p1.x = cursor.x + size.x - size.x / 3.0-1;
  ImDrawList_AddLine(drawlist, p0, p1, igGetColorU32_Vec4(color), 2);
  return pressed;
}

static bool UI_DrawStopButton(ImVec2 size, bool active) {
  ImVec4 color = active ? ImVec4(0.0f, 0.75f, 1.0f, 0.75f) : ImVec4(0.25f, 0.5f, 1.0f, 0.75f);
  ImVec4 bg_color = ImVec4(color.x, color.y, color.z, color.w * 0.2f);
  ImVec2 cursor = igGetCursorScreenPos();
  bool pressed = UI_DrawButtonBase(&size, color, bg_color, false);
  
  ImDrawList *drawlist = igGetWindowDrawList();
  ImVec2 p0 = ImVec2(cursor.x + size.x / 4.0f, cursor.y + size.y / 4.0f);
  ImVec2 p1 = ImVec2(p0.x + size.x/2.0f, p0.y + size.y/2.0f);
  
  ImU32 col = igGetColorU32_Vec4(color);
  if (active)
    ImDrawList_AddRectFilled(drawlist, p0, p1, col, 0.0f, 0);
  else
    ImDrawList_AddRect(drawlist, p0, p1, col, 1.0f, 0, 1.0f);
  
  if (igIsItemHovered(ImGuiHoveredFlags_None))
    igSetMouseCursor(ImGuiMouseCursor_Hand);
  
  return pressed;
}

static bool UI_DrawButton(enum UI_ButtonType type, ImVec2 size, bool active) {
  if (type == UI_ButtonType_Play)
    return UI_DrawPlayButton(size);
  else if (type == UI_ButtonType_Pause)
    return UI_DrawPauseButton(size);
  else if (type == UI_ButtonType_Stop)
    return UI_DrawStopButton(size, active);
  else if (type == UI_ButtonType_Modify)
    return UI_DrawStopButton(size, active);
  SDL_assert(false);
}

static bool UI_ObjectSelector(const char *label,
  const enum usnd_class *allow, u32 count, usnd_uuid *uuid)
{
  String name = {};
  UUID_ToString(*uuid, name);
  usnd_entry *entry = Project_Find(Project, *uuid);
  if (entry)
    GetEntryName(entry, name);
  
  ImVec4 bg_color = ImVec4(0.3f, 0.5f, 1.0f, 0.35f);
  ImVec4 text_color = ImVec4(0.8f, 0.9f, 1.0f, 1.0f);
  
  if (*uuid == USND_INVALID_UUID) bg_color.w *= 0.5f;
  if (*uuid == USND_INVALID_UUID) text_color.w *= 0.5f;
  
  igPushStyleColor_Vec4(ImGuiCol_Button, bg_color);
  igPushStyleColor_Vec4(ImGuiCol_Text, text_color);
  bool b = igButton(name, ImVec2(0.0f, 0.0f));
  igPopStyleColor(2);
  
  if (igBeginDragDropTarget()) {
    for (u32 i = 0; i < count; i++) {
      const char *name = usnd_class_name(allow[i]);
      const ImGuiPayload *payload = igAcceptDragDropPayload(name, ImGuiDragDropFlags_None);
      if (payload && payload->Data) {
        SDL_Log("Accepted drag drop element (%s)\n", name);
        usnd_uuid *d = payload->Data;
        if (*d != *uuid) {
          *uuid = *d;
          return true;
        }
      }
    }
    igEndDragDropTarget();
  }
  
  if (igIsItemHovered(ImGuiHoveredFlags_None))
    igSetMouseCursor(ImGuiMouseCursor_Hand);
  
  if (igBeginItemTooltip()) {
    igText("Accepts:");
    for (u32 i = 0; i < count; i++)
      igTextColored(ImVec4(0.6f, 0.75f, 1.0f, 1.0f), " %s", usnd_class_name(allow[i]));
    igText("Drag & drop/click to select");
    igEndTooltip();
  }
  
  return b;
}

static ImGuiID AdapterIndexToStorageId(ImGuiSelectionBasicStorage *s, int idx) {
  usnd_entry **selection = s->UserData;
  if (!selection[idx]) return 0;
  return igImHashData(&selection[idx]->uuid, sizeof(usnd_uuid), 0);
}

static bool UI_DrawEntryTable(struct SearchContext *s, usnd_entry **selection) {
  igPushStyleVar_Vec2(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  
  igPushStyleVar_Float(ImGuiStyleVar_FrameRounding, 0.0f);
  igSetNextItemWidth(-0.000001f); /* don't ask */
  if (igInputTextWithHint("##Search", "Search entry...", s->query,
    USND_STRING_MAX, ImGuiInputTextFlags_None, NULL, NULL))
  {
    SearchContext_DoSearch(s);
  }
  igPopStyleVar(1);
  
  bool selected = false;
  if (*selection == NULL && s->results[0])
    *selection = s->results[0];
  
  static ImGuiSelectionBasicStorage selection_storage = {};
  selection_storage.UserData = s->results;
  selection_storage.AdapterIndexToStorageId = AdapterIndexToStorageId;
  
  igPushStyleVar_Vec2(ImGuiStyleVar_ItemInnerSpacing, ImVec2(3.0f, 0.0f));
  ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
  if (igBeginTable("##EntryTabl", 2, table_flags, ImVec2(0.0f, 0.0f), 1.0f))
  {
    ImGuiMultiSelectFlags ms_flags = 0;
    ms_flags |= ImGuiMultiSelectFlags_SelectOnClickRelease;
    ms_flags |= ImGuiMultiSelectFlags_NoSelectOnRightClick;
    ms_flags |= ImGuiMultiSelectFlags_ClearOnClickVoid;
    
    ImGuiMultiSelectIO *ms_io = igBeginMultiSelect(ms_flags, selection_storage.Size, s->num_results);
    ImGuiSelectionBasicStorage_ApplyRequests(&selection_storage, ms_io);
    igTableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, -1, 0);
    igTableSetupColumn("##Name", ImGuiTableColumnFlags_WidthStretch, -1, 0);
    
    ImGuiListClipper clipper = {};
    ImGuiListClipper_Begin(&clipper, s->num_results, 17.0f);
    while (ImGuiListClipper_Step(&clipper))
    {
      for (u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
      {
        usnd_entry *e = s->results[i];
        String name = {};
        GetEntryName(e, name);
        
        igTableNextRow(ImGuiTableRowFlags_None, 0.0f);
        igTableNextColumn();
        igSetCursorPosX(5.0f);
        
        igPushID_Ptr(e);
        if (IsPlayable(e)) {
          enum UI_ButtonType type = UI_ButtonType_Play;
          if (e->type == CEventResData) {
            if (e->resource.event.type == USND_EVENT_STOP) {
              usnd_entry *entry = NULL;
              type = UI_ButtonType_Stop;
            } else if (e->resource.event.type != USND_EVENT_PLAY)
              type = UI_ButtonType_Modify;
          }
          
          UI_DrawButton(type, ImVec2(0,0), false);
          
        } else {
          igDummy(ImVec2(0.0f, 0.0f));
        }
        igPopID();
        
        
        igTableNextColumn();
        
        ImVec4 color = UI_GetEntryColor(e);
//        if (e == *selection) {
        
        bool is_selected = ImGuiSelectionBasicStorage_Contains(
          &selection_storage, igImHashData(&e->uuid, sizeof(usnd_uuid), 0));
        is_selected |= (e == *selection);
        
        if (is_selected) {
          color = ImVec4(1.0f, 0.8f, 0.1f, 1.0f);
          ImVec4 bg = color;
          bg.w = 0.1f;
          igTableSetBgColor(ImGuiTableBgTarget_CellBg, igColorConvertFloat4ToU32(bg), -1);
        }
        
        igPushStyleColor_Vec4(ImGuiCol_Text, color);
        
        /* Entry */
        igPushID_Ptr(e);
        igSetNextItemSelectionUserData(i);
        if (igSelectable_Bool(name, false, ImGuiSelectableFlags_SelectOnNav, ImVec2(0.0f, 0.0f))) {
          selected = true;
        }
        igPopID();
        
        /* object info select  */
        if (igIsItemHovered(0)) {
          igSetMouseCursor(ImGuiMouseCursor_Hand);
        }
        
        if (igIsItemClicked(ImGuiMouseButton_Left)) {
          *selection = e;
        }
        
        if (igIsItemClicked(ImGuiMouseButton_Right)) {
          UI_ContextMenuTarget = e;
        }
        
        /* drag-drop source */
//        if (igBeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
//          String name = {};
//          GetEntryName(e, name);
//          const char *class_name = usnd_class_name(usnd_general_class(e->type));
//          igSetDragDropPayload(class_name, &e->uuid, sizeof(usnd_uuid*), ImGuiCond_Always);
//          
//          igButton(name, ImVec2(0.0f, 0.0f));
//          igEndDragDropSource();
//        }
        
        if (igBeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          usnd_uuid payload[UI_MAX_SELECTIONS];
          SDL_memset(payload, 0, sizeof(payload));
          u32 count = 0;

          if (is_selected && selection_storage.Size > 1) {
            for (int n = 0; n < selection_storage.Size; n++) {
              usnd_entry **entries = selection_storage.UserData;
//              ImGuiID id = USND_UUID_LOW(entries[n]->uuid);
              payload[count++] = entries[n]->uuid;
            }
          } else {
            /* Drag only the clicked item */
            payload[count++] = e->uuid;
          }
          
          igSetDragDropPayload("$entries", payload,
            sizeof(usnd_uuid) * count, ImGuiCond_Always);
          
          if (count == 1) {
            String name = {};
            GetEntryName(Project_Find(Project, payload[0]), name);
            igText(name);
          } else {
            igText("%d entries", count);
          }
            
          igEndDragDropSource();
        }
        
        igSameLine(0.0f, 0.0f);
      
        
        if (e->flags & USND_ENTRY_FLAG_SHARED) {
          igBullet();
        }
        
        igPopStyleColor(1);
      }
    }
   
    ms_io = igEndMultiSelect();
    ImGuiSelectionBasicStorage_ApplyRequests(&selection_storage, ms_io);
    
    igEndTable();
  }
  
//  igEndTable();
  igPopStyleVar(2);
  
  return selected;
}

static void UI_ObjectTree(usnd_entry *e, u32 depth, String info) {
  ImGuiTableFlags flags = 0;
  flags |= ImGuiTreeNodeFlags_DefaultOpen;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_SpanAllColumns;
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;

  igPushID_Ptr(e);
  igTableNextColumn();
  igSetCursorPosX(igGetCursorPosX() + depth * 5.0f);
  

  
  u32 num_next = 0;
  usnd_entry *next[USND_MAX_LINKS] = {};
  String info_next[USND_MAX_LINKS] = {};
  
  String name = {};
  GetEntryName(e, name);
  
  ImVec4 color = UI_GetEntryColor(e);
  if (e == UI_GetPrimarySelection())
    color = ImVec4(1.0f, 0.75f, 0.3f, 1.0f);
  igPushStyleColor_Vec4(ImGuiCol_Text, color);
  
  const struct CWavResData *wav = &e->resource.wav;
  const struct CEventResData *evt = &e->resource.event;
  const struct CRandomResData *random = &e->resource.random;
  const struct CProgramResData *program = &e->resource.program;
  const struct CSwitchResData *_switch = &e->resource._switch;
  
  if (igTreeNodeEx_Str(name, flags)) {
    switch (usnd_general_class(e->type)) {
      case CEventResData:
        next[num_next] = Project_Find(Project, evt->link_uuid);
        SDL_strlcpy(info_next[num_next++], "--", USND_STRING_MAX);
        break;
        
      case CRandomResData:
        SDL_snprintf(info, USND_STRING_MAX, "f=%.3f", random->fail_probability);
        for (u32 i = 0; i < random->num_elements; i++) {
          next[num_next] = Project_Find(Project, random->elements[i].uuid);
          SDL_snprintf(info_next[num_next++], USND_STRING_MAX, "p=%.3f", random->elements[i].probability);
        }
        break;
      
      case CProgramResData:
        for (u32 i = 0; i < program->num_links; i++)
          next[num_next++] = Project_Find(Project, program->links[i]);
        break;
        
      case CSwitchResData:
        for (u32 i = 0; i < _switch->num_elements; i++) {
          next[num_next] = Project_Find(Project, _switch->elements[i].uuid);
          SDL_snprintf(info_next[num_next++], USND_STRING_MAX, "%d", _switch->elements[i].index);
        }
        break;
        
      case CWavResData:
        if ((next[num_next] = Project_Find(Project, wav->default_link)))
          SDL_strlcpy(info_next[num_next++], "Default", USND_STRING_MAX);
        for (u32 i = 0; i < wav->num_links; i++) {
          next[num_next] = Project_Find(Project, wav->links[i].uuid);
          SDL_strlcpy(info_next[num_next++], GetLanguageName(wav->links[i].language), USND_STRING_MAX);
        }
        break;
        
      default:
        break;
    }
    
    igTreePop();
  }
  
  
  igPopStyleColor(1);
  
  if (igIsItemHovered(ImGuiHoveredFlags_None))
    igSetMouseCursor(ImGuiMouseCursor_Hand);
  
  if (igIsItemClicked(ImGuiMouseButton_Left)) {
    UI_Selection[1] = e;
    if (igIsMouseDoubleClicked_Nil(ImGuiMouseButton_Left)) {
      /* play object */
    }
  }
  
  if (igIsItemClicked(ImGuiMouseButton_Right))
    UI_ContextMenuTarget = e;
  
  /* class name */
  igTableNextColumn();
  igTextDisabled("%s", usnd_class_name(e->type));
  
  /* info */
  igTableNextColumn();
  igTextDisabled("%s", info);
  
  for (u32 i = 0; i < num_next; i++) {
    igTableNextRow(ImGuiTableRowFlags_None, 0);
    if (next[i])
      UI_ObjectTree(next[i], depth + 1, info_next[i]);
  }
  
  igPopID();
}


#pragma mark - Entry editing

#define UI_ARENA_MAX_SIZE 64000
/* Arena for temporary edits */
static usnd_arena UI_Arena;

static char *UI_DupString(char *p, u32 max_size) {
  void *data = usnd_arena_push(&UI_Arena, max_size);
  if (p) {
    u32 len = (u32)SDL_strlen(p);
    SDL_memcpy(data, p, len + 1);
  }
  return data;
}

static void UI_EditEventResData(usnd_entry *e) {
  struct CEventResData *evt = &e->resource.event;
  
  if (igBeginCombo("Type", GetEventTypeName(evt->type), ImGuiComboFlags_None)) {
    for (usnd_event_type type = 0; type <= USND_EVENT_STOP_AND_GO; type++) {
      const char *selection = GetEventTypeName(type);
      if (igSelectable_Bool(selection, evt->type == type, 0, ImVec2(0.0f, 0.0f))) {
        evt->type = type;
      }
    }
    igEndCombo();
  }
  
  /* Name */
  char *name = UI_DupString(e->resource.name, USND_STRING_MAX);
  igInputTextEx("Name", "No name", name, USND_STRING_MAX,
    ImVec2(0.0f, 0.0f), ImGuiInputTextFlags_CharsNoBlank, NULL, NULL);
  if (igIsItemDeactivatedAfterEdit()) {
    e->resource.name = SDL_strdup(name);
  }
  
  /* Coefficients */
  igInputFloat4("Coeff", &evt->coeff_a, "%.3f", 0);
  if (igIsItemDeactivatedAfterEdit()) {
    if (evt->coeff_a < 0.0f) evt->coeff_a = 0.0f;
  }
  
  const enum usnd_class allow[] = {
    CWavResData,
    CProgramResData,
    CRandomResData,
    CRandomResData,
  };
  
  UI_ObjectSelector("Link", allow, 4, &evt->link_uuid);
}


static void UI_EditRandomResData(usnd_entry *e) {
  
}

static void UI_EditProgramResData(usnd_entry *e) {
  
}

static void UI_EditWavResData(usnd_entry *e) {
  
}

static void UI_EditWaveFileIdObj(usnd_entry *e) {
  
}


#pragma mark - UI Panel

static void UI_DrawEntries(void) {
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  if (igBegin("Entries", NULL, ImGuiWindowFlags_MenuBar) && Project) {
    struct SearchContext *search = &UI_FixedSearch;
    igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, ImVec2(5,5));
    if (igBeginMenuBar()) {
      if (igBeginMenu("Filter", true)) {
        if (igBeginMenu("Type", true)) {
          u64 previous = search->class_flags;
          igCheckboxFlags_U64Ptr("Any",            &search->class_flags, 1 << CRTTIClass);
          if (search->class_flags & (1 << CRTTIClass)) igBeginDisabled(true);
          igCheckboxFlags_U64Ptr("EventResData",   &search->class_flags, 1 << CEventResData);
          igCheckboxFlags_U64Ptr("RandomResData",  &search->class_flags, 1 << CRandomResData);
          igCheckboxFlags_U64Ptr("ProgramResData", &search->class_flags, 1 << CProgramResData);
          igCheckboxFlags_U64Ptr("SwitchResData",  &search->class_flags, 1 << CSwitchResData);
          igCheckboxFlags_U64Ptr("ActorResData",   &search->class_flags, 1 << CActorResData);
          igCheckboxFlags_U64Ptr("WavResData",     &search->class_flags, 1 << CWavResData);
          igCheckboxFlags_U64Ptr("WaveFileIdObj",  &search->class_flags, 1 << CWaveFileIdObj);
          if (search->class_flags & (1 << CRTTIClass)) igEndDisabled();
          if (search->class_flags != previous) SearchContext_DoSearch(search);
          igEndMenu();
        }
        
        if (igBeginMenu("Bank", true)) {
          igPushStyleVar_Vec2(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
          if (igRadioButton_Bool("Any", !search->bank)) {
            search->bank = NULL;
            SearchContext_DoSearch(search);
          }
          
          igPopStyleVar(1);
          igEndMenu();
        }
        igEndMenu();
      }
    }
    
    igTextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "(%d)", search->num_results);
    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
      igSetTooltip("%d entries total across banks", Project->num_entries);
    }
    
    igEndMenuBar();
   
    igPopStyleVar(1);
    
    static u32 num_selected = 0;
    if (UI_DrawEntryTable(&UI_FixedSearch, UI_Selection)) {
      UI_Selection[1] = NULL;
    }
  }
  igEnd();
  igPopStyleVar(1);
}

static void UI_DrawEntryInfo(void) {
  usnd_entry *e = UI_GetPrimarySelection();
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, ImVec2(5,5));
  if (igBegin("EntryInfo", NULL, ImGuiWindowFlags_None) && Project && e) {
    String uuid = {};
    GetEntryUUID(e, uuid);
    
    ImVec4 color0 = UI_GetEntryColor(e);
    ImVec4 color1 = color0;
    ImVec4 color2 = color0;
    color0.w = 1.0f;
    color1.w = 0.85f;
    color2.w = 0.7f;
    
    igTextColored(color0, uuid);
    igTextColored(color1, "%s", usnd_class_name(e->type));

    int version = GetEntryVersion(e);
    if (version != -1) {
      color1.w *= 0.5f;
      igSameLine(0.0f, 5.0f);
      igTextColored(color1, "(v%d)", version);
    }
      
    igSeparator();
    
    enum usnd_class type = usnd_general_class(e->type);
    if (type == CEventResData)
      UI_EditEventResData(e);
    else if (type == CRandomResData)
      UI_EditRandomResData(e);
    else if (type == CProgramResData)
      UI_EditProgramResData(e);
    else if (type == CWavResData)
      UI_EditWavResData(e);
    else if (type == CWaveFileIdObj)
      UI_EditWaveFileIdObj(e);
  }
  igEnd();
  igPopStyleVar(1);
}

static void UI_DrawEntryTree(void) {
  ImGuiWindowFlags flags = 0;
  flags |= ImGuiWindowFlags_NoMove;
  flags |= ImGuiWindowFlags_NoScrollbar;
  flags |= ImGuiWindowFlags_NoScrollWithMouse;
  
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  if (igBegin("EntryTree", NULL, flags) && Project && UI_Selection[0]) {
    ImGuiTableFlags flags = 0;
    flags |= ImGuiTableFlags_BordersInnerV;
    flags |= ImGuiTableFlags_Resizable;
    flags |= ImGuiTableFlags_ScrollY;
    flags |= ImGuiTableFlags_RowBg;
    flags |= ImGuiTableFlags_NoBordersInBody;
    
    if (igBeginTable("Entries", 3, flags, ImVec2(0.0f, 0.0f), 0.0f)) {
      igTableSetupColumn("Name/UUID", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
      igTableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 115.0f, 0);
      igTableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 55.0f, 0);
      igTableHeadersRow();
      UI_ObjectTree(UI_Selection[0], 0, "--");
      igEndTable();
    }
  }
  igEnd();
  igPopStyleVar(1);
}

static void UI_DrawAudioPlayer(void) {
  igBegin("AudioPlayer", NULL, ImGuiWindowFlags_None);
  
  igEnd();
}

static void UI_DrawPlaybackControls(void) {
  igBegin("PlaybackControls", NULL, ImGuiWindowFlags_None);
  
  igEnd();
}

static void UI_DrawAudioQueue(void) {
  igBegin("AudioQueue", NULL, ImGuiWindowFlags_None);
  
  igEnd();
}

static void UI_DrawPanels(void) {
  UI_DrawEntries();
  UI_DrawEntryInfo();
  UI_DrawEntryTree();
  UI_DrawAudioPlayer();
  UI_DrawPlaybackControls();
  UI_DrawAudioQueue();
}

static void UI_HandleGlobalShortcuts(void) {
  
}

static void UI_DrawMainMenuBar(void) {
  if (igBeginMainMenuBar()) {
    if (igBeginMenu("File", true)) {
      igMenuItem_BoolPtr("Quit", NULL, &Quit, true);
      igEndMenu();
    }
    
    if (igBeginMenu("Edit", true)) {
      if (igMenuItem_Bool("Undo", "Ctrl+Z", false, false));
      if (igMenuItem_Bool("Redo", "Ctrl+Shift+Z", false, false));
      igSeparator();
      if (igMenuItem_Bool("Find", "Ctrl+F", false, true));
      
      igEndMenu();
    }
    
    if (igBeginMenu("Options", true)) {
      if (igBeginMenu("Audio options", true)) {
        if (igBeginMenu("Default language", true)) {
          usnd_language *l = &Opt_AudioDefaultLanguage;
          if (igMenuItem_Bool("German",  NULL, *l == USND_LANGUAGE_GERMAN, true)) *l = USND_LANGUAGE_GERMAN;
          if (igMenuItem_Bool("English", NULL, *l == USND_LANGUAGE_ENGLISH, true)) *l = USND_LANGUAGE_ENGLISH;
          if (igMenuItem_Bool("Spanish", NULL, *l == USND_LANGUAGE_SPANISH, true)) *l = USND_LANGUAGE_SPANISH;
          if (igMenuItem_Bool("French",  NULL, *l == USND_LANGUAGE_FRENCH, true)) *l = USND_LANGUAGE_FRENCH;
          if (igMenuItem_Bool("Italian", NULL, *l == USND_LANGUAGE_ITALIAN, true)) *l = USND_LANGUAGE_ITALIAN;
          igEndMenu();
        }
       
        igCheckbox("Allow randomness", &Opt_AudioAllowRandomness);
        igSeparator();
        igCheckbox("Draw sync markers", &Opt_DrawSyncMarkers);
        igCheckbox("Draw visualizer", &Opt_DrawVisualizer);
        
        igEndMenu();
      }
      
      if (igBeginMenu("Style options", true)) {
        igCheckbox("Compact UUIDs", &Opt_CompactUUIDs);
        igEndMenu();
      }
        
      igSeparator();
      
      igCheckbox("Draw timeline", &Opt_DrawTimeline);
      igCheckbox("Draw debugger", &Opt_DrawDebugger);
        
      igEndMenu();
    }
    
    /* version */
    const char text[] = USNDTOOL_VERSION_STRING;
    ImVec2 textsize = igCalcTextSize(text, text + sizeof(text), false, 0.0f);
    igSetCursorScreenPos(ImVec2(igGetIO_Nil()->DisplaySize.x - textsize.x, 0));
    igTextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.1f), text);
    
    igEndMainMenuBar();
  }
}

static void UI_DoStyle(void) {
  ImGuiStyle *style = igGetStyle();
  igStyleColorsDark(style);
  
  style->AntiAliasedFill = true;
  style->AntiAliasedLines = true;
  style->AntiAliasedLinesUseTex = true;
  
  style->FrameRounding  = 5.0f;
  style->PopupRounding  = 5.0f;
  style->GrabRounding   = 5.0f;
  style->ChildRounding  = 5.0f;
  style->WindowRounding = 5.5f;
  style->CellPadding    = ImVec2(2.0f, 1.0f);
  
  style->FrameBorderSize      = 0.0f;
  style->PopupBorderSize      = 1.0f;
  style->WindowBorderSize     = 2.0f;
  style->DockingSeparatorSize = 1.0f;
  style->DisabledAlpha        = 0.25f;
  
  style->Colors[ImGuiCol_MenuBarBg]                 = ImColor(30, 30, 30, 255);
  style->Colors[ImGuiCol_DockingEmptyBg]            = ImColor(5, 5, 5, 255);
  style->Colors[ImGuiCol_WindowBg]                  = ImColor(12, 12, 16, 255);
  style->Colors[ImGuiCol_TitleBg]                   = ImColor(20, 20, 20, 255);
  style->Colors[ImGuiCol_TitleBgActive]             = ImColor(30, 30, 30, 255);
  style->Colors[ImGuiCol_Border]                    = ImColor(45, 45, 45, 255);
  style->Colors[ImGuiCol_BorderShadow]              = ImColor(55, 55, 55, 255);
  style->Colors[ImGuiCol_PopupBg]                   = ImColor(30, 30, 30, 255);
  style->Colors[ImGuiCol_Tab]                       = ImColor(15, 15, 15, 255);
  style->Colors[ImGuiCol_TabDimmedSelected]         = ImColor(40, 40, 55, 255);
  style->Colors[ImGuiCol_TabHovered]                = ImColor(60, 60, 75, 255);
  style->Colors[ImGuiCol_TabSelected]               = ImColor(80, 80, 85, 255);
  style->Colors[ImGuiCol_Header]                    = ImColor(65, 65, 65, 255);
  style->Colors[ImGuiCol_HeaderHovered]             = ImColor(65, 65, 65, 255);
  style->Colors[ImGuiCol_HeaderActive]              = ImColor(65, 65, 65, 255);
  style->Colors[ImGuiCol_TableRowBg]                = ImColor(255, 255, 255, 2);
  style->Colors[ImGuiCol_TableRowBgAlt]             = ImColor(255, 255, 255, 8);
  style->Colors[ImGuiCol_Button]                    = ImColor(65, 65, 65, 255);
  style->Colors[ImGuiCol_ChildBg]                   = ImColor(30, 30, 30, 50);
  style->Colors[ImGuiCol_TabDimmedSelectedOverline] = ImColor(0,0,0,0);
  style->Colors[ImGuiCol_TabSelectedOverline]       = ImColor(0,0,0,0);
  style->Colors[ImGuiCol_CheckMark]                 = ImColor(50, 255, 180, 255);
  style->Colors[ImGuiCol_FrameBg]                   = ImColor(80, 80, 80, 50);
  style->Colors[ImGuiCol_ModalWindowDimBg]          = ImColor(15, 15, 20, 240);
  style->Colors[ImGuiCol_ScrollbarBg]               = ImColor(0, 0, 0, 100);
  
  style->HoverDelayNormal = 0.25f;
  style->HoverDelayShort = 0.25f;
  style->HoverStationaryDelay = 0.15f;
}

static void UI_DoLayout(void) {
  ImGuiDockNodeFlags flags = 0;
  flags |= ImGuiDockNodeFlags_DockSpace;
  flags |= ImGuiDockNodeFlags_NoDockingOverCentralNode;
  
  igDockBuilderRemoveNode(UI_MainDockspaceID);
  igDockBuilderAddNode(UI_MainDockspaceID, flags);
  igDockBuilderSetNodeSize(UI_MainDockspaceID, igGetMainViewport()->WorkSize);
  
  ImGuiID dock_main_id = UI_MainDockspaceID;
  ImGuiID left0 = igDockBuilderSplitNode(UI_MainDockspaceID, ImGuiDir_Left, 1.0f/4.0f, NULL, &dock_main_id);
  ImGuiID middle1 = dock_main_id;
  ImGuiID bottom = igDockBuilderSplitNode(middle1, ImGuiDir_Down, 1.0f/2.7f, NULL, &middle1);
  ImGuiID right = igDockBuilderSplitNode(middle1, ImGuiDir_Right, 1.0f/3.0f, NULL, &middle1);
  ImGuiID middle2 = igDockBuilderSplitNode(middle1, ImGuiDir_Down, 1.0f/4.0f, NULL, &middle1);
  ImGuiID right1 = igDockBuilderSplitNode(bottom, ImGuiDir_Right, 1.0/3.0f, NULL, &bottom);
  
  igDockBuilderDockWindow("Entries", left0);
  igDockBuilderDockWindow("AudioPlayer", middle1);
  igDockBuilderDockWindow("EntryInfo", right);
  igDockBuilderDockWindow("PlaybackControls", middle2);
  igDockBuilderDockWindow("AudioQueue", right1);
  igDockBuilderDockWindow("EntryTree", bottom);
  
  igDockBuilderFinish(UI_MainDockspaceID);
}

static void UI_Draw(void) {
  UI_DrawMainMenuBar();
  
  ImGuiViewport *viewport = igGetMainViewport();
  igSetNextWindowPos(viewport->WorkPos, ImGuiCond_None, ImVec2(0.0f, 0.0f));
  igSetNextWindowSize(viewport->WorkSize, ImGuiCond_None);
  igSetNextWindowViewport(viewport->ID);
  
  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_NoDocking;
  window_flags |= ImGuiWindowFlags_NoTitleBar;
  window_flags |= ImGuiWindowFlags_NoCollapse;
  window_flags |= ImGuiWindowFlags_NoResize;
  window_flags |= ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
  window_flags |= ImGuiWindowFlags_NoNavFocus;
  window_flags |= ImGuiWindowFlags_NoDecoration;
  
  igPushStyleVar_Float(ImGuiStyleVar_WindowRounding, 0.0f);
  igPushStyleVar_Float(ImGuiStyleVar_WindowBorderSize, 0.0f);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  
  if (igBegin("DockSpace", NULL, window_flags)) {
    ImGuiDockNodeFlags docknode_flags = 0;
    docknode_flags |= ImGuiDockNodeFlags_PassthruCentralNode;
    docknode_flags |= ImGuiDockNodeFlags_NoDocking;
    docknode_flags |= ImGuiDockNodeFlags_NoTabBar;
    docknode_flags |= ImGuiDockNodeFlags_NoResize;
    
    UI_MainDockspaceID = igGetID_Str("DockSpace");
    igDockSpace(UI_MainDockspaceID, ImVec2(0.0f, 0.0f), docknode_flags, NULL);
    
    if (UI_WantsLayout) {
      UI_WantsLayout = false;
      UI_DoLayout();
      UI_DoStyle();
    }
    
    UI_DrawPanels();
  }
  igEnd();

  igPopStyleVar(3);
  
  UI_HandleGlobalShortcuts();
  /* Clear temporary edits */
  usnd_arena_clear(&UI_Arena);
}

static bool UI_Init(void) {
  SearchContext_Clear(&UI_FixedSearch);
  SearchContext_Clear(&UI_PopupSearch);
  
  UI_FixedSearch.class_flags |= (1 << CRTTIClass);
  SearchContext_DoSearch(&UI_FixedSearch);
  
  UI_Arena.size = UI_ARENA_MAX_SIZE;
  UI_Arena.base = SDL_malloc(UI_ARENA_MAX_SIZE);
  UI_Arena.position = 0;
  
  return true;
}

static bool UI_Deinit(void) {
  SDL_free(UI_Arena.base);
}
  
#pragma mark - Application

extern void ImGui_ImplSDLRenderer3_Init(SDL_Renderer*);
extern void ImGui_ImplSDLRenderer3_NewFrame(void);
extern void ImGui_ImplSDLRenderer3_RenderDrawData(ImDrawData*, SDL_Renderer*);

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv) {
  if (argc > 1) {
    const char *dir_name = argv[1];
    Project = Project_LoadFromSoundFolder(dir_name);
  }
  
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("Failed to initialize SDL: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (!SDL_CreateWindowAndRenderer("usndtool", 800, 500, flags, &Window, &Renderer)) {
    SDL_Log("Failed to create window/renderer: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  
  SDL_SetAppMetadata("usndtool", USNDTOOL_VERSION_STRING, NULL);
  SDL_SetWindowMinimumSize(Window, 800, 500);
  SDL_SetWindowPosition(Window, SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1));
  
  if (!igCreateContext(NULL))
    return SDL_APP_FAILURE;
  
  ImGuiIO* io = igGetIO_Nil();
  io->BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  
  if (!ImGui_ImplSDL3_InitForSDLRenderer(Window, Renderer))
    return SDL_APP_FAILURE;
  
  ImGui_ImplSDLRenderer3_Init(Renderer);
    
  if (!UI_Init()) {
    SDL_Log("UI initialization failed\n");
    return SDL_APP_FAILURE;
  }
  
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *evt) {
  ImGui_ImplSDL3_ProcessEvent(evt);
  switch (evt->type) {
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;
      
    case SDL_EVENT_DROP_BEGIN:
      break;
      
    case SDL_EVENT_DROP_COMPLETE:
      break;
      
    case SDL_EVENT_DROP_FILE:
      break;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *state) {
  Uint64 start = SDL_GetTicks();
  
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  
  igNewFrame();
  UI_Draw();
  igRender();
  
  SDL_RenderClear(Renderer);
  ImGui_ImplSDLRenderer3_RenderDrawData(igGetDrawData(), Renderer);
  SDL_RenderPresent(Renderer);
  
  const float delay = 1000.0f / 60.0f;
  Uint64 end = SDL_GetTicks() - start;
  if (delay > end) SDL_Delay(delay - end);
  
  if (Quit)
    return SDL_APP_SUCCESS;
 
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *state, SDL_AppResult result) {
  UI_Deinit();
  ImGui_ImplSDL3_Shutdown();
  SDL_DestroyRenderer(Renderer);
  SDL_DestroyWindow(Window);
  SDL_Quit();
}
