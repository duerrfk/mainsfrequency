#include "tty.h"
#include "slip.h"
#include "crc.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_TTYDEV_SIZE 1000

#define MAX_PKT_SIZE 9000

void usage(const char *app)
{
     fprintf(stderr, "USAGE: %s "
	     "-d DEVICE "
	     "-s BAUDRATE "
	     "\n", app);
}

int main(int argc, char *argv[])
{
     char ttydev[MAX_TTYDEV_SIZE];
     speed_t ttyspeed;
     
     char c;
     int intarg;
     memset(ttydev, 0, MAX_TTYDEV_SIZE);
     while ((c = getopt (argc, argv, "d:s:")) != -1) {
	  switch (c) {
	  case 'd' :
	       strncpy(ttydev, optarg, MAX_TTYDEV_SIZE-1);
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
	  perror("Could not init serial device");
	  exit(-1);
     }

     unsigned char pktbuffer[MAX_PKT_SIZE];
     while (1) {
	  ssize_t pktsize = slip_recvpkt(fdserial, pktbuffer, MAX_PKT_SIZE);
	  if (pktsize < 0) {
	       perror("Could not receive packet");
	       exit(-1);
	  } else if (pktsize <= 2) {
	       fprintf(stderr, "Warning: ignoring empty packet\n");
	       continue;
	  }
	  
	  // Received a packet with more than two bytes, i.e., including at leat one byte data and 16 bit CRC checksum.
	  uint16_t *crcsum = (uint16_t *) &pktbuffer[pktsize-sizeof(uint16_t)];
	  if (crc_check_crc16ccitt(pktbuffer, pktsize-sizeof(uint16_t), *crcsum) < 0) {
	       fprintf(stderr, "Warning: CRC checksum error (ignoring packet)\n");
	       continue;
	  }

	  // TODO
     }
     
     return 0;
}
