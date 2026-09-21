#ifndef _ACM_LAB_SUBBAND_DECODER_H
#define _ACM_LAB_SUBBAND_DECODER_H

#include <stdlib.h>
#include <cstdint>

class CSubbandDecoder {
private:
	int levels, block_size;
	int32_t* memory_buffer;
	void sub_4d3fcc(int32_t* memory, int32_t* buffer, int sb_size, int blocks);
	void sub_4d420c(int32_t* memory, int32_t* buffer, int sb_size, int blocks);
public:
	CSubbandDecoder(int lev_cnt)
		: levels(lev_cnt),
		block_size(1 << lev_cnt),
		memory_buffer(NULL) {
	};
	virtual ~CSubbandDecoder() { if (memory_buffer) free(memory_buffer); };

	int init_decoder();
	void decode_data(int32_t* buffer, int blocks);
};

#endif