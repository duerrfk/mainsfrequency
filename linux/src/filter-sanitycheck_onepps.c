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
#define F_CLK_NOMINAL (84000000/2)

uint32_t f_clk_corrected = F_CLK_NOMINAL;

void usage(const char *app)
{
     fprintf(stderr, "USAGE: %s "
	     "-d MAX_DEVIATION_PPM "
	     "\n", app);
}

void sanity_check_onepps(tlv_t *tlv, unsigned int max_deviation_ppm)
{
     double deviation = (double) (tlv->value.fclock) / F_CLK_NOMINAL;
     if (deviation > 1.0) {
	  deviation -= 1.0;
     } else {
	  deviation = 1.0 - deviation;
     }
     unsigned int deviation_ppm = (unsigned int) (1.0e6*deviation + 0.5);

     if (deviation_ppm > max_deviation_ppm) {
	  // Drop this 1-pps measurement.
	  WARNING("1-pps measurement exceeds maximum deviation from nominal frequency (dropped 1-pps measurement)");
	  return;
     }

     // 1-pps measurement passed sanity check. Pass it through.

     if (write_tlv(tlv, stdout) < 0) {
	  ERROR("Error while writing to stdout");
	  exit(-1);
     }	                
}

int main(int argc, char *argv[])
{
     double fnominal = -1.0; // nominal frequency
     int max_deviation_ppm = -1; // maximum allowed relative deviation in ppm  
     
     int c;
     while ((c = getopt (argc, argv, "d:")) != -1) {
	  switch (c) {
	  case 'd' :
	       max_deviation_ppm = atoi(optarg);
	       break;
	  case '?':
	  default :
	       usage(argv[0]);
	       exit(-1);
	  }
     }
     if (max_deviation_ppm < 0) {
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
	  case TLV_TYPE_ONEPPS :
	       sanity_check_onepps(&tlv, max_deviation_ppm);
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
