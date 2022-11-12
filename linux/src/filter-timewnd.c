#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tlv.h"

#define MAX_TIMESTR_LEN 1000

#define WARNING(m) (fprintf(stderr, "Warning: " m "\n"))
#define ERROR(m) (fprintf(stderr, "Error: " m "\n"))

enum State {
     before,
     within,
     after};

void usage(const char *app)
{
     fprintf(stderr, "USAGE: %s \n"
	     "-l : time specified as local time\n"
	     "-u : time specified as UTC\n"
	     "-s STARTTIME : include everything after and including this time (format see below)\n"
	     "-e ENDTIME : include everything before and including this time (format see below)\n"
	     "\n"
	     "Time format (quoted string): year-month-day hour:minute:second\n"
	     "year: yyyy \t month: 1-12 \t day: 1-31 \t hour: 0-23 \t minute: 0-59 \t second: 0-59 \n",
	     app);
}

int main(int argc, char *argv[])
{
     // If set to false, use UTC (default). 
     bool uselocaltime = false;
     char starttime_arg[MAX_TIMESTR_LEN];
     char endtime_arg[MAX_TIMESTR_LEN];
     time_t starttime; // in seconds since Unix epoch (UTC)
     time_t endtime;   // in seconds since Unix epoch (UTC)
     
     memset(starttime_arg, 0, MAX_TIMESTR_LEN);
     memset(endtime_arg, 0, MAX_TIMESTR_LEN);	       
     int c;
     while ((c = getopt (argc, argv, "lus:e:")) != -1) {
	  switch (c) {
	  case 'l' :
	       uselocaltime = true;
	       break;
	  case 'u' :
	       uselocaltime = false;
	       break;
	  case 's' :
	       strncpy(starttime_arg, optarg, MAX_TIMESTR_LEN-1);
	       break;
	  case 'e' :
	       strncpy(endtime_arg, optarg, MAX_TIMESTR_LEN-1);
	       break;
	  case '?':
	  default :
	       usage(argv[0]);
	       exit(-1);
	  }
     }


     if (strlen(starttime_arg) == 0) {
	  usage(argv[0]);
	  exit(-1);
     }

     if (strlen(endtime_arg) == 0) {
	  usage(argv[0]);
	  exit(-1);
     }


     struct tm t;
     if (strptime(starttime_arg, "%Y-%m-%d %H:%M:%S", &t) == NULL) {
	  fprintf(stderr, "Could not parse start time\n");
	  exit(-1);
     }
     // The result of both, mktime() and timegm(), is time since epoch in UTC.
     if (uselocaltime)
	  starttime = mktime(&t); // tm defined as local time
     else
	  starttime = timegm(&t); // tm defined as UTC
     

     if (strptime(endtime_arg, "%Y-%m-%d %H:%M:%S", &t) == NULL) {
	  fprintf(stderr, "Could not parse start time\n");
	  exit(-1);
     }
     if (uselocaltime)
	  endtime = mktime(&t); // convert given local time to UTC
     else
	  endtime = timegm(&t); // given time is UTC (just convert to time_t)

     // The POSIX standard defines that time_t (starttime, endtime) is time in
     // seconds since the Unix epoch. Therefore, we convert it to time in
     // nano-seconds since Unix expoch as follows.
     uint64_t tstartns = 1000000000ull*starttime;
     uint64_t tendns = 1000000000ull*endtime;
		    
     tlv_t tlv;
     enum State state = before;
     while (state != after) {
	  if (read_tlv(&tlv, stdin) < 0) {
	       if (feof(stdin)) {
		    break;
	       } else if (ferror(stdin)) {
		    ERROR("Could not read TLV element from stdin");
		    exit(-1);
	       } else {
		    ERROR("Could not read TLV element from stdin (corrupt file)");
		    exit(-1);
	       }
	  }

	  switch (state) {
	  case before :
	       if (tlv.type == TLV_TYPE_WALLCLOCKTIME) {
		    if (tlv.value.wallclocktime >= tstartns) {
			 // Entered time window.
			 state = within;
		    }
		    if (tlv.value.wallclocktime > tendns) {
			 // ... and left time window.
			 state = after;
		    }
		    if (state == within) {
			 // Pass through packet within time window.
			 if (write_tlv(&tlv, stdout) < 0) {
			      ERROR("Could not write packet to stdout");
			      exit(-1);
			 }
		    }
	       } else {
		    // Ignore all packets before start time.
	       }
	       break;
	  case within :
	       if (tlv.type == TLV_TYPE_WALLCLOCKTIME) {
		    if (tlv.value.wallclocktime > tendns) {
			 // Left time window.
			 state = after;
		    } else {
			 // Still within time window.
			 if (write_tlv(&tlv, stdout) < 0) {
			      ERROR("Could not write packet to stdout");
			      exit(-1);
			 }
		    }
	       } else {
		    // Pass-through packet within time window.
		    if (write_tlv(&tlv, stdout) < 0) {
			 ERROR("Could not write packet to stdout");
			 exit(-1);
		    }	    
	       }
	       break;
	  case after :
	       // Should have left while loop already.
	       ERROR("Invalid state\n");
	       exit(-1);
	  }
     }

     return 0;
}
