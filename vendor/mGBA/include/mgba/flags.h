/* TizGBA: replaces the CMake-generated mgba/flags.h (see PATCHES.md). The
   same switches are also passed with -D by the Makefiles; guarded here. */
#ifndef FLAGS_H
#define FLAGS_H

#ifndef MINIMAL_CORE
#define MINIMAL_CORE 2
#endif
#ifndef M_CORE_GBA
#define M_CORE_GBA
#endif
#ifndef DISABLE_THREADING
#define DISABLE_THREADING
#endif
#ifndef HAVE_LOCALTIME_R
#define HAVE_LOCALTIME_R
#endif
#ifndef HAVE_STRDUP
#define HAVE_STRDUP
#endif

#endif
