#ifndef _ACM_LAB_VALUE_UNPACKER_H
#define _ACM_LAB_VALUE_UNPACKER_H

#include <cstdint>

class CValueUnpacker {
private:
// Parameters of ACM stream
	int levels, subblocks;
	FILE* file;
  int maxlen;
// Bits
	uint32_t next_bits; // new bits
	int avail_bits; // count of new bits

// Buffered input. Reading the stream byte by byte with an ftell() check
// per byte is very slow (millions of locked CRT calls); serve the bytes
// from a read-ahead buffer instead and track the stream position inline.
	enum { IN_BUF_SIZE = 1 << 16 };
	unsigned char* in_buf;
	int in_buf_pos;   // next index to serve inside in_buf
	int in_buf_len;   // valid bytes in in_buf
	int in_buf_eof;   // set once the stream is exhausted

	int sb_size, block_size;
	int16_t *amp_buffer, *buff_middle;
	int32_t* block_ptr;

// Reading routines
	void prepare_bits (int bits); // request bits
	int32_t get_bits (int bits); // request and return next bits
public:
// These functions are used to fill the buffer with the amplitude values
	int return0 (int pass, int ind);
	int zero_fill (int pass, int ind);
	int linear_fill (int pass, int ind);

	int k1_3bits (int pass, int ind);
	int k1_2bits (int pass, int ind);
	int t1_5bits (int pass, int ind);

	int k2_4bits (int pass, int ind);
	int k2_3bits (int pass, int ind);
	int t2_7bits (int pass, int ind);

	int k3_5bits (int pass, int ind);
	int k3_4bits (int pass, int ind);

	int k4_5bits (int pass, int ind);
	int k4_4bits (int pass, int ind);

	int t3_7bits (int pass, int ind);


	CValueUnpacker (int lev_cnt, int sb_count, FILE* handle, int len)
		: levels (lev_cnt), subblocks (sb_count),
		file (handle), maxlen (len),
		next_bits (0), avail_bits (0),
		in_buf (NULL), in_buf_pos (0), in_buf_len (0), in_buf_eof (0),
		sb_size (1<< levels), block_size (sb_size * subblocks),
		amp_buffer (NULL), buff_middle (NULL), block_ptr (NULL)
  {};
	virtual ~CValueUnpacker() { if (amp_buffer) delete amp_buffer; if (in_buf) delete in_buf; };

	int init_unpacker();
	int get_one_block (int32_t* block);
};

typedef int (CValueUnpacker::* FillerProc) (int pass, int ind);

#endif
