#ifndef TTYUTIL_H
#define TTYUTIL_H

#include <termios.h>

int ttyutil_initraw(const char *dev, speed_t speed);

#endif
