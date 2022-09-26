/**
 * Copyright 2022 Frank Duerr
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
