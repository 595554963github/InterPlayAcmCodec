#ifndef _ACM_LAB_BIT_STREAM_H
#define _ACM_LAB_BIT_STREAM_H

#include <stdio.h>
#include <cstdint>

class CBitStream {
private:
	FILE* file;
	uint32_t accumulator; // not yet stored bits
	int shift; // next bit position
	uint32_t bits_written, bytes_written; // for statistics
	int32_t seq_zeros; // count of sequential zeros

	void internal_flush(); // drops all the fully filled bytes
	void drop_zeros(); // flushes all the sequentially coming zero bytes
public:
	CBitStream (FILE* a_file) 
		: file (a_file),
		accumulator (0), shift (0),
		bits_written (0), bytes_written (0),
		seq_zeros (0) {};
	~CBitStream() {	flush(); };
	
	int init_bit_stream() { return 1; }; // no initialization in current version
	void flush(); // flushes the last, unfinished byte. MUST be called at the and of processing
	void write_bits (uint32_t value, int count);
	uint32_t get_bits_written() { return bits_written; };
	uint32_t get_bytes_written() { return bytes_written; };
};

#endif
