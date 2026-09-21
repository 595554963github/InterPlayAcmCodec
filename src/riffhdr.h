#ifndef _ACM_LAB_RIFF_HEADER_H
#define _ACM_LAB_RIFF_HEADER_H

#include <stdio.h>
#include <cstdint>

typedef struct {
	char riff_sig[4];
	uint32_t  total_len_m8;
	char wave_sig[8];
	uint32_t formatex_len;
	uint16_t wFormatTag, nChannels;
	uint32_t nSamplesPerSec, nAvgBytesPerSec;
	uint16_t nBlockAlign, wBitsPerSample;
	char data_sig[4];
	uint32_t raw_data_len;
} RIFF_HEADER;

typedef struct {
	char wavc_sig[4];
	char wavc_ver[4];
  int32_t uncompressed;
  int32_t compressed;
  int32_t headersize;
  uint16_t channels;
  uint16_t bits;
  uint16_t samplespersec;
  uint16_t unknown;
} WAVC_HEADER;

void write_riff_header (void* memory, int32_t samples, int channels, int samplerate);
void write_wavc_header (FILE *foutp, int32_t samples, int channels, int compressed, int samplerate);

#endif //_ACM_LAB_RIFF_HEADER_H
