#ifndef _ACM_LAB_SOUND_READER_H
#define _ACM_LAB_SOUND_READER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unpacker.h"
#include "decoder.h"
#include "general.h"

#define INIT_NO_ERROR_MSG 0
#define INIT_NEED_ERROR_MSG 1

// Abstract Sound Reader class
class CSoundReader {
protected:
	int32_t samples; // total count of sound samples
  int32_t channels;
  int32_t samplerate;
	int32_t samples_left; // count of unread samples
	int is16bit; // 1 - if 16 bit file, 0 - otherwise
	FILE* file; // file handle
  int maxlen;

public:
	CSoundReader (int fhandle, int len)
    : samples (0), channels(0), samples_left (0), is16bit (1), file (NULL) 
  {
    file=fdopen(fhandle,"rb");
    maxlen=len;
  };

	virtual ~CSoundReader() {
		if (file) fclose (file);
	};

  int32_t get_channels()
  {
    return channels;
  }
  int32_t get_samplerate() { return samplerate; }
	virtual int init_reader () = 0; // initializes the sound reader

	int32_t get_length() { return samples; }; // returns the total samples count
	int32_t get_samples_left() { return samples_left; };
	int get_bits() { return (is16bit)?16:8; };

	virtual const char* get_file_type() = 0;

	virtual int32_t read_samples (int16_t* buffer, int32_t count) = 0; // returns actual count of read samples
};

// RAW file reader
class CRawPCMReader: public CSoundReader {
protected:
	int dup_mono; // 1 - duplicate mono input to stereo on the fly
	int16_t dup_pending; // deferred sample for odd-sized requests
	int dup_pending_valid;
public:
	CRawPCMReader (int fhandle, int bits, int len)
		: CSoundReader (fhandle, len),
		dup_mono (0), dup_pending (0), dup_pending_valid (0) {
			is16bit = (bits == 16);
      samples=len;
		};

	virtual int init_reader ();
	virtual int32_t read_samples (int16_t* buffer, int32_t count);
	virtual const char* get_file_type() { return (is16bit? "RAW16": "RAW8"); };
};

// WAV files
class CWavPCMReader: public CRawPCMReader {
public:
	CWavPCMReader (int fhandle,int32_t len)
		: CRawPCMReader (fhandle, 16, len) {};
	virtual int init_reader ();
	virtual const char* get_file_type() { return "WAV"; };
};

// IP's ACM files
class CACMReader: public CSoundReader {
private:
	int levels, subblocks;
	int block_size;
	int32_t *block, *values;
	int32_t samples_ready;
	CValueUnpacker* unpacker; // ACM-stream unpacker
	CSubbandDecoder* decoder; // IP's subband decoder

	int make_new_samples();
public:
	CACMReader (int fhandle, int len)
		: CSoundReader (fhandle, len),
		block (NULL), values (NULL),
		samples_ready (0),
		unpacker (NULL), decoder (NULL)
  {  };
	virtual ~CACMReader() {
		if (block) delete block;
		if (unpacker) delete unpacker;
		if (decoder) delete decoder;
	};

	virtual int init_reader ();
	virtual const char* get_file_type() { return "ACM"; };
	virtual int32_t read_samples (int16_t* buffer, int32_t count);

	int get_levels() { return levels; };
	int get_subblocks() { return subblocks; }
};


// WAVEFORMATEX structure (from MS SDK)
typedef struct
{
	uint16_t wFormatTag;         /* format type */
	uint16_t nChannels;          /* number of channels (i.e. mono, stereo...) */
	uint32_t  nSamplesPerSec;     /* sample rate */
	uint32_t  nAvgBytesPerSec;    /* for buffer estimation */
	uint16_t nBlockAlign;        /* block size of data */
	uint16_t wBitsPerSample;     /* number of bits per sample of mono data */
	uint16_t cbSize;             /* the count in bytes of the size of */
					   /* extra information (after cbSize) */
} cWAVEFORMATEX;

typedef struct {
	uint32_t fourcc;
	uint32_t length;
} RIFF_CHUNK;

const unsigned char RIFF_4cc[] = { 'R', 'I', 'F', 'F' };
const unsigned char WAVE_4cc[] = { 'W', 'A', 'V', 'E' };
const unsigned char fmt_4cc[]  = { 'f', 'm', 't', ' ' };
const unsigned char fact_4cc[] = { 'f', 'a', 'c', 't' };
const unsigned char data_4cc[] = { 'd', 'a', 't', 'a' };


// File open routine.
CSoundReader* CreateSoundReader (int fhandle, int open_mode, int32_t samples);

// Open modes:
#define SND_READER_AUTO 0
#define SND_READER_RAW8 1
#define SND_READER_RAW16 2
#define SND_READER_WAV 3
#define SND_READER_ACM 4

typedef struct MyFile {
  int fhandle;
  int32_t base;
  int32_t end;
} MyFile;

#endif
