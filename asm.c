/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"

#include "asm.h"
#include "run.h"    /* for memory[] */

/* --- The assembly buffer --- */

Address here = 0;

Cell asm_fetch_field(Address address, unsigned L, unsigned R)
{
    assert(address < memory_size);
    return field(make_field_spec(L, R), memory[address]);
}

#include <stdio.h>

void asm_store_field(Address address, unsigned L, unsigned R, Cell cell)
{
    assert(address < memory_size);
    if (VERBOSE) {
	char temp[12];
	unparse_cell(temp, cell);
	printf("%4o(%u,%u): %s\n", address, L, R, temp);
    }
    memory[address] = set_field(cell, make_field_spec(L, R), memory[address]);
}

void assemble(Cell cell)
{
    if (VERBOSE) {
        char temp[12];
	unparse_cell(temp, cell);
	printf("%4o: %s\n", here, temp);
    }
    if (here < memory_size)
	memory[here] = cell;
    else
	error("Address out of range");
    ++here;
}

Address entry_point;

void set_entry_point(Address address)
{
    assert(address < memory_size);
    if (VERBOSE)
        printf("entry point: %4o\n", address);
    entry_point = address;
}
