/* MIX simulator, copyright 1994 by Darius Bacon */ 
#ifndef IO_H
#define IO_H

#include "mix.h"

void io_init(void);
void io_control(Byte device, Cell argument);
void do_input(Byte device, Cell argument, Address buffer);
void do_output(Byte device, Cell argument, Address buffer);

#endif
