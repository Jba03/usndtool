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

/* General options */
static Path Opt_ProjectsDir;
static bool Opt_DrawVisualizer = true;
static bool Opt_DrawSyncMarkers = true;
static bool Opt_DrawDebugger = false;
static bool Opt_DrawTimeline = false;
static bool Opt_CompactUUIDs = true;

/* Audio options */
static float Opt_AudioMasterGain = 1.0f;
static float Opt_AudioMasterPan = 0.0f;
static bool  Opt_AudioRepeat = false;
static bool  Opt_AudioAllowRandomness = true;
static u32   Opt_AudioDefaultLanguage = USND_LANGUAGE_ENGLISH;

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

#pragma mark - UI -

#define ImVec2(x,y)       (ImVec2){x,y}
#define ImVec4(x,y,z,w)   (ImVec4){x,y,z,w}
#define ImColor(x,y,z,w)  (ImVec4){x/255.0f,y/255.0f,z/255.0f,w/255.0f}

static bool UI_WantsLayout = true;
static ImGuiID UI_MainDockspaceID;

/* Global shortcuts */
static const ImGuiKeyChord UI_UndoKeyChord = ImGuiMod_Ctrl | ImGuiKey_Z;
static const ImGuiKeyChord UI_RedoKeyChord = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z;
static const ImGuiKeyChord UI_AudioPauseKeyChord = ImGuiKey_Space;
static const ImGuiKeyChord UI_QuickSearchKeyChord = ImGuiMod_Ctrl | ImGuiKey_F;
static const ImGuiKeyChord UI_QueueResetKeyChord = ImGuiMod_Ctrl | ImGuiKey_R;

#pragma mark - UI Element

static bool UI_DrawButtonBase(ImVec2 *size, ImVec4 text, ImVec4 bg, bool arrow) {
  if (size->x == 0 && size->y == 0) *size = ImVec2(15, 15);
  igPushStyleColor_Vec4(ImGuiCol_Text, text);
  igPushStyleColor_Vec4(ImGuiCol_Button, bg);
  bool pressed = arrow ? igArrowButtonEx("##", ImGuiDir_Right, *size, 0) : igButtonEx("##", *size, 0);
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

#pragma mark - UI Panel

static void UI_DrawEntries(void) {
  igBegin("Entries", NULL, ImGuiWindowFlags_None);
  
  igEnd();
}

static void UI_DrawEntryInfo(void) {
  igBegin("EntryInfo", NULL, ImGuiWindowFlags_None);
  
  igEnd();
}

static void UI_DrawEntryTree(void) {
  igBegin("EntryTree", NULL, ImGuiWindowFlags_None);
  
  igEnd();
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
        igCheckbox("Use compact UUIDs", &Opt_CompactUUIDs);
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
}

#pragma mark - Application

extern void ImGui_ImplSDLRenderer3_Init(SDL_Renderer*);
extern void ImGui_ImplSDLRenderer3_NewFrame(void);
extern void ImGui_ImplSDLRenderer3_RenderDrawData(ImDrawData*, SDL_Renderer*);

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv) {
  if (argc > 1) {
    const char *dir_name = argv[1];
    struct Project *project = Project_LoadFromSoundFolder(dir_name);
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
  ImGui_ImplSDL3_Shutdown();
  SDL_DestroyRenderer(Renderer);
  SDL_DestroyWindow(Window);
  SDL_Quit();
}
