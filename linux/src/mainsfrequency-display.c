#include "tty.h"
#include "slip.h"
#include "crc.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PATH_SIZE 1000

#define MAX_PKT_SIZE 1500

// Different packet types to transport different data.
#define PKTTYPE_SAMPLES 0 /* samples packet */
#define PKTTYPE_ONEPPS 1  /* 1-pps calibration packet */
#define PKTTYPE_WALLCLOCK 2  /* packet carrying wallclock timestamp as uint64_t representing nanoseconds since Epoch */

// Samples are taken at a nominal frequency of MCLK/2 = 42 MHz.
// (MCLK = master clock frequency of Arduino Due)
#define F_CLK (84000000/2)

#define WARNING(m) (fprintf(stderr, "\nWarning: " m "\n"))

typedef struct __attribute__((__packed__)) {
     uint16_t type;
     uint16_t payload_length;
     unsigned char payload[MAX_PKT_SIZE - 2*sizeof(uint16_t)];
} pkt_t;

unsigned int nwin = 0;
double sumwin = 0.0;
uint32_t f_clk = F_CLK;
double avg = 0.0;
FILE *outfile = NULL;

void usage(const char *app)
{
     fprintf(stderr, "USAGE: %s "
	     "-d DEVICE "
	     "-s BAUDRATE "
	     "-f OUFILE "
	     "\n", app);
}

void print()
{
     printf("\rf_mains = %f Hz \t f_clock = %u Hz", avg, f_clk);
     fflush(stdout);
}

void write_pkt(const pkt_t *pkt, FILE *of)
{
     size_t len = 2*sizeof(uint16_t) + pkt->payload_length;

     size_t nwritten = fwrite(pkt, 1, len, of);
     if (nwritten != len)
	  perror("Error while writing packet");
}

void process_pkt_samples(const pkt_t *pkt)
{
     uint32_t *samples = (uint32_t *) pkt->payload;
     size_t nsamples = pkt->payload_length/sizeof(uint32_t);

     for (unsigned int i = 0; i < nsamples; i++) {
	  double freq = (double) f_clk / samples[i];
	  sumwin += freq;
	  nwin++;
	  if (nwin == 50) {
	       avg = sumwin/50.0;
	       print();
	       sumwin = 0.0;
	       nwin = 0;
	  }
     }

     write_pkt(pkt, outfile); 
}

void process_pkt_onepps(const pkt_t *pkt)
{
     f_clk = *((uint32_t *) pkt->payload);
     print();

     write_pkt(pkt, outfile);

     // Each second write a wall-clock timestamp to roughly reference samples to wall-clock time.
     
     pkt_t pkt_wallclock;
     pkt_wallclock.type = PKTTYPE_WALLCLOCK;
     pkt_wallclock.payload_length = sizeof(uint64_t);
     uint64_t *tns = (uint64_t *) pkt_wallclock.payload;

     struct timespec spec;
     clock_gettime(CLOCK_REALTIME, &spec);
     // Converting time to 64 bit value in nanoseconds works for several
     // hundreds of years after Epoch.
     *tns = 1000000000ull*spec.tv_sec + spec.tv_nsec;

     write_pkt(&pkt_wallclock, outfile);
}

void process_pkt(const pkt_t *pkt)
{
     switch (pkt->type) {
     case PKTTYPE_SAMPLES :
	  process_pkt_samples(pkt);
	  break;
     case PKTTYPE_ONEPPS :
	  process_pkt_onepps(pkt);
	  break;
     default :
	  fprintf(stderr, "%u\n", pkt->type);
	  WARNING("Unknown packet type (ignoring packet)");
     }
}

int main(int argc, char *argv[])
{
     char ttydev[MAX_PATH_SIZE];
     char ofpath[MAX_PATH_SIZE];
     speed_t ttyspeed;
     
     int c;
     int intarg;
     memset(ttydev, 0, MAX_PATH_SIZE);
     memset(ofpath, 0, MAX_PATH_SIZE);
     while ((c = getopt (argc, argv, "d:s:f:")) != -1) {
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
	  case 'f' :
	       strncpy(ofpath, optarg, MAX_PATH_SIZE-1);
	       break;
	  case '?':
	  default :
	       usage(argv[0]);
	       exit(-1);
	  }
     }
     if (strlen(ttydev) == 0 || strlen(ofpath) == 0 || ttyspeed == B0) {
	  usage(argv[0]);
	  exit(-1);
     }
     
     int fdserial = tty_init_raw(ttydev, ttyspeed);
     if (fdserial < 0) {
	  perror("Could not init serial device");
	  exit(-1);
     }

     outfile = fopen(ofpath, "w+");
     if (outfile == NULL) {
	  perror("Could not open output file");
	  exit(-1);
     }
     
     unsigned char pktbuffer[MAX_PKT_SIZE];
     while (1) {
	  ssize_t pktsize = slip_recvpkt(fdserial, pktbuffer, MAX_PKT_SIZE);
	  if (pktsize < 0) {
	       perror("Could not receive packet");
	       exit(-1);
	  } else if (pktsize < 3*sizeof(uint16_t)) {
	       // Expecting at least packet header (2*uint16_t) + CRC checksum (1*uint16_t).
	       WARNING("Short packet (ignoring packet)");
	       continue;
	  }
	  
	  // Received a packet with at least header and CRC sum.
	  uint16_t *crcsum = (uint16_t *) &pktbuffer[pktsize-sizeof(uint16_t)];
	  if (crc_check_crc16ccitt(pktbuffer, pktsize-sizeof(uint16_t), *crcsum) < 0) {
	       WARNING("CRC checksum error (ignoring packet)");
	       continue;
	  }

	  pkt_t *pkt = (pkt_t *) pktbuffer;
	  process_pkt(pkt);
     }
     
     return 0;
}
