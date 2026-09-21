#ifndef _ACM_LAB_VALUE_PACKER_H
#define _ACM_LAB_VALUE_PACKER_H

#include <stdio.h>
#include "bitstream.h"

class CValuePacker {
private:
	FILE* file; // output file
	CBitStream* bit_stream; // output bitstream
	int32_t subblocks, // count of sub-blocks in the PackedBlock
		sb_size, // size of one sub-block
		pblock_size; // size of PackedBlock
	int16_t* pblock; // PackedBlock
	int16_t *max_abs, *max_plus; // max abs and max positive values in the columns
	int32_t max_bits_limit; // limit on the length of bits in a pblock
	int16_t cur_val; // quantizer step (val) used for the current packed block
	int32_t* ck_wrap; // persistent decoder wrap state for the decode check
	int32_t ck_levels; // transform levels (log2 of sb_size)
	int32_t ck_pwr; // pwr field written for the current block (amp-table bound)

	void decode_check (); // simulate the decoder's transform and prevent output wraparound

	void pack_column (int col); // chooses which packing method to use in a column and writes the bits into stream
	void analyse (const int16_t* block); // finds the quntizer cell width to fit the data into specified bit-limit
	void granulate (int val); // granulates the block
	double estimate (int val); // roughly estimates the length of packed block

	void make_k (int ind, int col);
	void make_t15 (int col);
	void make_t27 (int col);
	void make_t37 (int col);
	void make_linear (int bits, int col);
public:
	CValuePacker (FILE* out_file, int32_t sbc, int32_t sbs, int32_t limit) // count of sub-blocks, their size
	// and the limit on the bits per packed block (-1 means no limit)
		: file (out_file),
		bit_stream (NULL),
		subblocks (sbc), sb_size (sbs),
		pblock_size (subblocks * sb_size),
		pblock (NULL),
		max_abs (NULL), max_plus (NULL),
		max_bits_limit (limit),
		cur_val (1), ck_wrap (NULL), ck_levels (0), ck_pwr (0) {};
	virtual ~CValuePacker() {
		if (bit_stream) delete bit_stream;
		if (pblock) delete pblock;
		if (max_abs) delete max_abs;
		if (max_plus) delete max_plus;
		if (ck_wrap) delete ck_wrap;
	};

	int init_packer();
	void add_one_block (const int16_t* block); // add one PackedBlock
	int32_t flush_bit_stream(); // return value is the count of written bytes
};

#endif
