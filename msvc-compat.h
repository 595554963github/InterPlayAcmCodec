#ifndef _MSVC_COMPAT_H
#define _MSVC_COMPAT_H

#ifdef _MSC_VER

// Standard headers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <io.h>
#include <direct.h>

// off_t type definition
typedef __int64 off_t;

// lseek replacement
#ifndef lseek
#define lseek _lseeki64
#endif

// read/write/close - use underscore versions
#ifndef read
#define read _read
#endif
#ifndef write
#define write _write
#endif
#ifndef close
#define close _close
#endif

// fileno replacement
#ifndef fileno
#define fileno _fileno
#endif

// strcasecmp/stricmp
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

// basename function for Windows
static inline char* basename(char* path)
{
    char* base = strrchr(path, '/');
    char* base2 = strrchr(path, '\\');
    if (base2 > base) base = base2;
    return base ? base + 1 : path;
}

// dirname function for Windows
static inline char* dirname(char* path)
{
    static char buf[1024];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* slash = strrchr(buf, '/');
    char* backslash = strrchr(buf, '\\');
    char* last = (slash > backslash) ? slash : backslash;
    if (last)
    {
        if (last == buf)
            *(last + 1) = '\0';
        else
            *last = '\0';
    }
    else
    {
        buf[0] = '.';
        buf[1] = '\0';
    }
    return buf;
}

// snprintf compatibility
#ifndef snprintf
#define snprintf _snprintf
#endif

// vsnprintf compatibility
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif

// unlink compatibility
#ifndef unlink
#define unlink _unlink
#endif

// chdir compatibility
#ifndef chdir
#define chdir _chdir
#endif

// getcwd compatibility
#ifndef getcwd
#define getcwd _getcwd
#endif

// SEEK_* macros (should already be in stdio.h)
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#endif // _MSC_VER

#endif // _MSVC_COMPAT_H
