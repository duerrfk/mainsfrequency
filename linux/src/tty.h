#ifndef TTY_H
#define TTY_H

#include <termios.h>

int tty_init_raw(const char *dev, speed_t speed);

#endif
