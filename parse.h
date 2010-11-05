/* MIX simulator, copyright 1994 by Darius Bacon */ 
#ifndef PARSE_H
#define PARSE_H

#include "mix.h"

/* --- The scanner --- */

void setup_scanner(const char *s);

/* --- The parser --- */

Cell parse_operand(Flag F_must_be_default, Byte default_F);
Cell parse_W(void);

void done_parsing(void);

#endif
