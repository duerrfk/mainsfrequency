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

#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tlv.h"

#define WARNING(m) (fprintf(stderr, "Warning: " m "\n"))
#define ERROR(m) (fprintf(stderr, "Error: " m "\n"))

// Samples are taken at a nominal frequency of MCLK/2 = 42 MHz.
// (MCLK = master clock frequency of Arduino Due = 84 MHz)
#define F_CLK (84000000/2)

uint32_t fclk = F_CLK;

void usage(const char *app)
{
     fprintf(stderr, "USAGE: %s "
	     "-f NOMINAL_FREQUENCY "
	     "-d MAX_DEVIATION "
	     "\n", app);
}

void sanity_check(tlv_t *tlv, double fnominal, double maxdev, uint32_t fclk)
{
     size_t nsamples = tlv->length/sizeof(uint32_t);
     size_t ncorrect = 0;
     tlv_t tlv_checked;

     tlv_checked.type = TLV_TYPE_SAMPLES;
     
     for (int i = 0; i < nsamples; i++) {
	  double f = (double) fclk / tlv->value.samples[i];
	  if (f > fnominal+maxdev || f < fnominal-maxdev) {
	       fprintf(stderr, "Dropped sample exceeding maximum deviation with f_mains = %f Hz (f_clock = %u)\n", f, fclk);
	  } else {
	       // Copy correct sample.
	       tlv_checked.value.samples[ncorrect++] = tlv->value.samples[i];
	  }
     }

     tlv_checked.length = ncorrect*sizeof(uint32_t);
     
     if (write_tlv(&tlv_checked, stdout) < 0) {
	  ERROR("Error while writing to stdout");
	  exit(-1);
     }	            
}

int main(int argc, char *argv[])
{
     double fnominal = -1.0; // nominal mains frequency
     double maxdev = -1.0; // maximum allowed deviation from nominal mains frequency in Hertz
     
     int c;
     while ((c = getopt (argc, argv, "f:d:")) != -1) {
	  switch (c) {
	  case 'f' :
	       fnominal = strtod(optarg, NULL);
	       break;
	  case 'd' :
	       maxdev = strtod(optarg, NULL);
	       break;
	  case '?':
	  default :
	       usage(argv[0]);
	       exit(-1);
	  }
     }
     if (fnominal < 0.0 || maxdev <= 0) {
	  usage(argv[0]);
	  exit(-1);
     }

     tlv_t tlv;
     while (1) {
	  if (read_tlv(&tlv, stdin) < 0) {
	       if (feof(stdin)) {
		    break;
	       } else if (ferror(stdin)) {
		    ERROR("Could not read TLV element from stdin");
		    exit(-1);
	       }
	  }

	  switch (tlv.type) {
	  case TLV_TYPE_SAMPLES :
	       sanity_check(&tlv, fnominal, maxdev, fclk);
	       break;
	  case TLV_TYPE_ONEPPS :
	       fclk = tlv.value.fclock;
	       if (write_tlv(&tlv, stdout) < 0) {
		    ERROR("Error while writing to stdout");
		    exit(-1);
	       }
	       break;
	  default :
	       // Pass-through any other element.
	       if (write_tlv(&tlv, stdout) < 0) {
		    ERROR("Error while writing to stdout");
		    exit(-1);
	       }
	  }
     }
     
     return 0;
}
