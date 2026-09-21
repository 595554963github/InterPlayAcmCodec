#ifndef _UH_PORTABLE_UTILS_H
#define _UH_PORTABLE_UTILS_H

typedef unsigned char BYTE;

int strstartswith(char const *str, char const *prefix);

#if defined(_MSC_VER)
// MSVC: filelength and tell conflict with CRT POSIX names; use our own
long int acm_filelength(int fd);
long int acm_tell(int fd);
#define filelength acm_filelength
#define tell acm_tell
#elif defined(_WIN32)
// MinGW: CRT <io.h> already provides filelength()/tell()
#include <io.h>
#else
// Other platforms: no CRT version, declare our own
int filelength(int fd);
int tell(int fd);
#endif

#endif
