#ifndef _UH_ACM2SND_H
#define _UH_ACM2SND_H

#include <cstdint>

int ConvertAcmWav(int fhandle, int maxlen, unsigned char *&memory, int32_t &samples_written, int forcestereo);

#endif
