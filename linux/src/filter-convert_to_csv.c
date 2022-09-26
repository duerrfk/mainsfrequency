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
#include "errandwarn.h"

// Samples are taken at a nominal frequency of MCLK/2 = 42 MHz.
// (MCLK = master clock frequency of Arduino Due = 84 MHz)
#define F_CLK_NOMINAL (84000000/2)

#define MAX_TIMESTR_LEN 1000

// Clock frequency synchronized to 1-pps signal.
uint32_t f_clk_syncd = F_CLK_NOMINAL;

// Last wallclock timestamp (Unix epoch) seen in the stream.
uint64_t t_wallclock = 0;

void process_tlv_samples(const tlv_t *tlv)
{
     size_t nsamples = tlv->length/sizeof(uint32_t);

     struct tm *tmtime;
     char timestr[MAX_TIMESTR_LEN];
     time_t tsec = t_wallclock/1000000000; // POSIX defines type time_t as seconds since UNIX epoch.
     if ( (tmtime = gmtime(&tsec)) == NULL) {
	  ERROR("Could not convert time");
	  exit(-1);
     }
     memset(timestr, 0, MAX_TIMESTR_LEN);
     strftime(timestr, MAX_TIMESTR_LEN, "%Y-%m-%d %H:%M:%S", tmtime);
     
     for (unsigned int i = 0; i < nsamples; i++) {
	  double freq = (double) F_CLK_NOMINAL / tlv->value.samples[i];
	  double freq_syncd = (double) f_clk_syncd / tlv->value.samples[i];
	  double deviation = (double) (f_clk_syncd) / F_CLK_NOMINAL;
	  if (deviation > 1.0) {
	       deviation -= 1.0;
	  } else {
	       deviation = 1.0 - deviation;
	  }
	  unsigned int deviation_ppm = (unsigned int) (1.0e6*deviation + 0.5);
	  printf("%.4f,%.4f,%d,%d,%llu,%s\n", freq, freq_syncd, f_clk_syncd, deviation_ppm, t_wallclock, timestr);  
     }
}

void process_tlv_onepps(const tlv_t *tlv)
{
     f_clk_syncd = tlv->value.fclock;
}

void process_tlv_wallclocktime(const tlv_t *tlv)
{
     t_wallclock = tlv->value.wallclocktime;
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
     case TLV_TYPE_WALLCLOCKTIME :
	  process_tlv_wallclocktime(tlv);
	  break;
     }
}

int main(int argc, char *argv[])
{
     printf("f_mains,f_mains_syncd,f_clk_syncd,clk_accuracy_ppm,t_wallclock,t_wallclock_str\n");
     
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

	  process_tlv(&tlv);
     }
     
     return 0;
}
