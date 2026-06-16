/*
 *  vita_runtime.c — PS Vita process tunables for Aleph One
 *
 *  These weak-overridable globals are read by vitasdk's crt0 at process start.
 *
 *  MEMORY BUDGET (the <300 MB guarantee):
 *  --------------------------------------
 *  We deliberately do NOT request the extended memory budget in param.sfo
 *  (re3-vita uses `vita-mksfoex -d ATTRIBUTE2=12` because GTA III is RAM-hungry;
 *  Marathon's assets are tiny by comparison). With the default budget the OS
 *  hard-caps this process's main RAM at ~256 MB — a ceiling enforced by the
 *  kernel, not by hope: allocation past it fails, the process can never *exceed*
 *  it. Our layout fits comfortably inside that ceiling:
 *
 *      newlib heap (below) .............. 128 MB
 *      executable + static data ......... ~40 MB
 *      main + worker thread stacks ...... ~6 MB
 *      SDL framebuffer / streaming tex .. ~8 MB   (software renderer)
 *      vitaGL GXM pool (USE_VITAGL only)   from CDRAM/phycont, not main RAM
 *      --------------------------------------------------
 *      worst case ....................... ~185 MB  << 300 MB
 *
 *  The 128 MB heap is itself the cap on dynamic allocation, so even a runaway
 *  WAD/image-cache load fails gracefully (malloc returns NULL) rather than
 *  pushing total RAM over budget. If a large scenario legitimately needs more,
 *  raise this AND add `-d ATTRIBUTE2=12` to VITA_MKSFOEX_FLAGS — but then you
 *  are intentionally going past the stock budget.
 */

#include <stdint.h>

/* 128 MB newlib heap — fits the stock ~256 MB budget with wide margin. */
unsigned int _newlib_heap_size_user = 128 * 1024 * 1024;

/* Main thread stack: the engine recurses through the renderer/Lua. */
unsigned int sceUserMainThreadStackSize = 2 * 1024 * 1024;
