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

#include "tty.h"
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int tty_init_raw(const char *dev, speed_t speed)
{
     int fd = open(dev, O_RDWR);
     if (fd < 0)
	  return fd;

     struct termios tios;
     
     if (tcgetattr(fd, &tios) < 0)
	  return -1;

     // ~ECHO: no echoing of characters on terminal
     // ~ICANON: non-canonical mode; no line-wise IO; don't process special characters ERASE, KILL, EOF, NL, EOL, EOL2, CR, REPRINT, STATUS, WERASE 
     // ~IXEXTEN: no processing of characters in extended input mode
     // ~ISIG: no processing of characters creating signals
     tios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
     
     // Disable processing of all special characters that create signals.
     // ~BRKINT: do not process BREAK character
     // ~ICRNL: do not process CR character
     // ~INPCK: no parity check
     // ~ISTRIP: do not strip bit #8
     // ~IXON: do not process stop character (turn off software flow control)
     tios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

     // ~CSIZE: do not define number of bits per byte
     // ~PARENB: no parity check
     tios.c_cflag &= ~(CSIZE | PARENB);
     // Eight bits per byte
     tios.c_cflag |= CS8;
     // Disable output processing
     tios.c_oflag &= ~(OPOST);

     // Block until there is at least one byte available.
     // Note: You might read more than one byte. 
     tios.c_cc[VMIN] = 1;
     tios.c_cc[VTIME] = 0;

     cfsetispeed(&tios, speed);
     cfsetospeed(&tios, speed);
     
     if (tcsetattr(fd, TCSAFLUSH, &tios) < 0)
	  return -1;


     return fd;
}
