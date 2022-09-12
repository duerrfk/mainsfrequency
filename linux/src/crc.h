#ifndef CRC_H
#define CRC_H

#include <stdint.h>
#include <sys/types.h>

int crc_check_crc16ccitt(uint8_t *buffer, size_t buffersize, uint16_t expected_crcsum);
     
#endif
