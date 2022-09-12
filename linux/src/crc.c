#include "crc.h"
#include "checksum.h"

int crc_check_crc16ccitt(uint8_t *buffer, size_t buffersize, uint16_t expected_crcsum)
{
     // Calculate 16 bit CRC-CCITT sum (polynomial 0x1021) with start value 0x0000.
     uint16_t crcsum = crc_xmodem(buffer, buffersize);

     if (crcsum != expected_crcsum)
	  return -1;
     else
	  return 0;
}
