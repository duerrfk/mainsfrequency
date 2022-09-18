#ifndef TLV_H
#define TLV_H

#include <stdint.h>
#include <stdio.h>

#define MAX_SAMPLE_COUNT 1000

// Different packet types to transport different data.
#define TLV_TYPE_SAMPLES 0    /* samples packet */
#define TLV_TYPE_ONEPPS 1     /* 1-pps calibration packet */
#define TLV_TYPE_WALLCLOCKTIME 2  /* packet carrying wallclock timestamp as uint64_t representing nanoseconds since Epoch */

typedef struct __attribute__((__packed__)) {
     uint16_t type;
     uint16_t length; // actual length of value
     union {
	  uint32_t fclock;
	  uint64_t wallclocktime;
	  uint32_t samples[MAX_SAMPLE_COUNT];	  
     } value;
} tlv_t;

int read_tlv(tlv_t *tlv, FILE *f);

int write_tlv(const tlv_t *tlv, FILE *f);
     
#endif
