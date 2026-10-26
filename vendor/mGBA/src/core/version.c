/* TizGBA: replaces the CMake-generated version.c (src/core/version.c.in).
   Values are those a clean `git describe` of the 0.10.5 release gives. */
#include <mgba/core/version.h>

MGBA_EXPORT const char* const gitCommit = "26b7884bc25a5933960f3cdcd98bac1ae14d42e2";
MGBA_EXPORT const char* const gitCommitShort = "26b7884";
MGBA_EXPORT const char* const gitBranch = "";
MGBA_EXPORT const int gitRevision = -1;
MGBA_EXPORT const char* const binaryName = "mgba";
MGBA_EXPORT const char* const projectName = "mGBA";
MGBA_EXPORT const char* const projectVersion = "0.10.5";
