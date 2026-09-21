#include "portable-utils.h"

#include <cstring>

#if defined(_MSC_VER)
#include <io.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif


int strstartswith(char const *str, char const *prefix)
{
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

#if defined(_MSC_VER)
long int acm_filelength(int fd)
{
  off_t pos = lseek(fd, 0, SEEK_CUR);
  off_t length = lseek(fd, 0, SEEK_END);
  lseek(fd, pos, SEEK_SET);
  return (int)length;
}

long int acm_tell(int fd)
{
  return (int)lseek(fd, 0, SEEK_CUR);
}
#elif !defined(_WIN32)
int filelength(int fd)
{
  off_t pos = lseek(fd, 0, SEEK_CUR);
  off_t length = lseek(fd, 0, SEEK_END);
  lseek(fd, pos, SEEK_SET);
  return (int)length;
}

int tell(int fd)
{
  return (int)lseek(fd, 0, SEEK_CUR);
}
#endif
