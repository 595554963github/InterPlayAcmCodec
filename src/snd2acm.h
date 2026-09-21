#ifndef _UH_SND2ACM_H
#define _UH_SND2ACM_H

#include <cstdint>

int ConvertWavAcm(int fh, int maxlen, FILE *foutp, bool wavc_or_acm);

#endif
