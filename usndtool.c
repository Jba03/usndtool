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

#define STRING_MAX USND_STRING_MAX
typedef char String[STRING_MAX];

#define PATH_MAX 1024
typedef char Path[PATH_MAX];

static bool Quit = false;
static SDL_Window *Window = NULL;
static SDL_Renderer *Renderer = NULL;

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
  SDL_strlcpy(name, p0+1, SDL_max(p1-p0-1, STRING_MAX-1));
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

#pragma mark - Memory utils

static void *usnd_realloc(void *mem, usnd_size size) {
  return SDL_realloc(mem, (size_t)size);
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
  usnd_size required_size = usnd_soundbank_loaded_size(original_size);
  
  usnd_arena *arena = &bank->arena;
  arena->position = 0;
  arena->size = required_size;
  arena->base = SDL_malloc(required_size);
  
  if (!(bank->handle = usnd_soundbank_load(arena, data, original_size))) {
    SDL_Log("Failed to read bank %s\n", filepath);
    goto fail;
  }
  
  SDL_free(data);
  
  Path_Copy(bank->absolute_path, filepath);
  Path_GetFilename(filepath, bank->filename);
  return bank;
  
fail:
  if (data) SDL_free(data);
  if (bank) SDL_free(bank);
  if (arena->base) SDL_free(arena->base);
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

static int Project_AddBank(struct Project *p, SoundBank *bank) {
  if (p->num_banks + 1 >= PROJECT_MAX_BANKS) {
    SDL_Log("Failed to add bank to project: maximum number of banks reached");
    return 0;
  }
  
  p->banks[p->num_banks++] = bank;
  for (u32 i = 0; i < bank->handle->num_entries; i++)
    Project_PutEntry(p, bank, i);
  
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
  
  SDL_strlcpy(project->name, "DefaultProject", STRING_MAX);
  
  return project;
}

#pragma mark - Application

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv) {
  if (argc > 1) {
    const char *dir_name = argv[1];
    
    struct Project *project = Project_LoadFromSoundFolder(dir_name);
    return SDL_APP_FAILURE;
  }
  
  
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    return SDL_APP_FAILURE;
  
  SDL_SetAppMetadata("usndtool", USNDTOOL_VERSION_STRING, NULL);
  
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (!SDL_CreateWindowAndRenderer("usndtool", 800, 500, flags, &Window, &Renderer)) {
    SDL_Log("Failed to create window/renderer: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  
  SDL_SetWindowMinimumSize(Window, 800, 500);
  SDL_SetWindowPosition(Window, SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1));
  
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *_, SDL_Event *evt) {
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

SDL_AppResult SDL_AppIterate(void*_) {
  Uint64 start = SDL_GetTicks();
  
  SDL_RenderClear(Renderer);
  SDL_RenderPresent(Renderer);
  
  const float delay = 1000.0f / 60.0f;
  Uint64 end = SDL_GetTicks() - start;
  if (delay > end) SDL_Delay(delay - end);
  
  if (Quit)
    return SDL_APP_SUCCESS;
  else
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void*_, SDL_AppResult result) {
  SDL_DestroyRenderer(Renderer);
  SDL_DestroyWindow(Window);
  SDL_Quit();
}
