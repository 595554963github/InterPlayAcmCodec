// Classes for sound files.
// Supported formats: PCM-RAW, PCM-WAV (both 8 and 16 bits),
//   and Interplay's ACM.

#include <stdio.h>
#include "readers.h"
#include "general.h"

int CACMReader::init_reader () {
  ACM_Header hdr;
  char tmp[4];
  
  maxlen+=ftell(file);
  fread(tmp,1,sizeof(tmp),file);
  
  if(!memcmp(tmp,"WAVC",4) )
  {
    fseek(file,24,SEEK_CUR);
  }
  else
  {
    fseek(file,-4,SEEK_CUR);
  }
  
  fread (&hdr, 1, sizeof(ACM_Header), file);
  if (hdr.signature != IP_ACM_SIG) {
    return 0;
  }
  samples_left = (samples = hdr.samples);
  channels = hdr.channels;
  samplerate = hdr.rate;
  levels = hdr.levels;
  subblocks = hdr.subblocks;
  
  block_size = (1 << levels) * subblocks;
  block = new int32_t [block_size]; if (!block) return 0;
  unpacker = new CValueUnpacker (levels, subblocks, file, maxlen);
  if ( !unpacker || !unpacker->init_unpacker() ) return 0;
  decoder = new CSubbandDecoder (levels);
  if ( !decoder || !decoder->init_decoder() ) return 0;
  return 1;
}
int CACMReader::make_new_samples() {
  if ( !unpacker->get_one_block (block) )
  {
    return 0;
  }  
  
  decoder->decode_data (block, subblocks);
  values = block;
  samples_ready = (block_size > samples_left) ? samples_left: block_size;
  samples_left -= samples_ready;
  return 1;
}
int32_t CACMReader::read_samples (int16_t* buffer, int32_t count) {
  int32_t res = 0;
  while (res < count) {
    if (samples_ready == 0) {
      if (samples_left == 0) break;
      if (!make_new_samples()) break;
    }
    *buffer = (int16_t) ((*values) >> levels);
    values++;
    buffer++;
    res += 1;
    samples_ready--;
  }
  for (int i=res; i<count; i++, buffer++) *buffer = 0;
  return res;
}

CSoundReader* CreateSoundReader (int fhandle, int type, int32_t bytes)
{
  CSoundReader* res = NULL;
  
  switch(type)
  {
  case SND_READER_ACM:
    res = new CACMReader (fhandle, bytes);
    break;
  case SND_READER_WAV:
    res = new CWavPCMReader (fhandle, bytes);
    break;
  }
  if (res)
  {
    if ( !res->init_reader() )
    {
      delete res;
      res = NULL;
    }
  }
  return res;
}

int CRawPCMReader::init_reader () {
  if(samples<0)
  {
    fseek (file, 0, SEEK_END); samples = ftell (file); rewind (file);
    if (is16bit) samples >>= 1; // each sample has 16 bit
  }
  samples_left = samples;
  return 1;
}

int32_t CRawPCMReader::read_samples (int16_t* buffer, int32_t count) {
  int32_t res = 0, i;
  if (!dup_mono) {
    if (count > samples_left) count = samples_left;
    if (count)
      res = fread (buffer, (is16bit)?2:1, count, file);
    if (is16bit) {
      res >>= 1;
    }
    else
    {
      char* alt_buff = (char*) buffer;
      for (i=res-1; i>=0; i--) {
        alt_buff [(i << 1) + 1] = (char) (alt_buff [i] - 0x80);
        alt_buff [i << 1] = 0;
      }
    }
    samples_left -= res;
    return res;
  }

  // Mono input duplicated to stereo. `count` is the number of stereo
  // (interleaved) values requested; the file itself holds half as many.
  int32_t out = 0;
  if (dup_pending_valid) {
    if (count <= 0) return 0;
    buffer [out++] = dup_pending;
    dup_pending_valid = 0;
  }
  if (count > samples_left) count = samples_left;
  if (count & 1) count--; // hand out whole stereo pairs only
  if (count <= out) return out;
  int32_t want = (count - out) >> 1; // mono values to read from the file
  int32_t got = 0;
  if (want) {
    got = fread (buffer + out, (is16bit)?2:1, want, file);
    if (is16bit) {
      // fread returns the number of 2-byte items, i.e. exactly the number
      // of int16 values placed at buffer+out — no further conversion.
    }
    else
    {
      char* alt_buff = (char*) (buffer + out);
      for (i=got-1; i>=0; i--) {
        alt_buff [(i << 1) + 1] = (char) (alt_buff [i] - 0x80);
        alt_buff [i << 1] = 0;
      }
      // 8-bit path: `got` bytes became `got` int16 values in place
    }
  }
  // duplicate backward so source values are not overwritten
  for (i = got - 1; i >= 0; i--) {
    int16_t v = buffer [out + i];
    buffer [out + 2*i] = v;
    buffer [out + 2*i + 1] = v;
  }
  samples_left -= 2 * got;
  return out + 2 * got;
}

int CWavPCMReader::init_reader () {
  int res = CRawPCMReader::init_reader (); if (!res) return res;
  
  cWAVEFORMATEX fmt;
  RIFF_CHUNK r_hdr, fmt_hdr, data_hdr;
  uint32_t wave;
  memset (&fmt, 0, sizeof(fmt));
  fread (&r_hdr, 1, sizeof(r_hdr), file); fread (&wave, 1, 4, file);
  if (r_hdr.fourcc != *(uint32_t*)RIFF_4cc || wave != *(uint32_t*)WAVE_4cc) {
    return 0;
  }
  
  fread (&fmt_hdr, 1, sizeof(fmt_hdr), file);
  if (fmt_hdr.fourcc != *(uint32_t*)fmt_4cc || fmt_hdr.length > sizeof(cWAVEFORMATEX)) {
    return 0;
  }
  fread (&fmt, 1, fmt_hdr.length, file);
  if (fmt.wFormatTag != 1) {
    return 0;
  }
  is16bit = (fmt.wBitsPerSample == 16);
  
  fread (&data_hdr, 1, sizeof(data_hdr), file);
  if (data_hdr.fourcc == *(uint32_t*)fact_4cc) {
    fseek (file, data_hdr.length, SEEK_CUR);
    fread (&data_hdr, 1, sizeof(data_hdr), file);
  }
  if (data_hdr.fourcc != *(uint32_t*)data_4cc) {
    return 0;
  }
  
  samples = data_hdr.length;
  if (is16bit) samples >>= 1;
  if (fmt.nChannels == 1) {
    // The whole ACM ecosystem (the game engine, libacm/vgmstream) treats
    // every plain ACM file as stereo — libacm even force-sets channels=2,
    // which halves the play time of a genuine mono stream. Duplicate the
    // mono channel to stereo so the encoded ACM behaves like the originals.
    dup_mono = 1;
    samples *= 2;
    channels = 2;
  }
  else
    channels = fmt.nChannels;
  samplerate=fmt.nSamplesPerSec;
  samples_left = samples;
  return 1;
}

