#ifndef _ACM_LAB_GENERAL_H
#define _ACM_LAB_GENERAL_H

#include <cstdint>

// Interplay ACM signature
#define IP_ACM_SIG 0x01032897

#pragma pack (push, 1)
struct ACM_Header {
	int32_t signature;
	int32_t samples;
	uint16_t channels;
	uint16_t rate;
	uint16_t levels : 4;
	uint16_t subblocks : 12;
};
#pragma pack (pop)

#endif
