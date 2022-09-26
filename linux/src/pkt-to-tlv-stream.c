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

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tty.h"
#include "slip.h"
#include "crc.h"
#include "tlv.h"

#define MAX_PATH_SIZE 1000

#define MAX_PKT_SIZE 9000

#define WARNING(m) (fprintf(stderr, "Warning: " m "\n"))
#define ERROR(m) (fprintf(stderr, "Error: " m "\n"))

void usage(const char *app)
{
     fprintf(stderr, "USAGE: %s "
	     "-d DEVICE "
	     "-s BAUDRATE "
	     "\n", app);
}

int main(int argc, char *argv[])
{
     char ttydev[MAX_PATH_SIZE];
     speed_t ttyspeed;
     
     int c;
     int intarg;
     memset(ttydev, 0, MAX_PATH_SIZE);
     while ((c = getopt (argc, argv, "d:s:")) != -1) {
	  switch (c) {
	  case 'd' :
	       strncpy(ttydev, optarg, MAX_PATH_SIZE-1);
	       break;
	  case 's' :
	       intarg = atoi(optarg);
	       switch (intarg) {
	       case 9600 : 
		    ttyspeed = B9600;
		    break;
	       case 19200 :
		    ttyspeed = B19200;
		    break;
	       case 38400 :
		    ttyspeed = B38400;
		    break;
	       case 57600 :
		    ttyspeed = B57600;
		    break;
	       case 115200 :
		    ttyspeed = B115200;
		    break;
	       case 230400 :
		    ttyspeed = 230400;
		    break;
	       case 460800 :
		    ttyspeed = B460800;
		    break;
	       default :
		    ttyspeed = B0;
	       }
	       break;
	  case '?':
	  default :
	       usage(argv[0]);
	       exit(-1);
	  }
     }
     if (strlen(ttydev) == 0 || ttyspeed == B0) {
	  usage(argv[0]);
	  exit(-1);
     }
     
     int fdserial = tty_init_raw(ttydev, ttyspeed);
     if (fdserial < 0) {
	  ERROR("Could not init serial device");
	  exit(-1);
     }

     unsigned char pkt[MAX_PKT_SIZE];
     // Time since Unix epoch when last wall-clock timestamp was sent.
     uint64_t tlast = 0; 
     while (1) {
	  ssize_t pktsize = slip_recvpkt(fdserial, pkt, MAX_PKT_SIZE);
	  if (pktsize < 0) {
	       ERROR("Could not receive packet");
	       exit(-1);
	  } else if (pktsize < 3*sizeof(uint16_t)) {
	       // Expecting at least packet header (2*uint16_t) + CRC checksum (uint16_t).
	       WARNING("Short packet (ignoring packet)");
	       continue;
	  }
	  
	  // Received a packet with at least header and CRC sum.
	  uint16_t *crcsum = (uint16_t *) &pkt[pktsize-sizeof(uint16_t)];
	  if (crc_check_crc16ccitt(pkt, pktsize-sizeof(uint16_t), *crcsum) < 0) {
	       WARNING("CRC checksum error (ignoring packet)");
	       continue;
	  }

	  // A tlv element is basically a packet stripped off the trailing CRC sum.
	  // Besides the CRC sum, packets and tlv elements have the same structure (type, length, value).
	  // Therfore, we can simply overlay a tlv structure over the packet. 
	  tlv_t *tlv = (tlv_t *) pkt;
	  if (write_tlv(tlv, stdout) != 0) {
	       ERROR("Error while writing tlv to stdout");
	       exit(-1);
	  }
	  fflush(stdout);
	       
	  // Each second write a wall-clock timestamp to roughly reference samples to wall-clock time.     
	  struct timespec tspec;
	  clock_gettime(CLOCK_REALTIME, &tspec);
	  // Time in nano-seconds since Unix epoch.
	  uint64_t tnow = 1000000000ull*tspec.tv_sec + tspec.tv_nsec;
	  if (tnow - tlast >= 1000000000ull) {
	       tlv_t tlv;
	       tlv.type = TLV_TYPE_WALLCLOCKTIME;
	       tlv.length = sizeof(uint64_t);
	       tlv.value.wallclocktime = tnow;
	       if (write_tlv(&tlv, stdout) != 0) {
		    ERROR("Could not send wallclock timestamp");
		    exit(-1);
	       }
	       fflush(stdout);
	       tlast = tnow;
	  }
     }
     
     return 0;
}
