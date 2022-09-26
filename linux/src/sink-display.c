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

#include <stdio.h>
#include <stdlib.h>
#include "tlv.h"

// Samples are taken at a nominal frequency of MCLK/2 = 42 MHz.
// (MCLK = master clock frequency of Arduino Due = 84 MHz)
#define F_CLK_NOMINAL (84000000/2)

unsigned int nwin = 0;
double sumwin = 0.0;
uint32_t f_clk_syncd = F_CLK_NOMINAL;
double avg = 0.0;
FILE *outfile = NULL;

#define WARNING(m) (fprintf(stderr, "Warning: " m "\n"))
#define ERROR(m) (fprintf(stderr, "Error: " m "\n"))

void print()
{
     printf("\rf_mains = %.4f Hz \t f_clock = %u Hz", avg, f_clk_syncd);
     fflush(stdout);
}

void process_tlv_samples(const tlv_t *tlv)
{
     size_t nsamples = tlv->length/sizeof(uint32_t);

     for (unsigned int i = 0; i < nsamples; i++) {
	  double freq_syncd = (double) f_clk_syncd / tlv->value.samples[i];
	  sumwin += freq_syncd;
	  nwin++;
	  if (nwin == 50) {
	       avg = sumwin/50.0;
	       print();
	       sumwin = 0.0;
	       nwin = 0;
	  }
     }
}

void process_tlv_onepps(const tlv_t *tlv)
{
     f_clk_syncd = tlv->value.fclock;
     print();
}

void process_tlv(const tlv_t *tlv)
{
     switch (tlv->type) {
     case TLV_TYPE_SAMPLES :
	  process_tlv_samples(tlv);
	  break;
     case TLV_TYPE_ONEPPS :
	  process_tlv_onepps(tlv);
	  break;
     }
}

int main(int argc, char *argv[])
{
     tlv_t tlv;
     while (1) {
	  if (read_tlv(&tlv, stdin) < 0) {
	       ERROR("Could not read from stdin");
	       exit(-1);
	  }

	  process_tlv(&tlv);
     }

     return 0;
}
