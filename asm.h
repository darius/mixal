/* MIX simulator, copyright 1994 by Darius Bacon */ 
#ifndef ASM_H
#define ASM_H

#include "cell.h"

/* --- The assembly buffer --- */

extern Address here;

Cell asm_fetch_field(Address address, unsigned L, unsigned R);
void asm_store_field(Address address, unsigned L, unsigned R, Cell cell);

extern Address entry_point;
void set_entry_point(Address address);

void assemble(Cell cell);

#endif
