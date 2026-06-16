cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.cmake" CACHE PATH "toolchain file")
    message(STATUS "Using VITASDK toolchain: ${CMAKE_TOOLCHAIN_FILE}")
  else()
    message(FATAL_ERROR "Define VITASDK env var or pass -DCMAKE_TOOLCHAIN_FILE=<vitasdk>/share/vita.cmake")
  endif()
endif()

project(AlephOneVita CXX C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# ---- Vita app identity -------------------------------------------------------
set(VITA_APP_NAME  "Aleph One")
set(VITA_TITLEID   "ALPH00001")           # any unique 9-char [A-Z0-9] id
set(VITA_VERSION   "01.00")

# ---- Port options ------------------------------------------------------------
option(USE_VITAGL          "Build the legacy GL renderer through vitaGL (Milestone 2)" OFF)
option(DISABLE_NETWORKING  "Compile out all network play (recommended for first boot)" ON)
option(WITH_OPENAL         "Link OpenAL Soft for sound (requires SOFT_loopback build)"  ON)

# The engine root (Source_Files lives one level up from vita/)
set(A1_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SRC      "${A1_ROOT}/Source_Files")

# =============================================================================
#  Source set
# =============================================================================
#  The desktop build compiles per-directory static libs from the Linux/SDL
#  source set. We mirror that with globs + targeted exclusions. GLOB only
#  matches *.cpp here, so platform .mm files are already excluded.
# =============================================================================

set(A1_DIRS
  CSeries Files Videos GameWorld Input Lua Misc
  ModelView RenderMain RenderOther Sound XML)

set(ENGINE_SOURCES "")
foreach(dir ${A1_DIRS})
  file(GLOB _dir_cpp "${SRC}/${dir}/*.cpp")
  list(APPEND ENGINE_SOURCES ${_dir_cpp})
endforeach()

# Bundled Lua interpreter (C sources live in Lua/)
file(GLOB LUA_C "${SRC}/Lua/*.c")
list(APPEND ENGINE_SOURCES ${LUA_C})

# Top-level engine entry points
list(APPEND ENGINE_SOURCES
  "${SRC}/shell.cpp"
  "${SRC}/shell_misc.cpp"
  "${SRC}/shell_options.cpp"
  "${SRC}/main.cpp")

# Vita-specific glue: heap/stack sizing (vita_runtime), startup clock hacks
# (vita_platform), and libc compat shims — popen/pclose stubs newlib-vita lacks
# but bundled Lua references (vita_compat).
list(APPEND ENGINE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/vita_runtime.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vita_platform.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vita_compat.c")

# ---- Exclusions --------------------------------------------------------------
# Helper: drop a file from ENGINE_SOURCES if present.
function(a1_exclude path)
  list(REMOVE_ITEM ENGINE_SOURCES "${path}")
  set(ENGINE_SOURCES "${ENGINE_SOURCES}" PARENT_SCOPE)
endfunction()

# Misc: keep the POSIX thread-priority impl, drop the others + Steam shim.
a1_exclude("${SRC}/Misc/thread_priority_sdl_win32.cpp")
a1_exclude("${SRC}/Misc/thread_priority_sdl_macosx.cpp")
a1_exclude("${SRC}/Misc/thread_priority_sdl_dummy.cpp")
a1_exclude("${SRC}/Misc/steamshim_child.cpp")

# Files: zzip rwops is a *.c and not globbed above; nothing to drop here unless
# you enable zziplib (then add ${SRC}/Files/SDL_rwops_zzip.c and define HAVE_ZZIP).

# Networking: when disabled, every network .cpp self-guards its body with
# `#if !defined(DISABLE_NETWORKING)`, EXCEPT the files that define the public
# Net* API (network.cpp, NetworkInterface.cpp, HTTP.cpp, Update.cpp, ...).
# Those are replaced wholesale by Network/network_dummy.cpp. The simplest
# correct recipe is: drop the entire Network/ + TCPMess/ trees and add only
# network_dummy.cpp.  (Network/ and TCPMess/ were not globbed, so just add it.)
if(DISABLE_NETWORKING)
  list(APPEND ENGINE_SOURCES "${SRC}/Network/network_dummy.cpp")
else()
  message(FATAL_ERROR
    "Networking-on is not wired for Vita yet: it pulls in asio/curl/miniupnpc. "
    "Keep DISABLE_NETWORKING=ON for now.")
endif()

# OpenGL renderer: the OGL_*.cpp files require GL headers and only do anything
# when HAVE_OPENGL is defined. Exclude them unless USE_VITAGL is set.
if(NOT USE_VITAGL)
  foreach(dir RenderMain RenderOther ModelView)
    file(GLOB _ogl "${SRC}/${dir}/OGL_*.cpp")
    foreach(f ${_ogl})
      a1_exclude("${f}")
    endforeach()
  endforeach()
endif()

# =============================================================================
#  Include paths (mirror the autotools AM_CPPFLAGS union)
# =============================================================================
set(A1_INCLUDES "${SRC}")
foreach(dir CSeries Files Videos GameWorld Input Lua Misc ModelView
            Network Network/Metaserver RenderMain RenderOther Sound TCPMess XML)
  list(APPEND A1_INCLUDES "${SRC}/${dir}")
endforeach()

# =============================================================================
#  Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${ENGINE_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${A1_INCLUDES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Feature defines that ./configure would otherwise put in config.h.
# We do NOT define HAVE_CONFIG_H, so confpaths.h/config.h are not required.
target_compile_definitions(${PROJECT_NAME} PRIVATE
  __VITA__                       # most SDL/vitasdk headers already set this; harmless to reassert
  HAVE_SDL_IMAGE
  HAVE_PNG
  HAVE_LUA
  HAVE_UNISTD_H
  TARGET_PLATFORM="psvita arm")

if(DISABLE_NETWORKING)
  target_compile_definitions(${PROJECT_NAME} PRIVATE DISABLE_NETWORKING)
endif()

if(USE_VITAGL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENGL HAVE_VITAGL)
endif()

# Vita is memory constrained — keep the binary lean and the stack sane.
target_compile_options(${PROJECT_NAME} PRIVATE
  -Wno-narrowing -Wno-write-strings -fno-strict-aliasing
  -fsigned-char)

# =============================================================================
#  Libraries
# =============================================================================
#  Order matters with static newlib linking. SDL2 first, then media/codec libs,
#  then Boost, then the Vita system stubs.
# =============================================================================

set(A1_LIBS "")

if(USE_VITAGL)
  # vitaGL + its runtime shader compiler stack. mathneon supplies the NEON
  # math vitaGL is built against. Order matters for static resolution.
  list(APPEND A1_LIBS
    vitaGL vitashark
    SceShaccCgExt SceShaccCg_stub
    taihen_stub mathneon)
endif()

# SDL2 (vitasdk build) + its satellites
list(APPEND A1_LIBS
  SDL2 SDL2_image SDL2_ttf
  freetype png jpeg webp
  zip z)

# Audio decoding: Sound/SndfileDecoder.cpp links libsndfile, which on vdpm is
# built against the Ogg/Vorbis/FLAC codecs — so they must be resolved at link.
# If your vdpm libsndfile was built minimal, trim this back to just `sndfile`.
list(APPEND A1_LIBS
  sndfile vorbisenc vorbis FLAC ogg)

# Boost components actually used by the engine (algorithm/uuid/property_tree are
# header-only; filesystem/iostreams/dll need their compiled libs).
list(APPEND A1_LIBS
  boost_filesystem boost_iostreams boost_system)

if(WITH_OPENAL)
  # OpenAL Soft (must be built with the ALC_SOFT_loopback extension — see README)
  list(APPEND A1_LIBS openal)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE A1_NO_SOUND)
endif()

# Vita system stubs
list(APPEND A1_LIBS
  m
  SceLibKernel_stub SceAppMgr_stub SceAppUtil_stub
  SceCommonDialog_stub SceDisplay_stub SceGxm_stub
  SceCtrl_stub SceTouch_stub SceMotion_stub
  SceAudio_stub SceAudioIn_stub
  SceSysmodule_stub SceProcessmgr_stub
  ScePower_stub SceRtc_stub SceHid_stub
  SceNet_stub SceNetCtl_stub
  SceFios2_stub SceIofilemgr_stub
  pthread)

target_link_libraries(${PROJECT_NAME} ${A1_LIBS})

# =============================================================================
#  VPK packaging
# =============================================================================
vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

set(VPK_FILES "")
set(_assets
  sce_sys/icon0.png
  sce_sys/livearea/contents/bg.png
  sce_sys/livearea/contents/startup.png
  sce_sys/livearea/contents/template.xml)
foreach(asset ${_assets})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${asset}")
    list(APPEND VPK_FILES FILE "${CMAKE_CURRENT_SOURCE_DIR}/${asset}" "${asset}")
  endif()
endforeach()

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME    ${VITA_APP_NAME}
  ${VPK_FILES})
