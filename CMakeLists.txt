cmake_minimum_required(VERSION 4.0)
project(usnd)

option(USND_BUILD_USNDTOOL "Build usndtool" ON)

set(USND_SRC
  usnd/audio.c
  usnd/audio.h
  usnd/common.c
  usnd/common.h
  usnd/object.h
  usnd/resource.c
  usnd/resource.h
  usnd/usnd.c
  usnd/usnd.h
  usnd/vm.c
  usnd/vm.h
  usnd/waveformat.c
  usnd/waveformat.h
)

add_library(usnd ${USND_SRC})
set_property(TARGET usnd PROPERTY C_STANDARD 99)

if (USND_BUILD_USNDTOOL)
  find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3-shared)

  set(cimgui_SOURCE_DIR "cimgui")
  add_library(cimgui SHARED
    ${cimgui_SOURCE_DIR}/cimgui.cpp
    ${cimgui_SOURCE_DIR}/cimgui_impl.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${cimgui_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    ${cimgui_SOURCE_DIR}/imgui/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(cimgui PUBLIC ${cimgui_SOURCE_DIR} ${cimgui_SOURCE_DIR}/generator/output)
  target_include_directories(cimgui PRIVATE ${cimgui_SOURCE_DIR}/imgui ${cimgui_SOURCE_DIR}/imgui/backends)
  target_compile_definitions(cimgui PUBLIC "-DIMGUI_USER_CONFIG=\"../cimconfig.h\"")
  target_compile_definitions(cimgui PUBLIC "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")
  
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(cimgui PUBLIC "-DIMGUI_IMPL_API=extern \"C\" __declspec\(dllexport\)")
  else()
    target_compile_definitions(cimgui PUBLIC "-DIMGUI_IMPL_API=extern \"C\" ")
  endif()

  target_compile_features(cimgui PRIVATE cxx_std_11)
  target_link_libraries(cimgui PUBLIC SDL3::SDL3)

  ## usndtool ##

  add_executable(usndtool usndtool.c)
  target_include_directories(usndtool PUBLIC usndtool .)
  set_property(TARGET usndtool PROPERTY C_STANDARD 99)
  target_link_libraries(usndtool PUBLIC SDL3::SDL3 cimgui usnd)
  target_compile_definitions(usndtool PUBLIC "-DIMGUI_IMPL_API=extern \"C\"")
  #target_compile_definitions(usndtool PUBLIC "-static-libgcc -static-libstdc++")

  if (APPLE)
    set_target_properties(usndtool PROPERTIES
      MACOSX_BUNDLE True
      MACOSX_BUNDLE_BUNDLE_NAME usndtool
      MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION})
  endif()

  # Versioning
  find_package(Git QUIET)
  if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    function(GitExec output)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} ${ARGN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE OUTPUT_VAR
      )
      set(${output} "${OUTPUT_VAR}" PARENT_SCOPE)
    endfunction()
    
    GitExec(VERSION_STRING describe --tags --dirty --always)
    if(NOT VERSION_STRING EQUAL "0")
      target_compile_definitions(usndtool PUBLIC -DUSNDTOOL_VERSION_STRING="${VERSION_STRING}")
    endif()
  endif()

endif()