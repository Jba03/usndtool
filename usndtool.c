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

#pragma mark - Application


static void *usnd_realloc(void *mem, usnd_size size) {
  return SDL_realloc(mem, (size_t)size);
}

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv) {
  if (argc > 1) {
    const char *filename = argv[1];
    
    size_t size;
    void *data = SDL_LoadFile(filename, &size);
    
    usnd_size wanted_size = usnd_soundbank_loaded_size((usnd_size)size);
    
    usnd_arena arena = {};
    arena.size = wanted_size;
    arena.base = SDL_malloc(wanted_size);
    
    usnd_soundbank *bank = usnd_soundbank_load(&arena, data, (usnd_size)size);
    
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
