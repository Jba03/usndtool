/* usndtool: Tool for editing 'HarmonX' soundbank files */
/* Created by Jba03 initially on 2024-07-02 */

#if !defined(USNDTOOL_VERSION_STRING)
# define USNDTOOL_VERSION_STRING "unknown_version"
#endif

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define CIMGUI_USE_SDL3
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimgui/cimgui_impl.h"

#include "usnd/usnd.h"
#include "usnd/audio.h"

static bool Quit = false;
static SDL_Window *Window = NULL;
static SDL_Renderer *Renderer = NULL;

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
