/*
 *  vita_compat.c — libc shims for functions the bundled Lua references but
 *  newlib-on-Vita does not implement (the Vita has no process model).
 *
 *  newlib *declares* popen/pclose in <stdio.h> (so liolib.c compiles) but does
 *  not *define* them, which would otherwise be an undefined-reference at link
 *  time. We provide inert definitions: io.popen() simply fails gracefully.
 *
 *  os.execute()/system() is already a no-op stub in newlib (returns nonzero),
 *  and tmpfile/tmpnam/mkstemp are provided by newlib, so they are not shimmed
 *  here.
 */

#ifdef __vita__

#include <stdio.h>
#include <errno.h>

FILE *popen(const char *command, const char *mode)
{
	(void)command;
	(void)mode;
	errno = ENOSYS;
	return NULL;
}

int pclose(FILE *stream)
{
	(void)stream;
	errno = ENOSYS;
	return -1;
}

#endif /* __vita__ */
