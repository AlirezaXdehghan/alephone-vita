/*
 *  vita_platform.c — PS Vita startup hacks, mirroring the re3-vita footsteps.
 *
 *  vita_platform_init() is called once from initialize_application() (shell.cpp,
 *  inside the __vita__ guard) before SDL_Init.
 *
 *  It raises the userland clocks to the maximum the kernel allows
 *  (444/222/222/166 MHz for ARM/BUS/GPU/GPU-xbar) — the standard Vita port
 *  performance hack used by re3, DaedalusX64 et al.
 */

#ifdef __vita__

#include <psp2/power.h>

void vita_platform_init(void)
{
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
}

#else

void vita_platform_init(void) {}

#endif /* __vita__ */
