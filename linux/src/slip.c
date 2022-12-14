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

#include "slip.h"
#include <unistd.h>

#define END             0300    /* indicates end of packet */
#define ESC             0333    /* indicates byte stuffing */
#define ESC_END         0334    /* ESC ESC_END means END data byte */
#define ESC_ESC         0335    /* ESC ESC_ESC means ESC data byte */

#define BUFFER_SIZE 1000

static int get_next_byte(int fd, unsigned char *buffer, size_t buffersize, size_t *len, size_t *pos)
{
     if (*len == *pos) {
	  // All bytes from buffer have been consumed -> refill buffer
	  ssize_t nread = read(fd, buffer, BUFFER_SIZE);
	  if (nread < 0)
	       return -1;
	  else if (nread == 0)
	       return -1; // EOF

	  // We have read at least one byte into the buffer.
	  
	  *len = nread;
	  *pos = 0;
     }

     // Buffer is not empty here.

     int c = buffer[(*pos)++];
     
     return c;
}

ssize_t slip_recvpkt(int fd, void *pktbuffer, size_t pktbuffer_size)
{
     // To avoid reading single bytes from fd, we introduce a buffer. 
     // The state of this buffer is kept between calls to this function.
     static unsigned char buffer[BUFFER_SIZE];
     static size_t len = 0;
     static size_t pos = 0;

     unsigned char *pktbytes = (unsigned char *) pktbuffer;
     
     int c;
     size_t nrcvd = 0;
     while (1) {
	  c = get_next_byte(fd, buffer, BUFFER_SIZE, &len, &pos);
	  if (c < 0)
	       return -1;
	  
	  switch (c) {
	       case END :
		    // If it is an END character then we are done with the packet.
		    // If there is no data in the packet, ignore it and start reading next packet.
		    // Empty packets can happen due to line noise or using the protocol variant
		    // that also sends an END character at the *beginning* of the packet.
		    if (nrcvd)
			 return nrcvd;
		    else
			 break;
	       case ESC:
		    // Wait and get another character and then figure out
		    // what to store in the packet based on that.
		    c = get_next_byte(fd, buffer, BUFFER_SIZE, &len, &pos);
		    if (c < 0)
			 return -1;
		    
		    // If "c" is not one of these two, then we have a protocol violation.
		    // The best bet seems to be to leave the byte alone and just stuff it
		    // into the packet.
		    switch (c) {
		    case ESC_END:
			 c = END;
			 break;
		    case ESC_ESC:
			 c = ESC;
			 break;
		    }
		    
		    // Here we deliberately fall into the default handler and let
		    // it store the character for us.
	       default:
		    if (nrcvd < pktbuffer_size)
			 pktbytes[nrcvd++] = c;
	  }
     }
}
