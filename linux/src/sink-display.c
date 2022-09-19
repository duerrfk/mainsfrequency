#include <stdio.h>
#include <stdlib.h>
#include "tlv.h"

// Samples are taken at a nominal frequency of MCLK/2 = 42 MHz.
// (MCLK = master clock frequency of Arduino Due = 84 MHz)
#define F_CLK (84000000/2)

unsigned int nwin = 0;
double sumwin = 0.0;
uint32_t f_clk = F_CLK;
double avg = 0.0;
FILE *outfile = NULL;

#define WARNING(m) (fprintf(stderr, "Warning: " m "\n"))
#define ERROR(m) (fprintf(stderr, "Error: " m "\n"))

void print()
{
     printf("\rf_mains = %.4f Hz \t f_clock = %u Hz", avg, f_clk);
     fflush(stdout);
}

void process_tlv_samples(const tlv_t *tlv)
{
     size_t nsamples = tlv->length/sizeof(uint32_t);

     for (unsigned int i = 0; i < nsamples; i++) {
	  double freq = (double) f_clk / tlv->value.samples[i];
	  sumwin += freq;
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
     f_clk = tlv->value.fclock;
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
