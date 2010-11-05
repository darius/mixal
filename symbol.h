/* MIX simulator, copyright 1994 by Darius Bacon */ 
#ifndef SYMBOL_H
#define SYMBOL_H

#include "mix.h"

typedef struct AddressList {
    struct AddressList *next;
    Address address;
} *AddressList;
    
/* --- Symbols --- */

typedef struct Symbol {
    struct Symbol *next;
    char *name;
    Flag defined;
    Cell cell;
    AddressList references;
} *Symbol;

/*
 * If sym->defined, then sym->cell is the symbol's value.
 * If !sym->defined, then sym->references is the set of
 * references to sym.  When the symbol is finally defined, 
 * the address field (0:2) of each cell in sym->references
 * gets set to the defined value.
 */

/* Return the symbol named; 
   if it doesn't exist, create it, initially undefined. */
Symbol string_to_symbol(const char *name);

Flag is_defined(Symbol symbol);

/* Pre: is_defined(symbol) */
Cell symbol_value(Symbol symbol);

/* Pre: !is_defined(symbol)
   Post: address has been adjoined to symbol's references. */
void forward_reference(Symbol symbol, Address address);

/* Post: symbol_value(symbol) == cell
    If symbol was undefined, then all refs to it are resolved.
    If symbol was already defined, and non-local, then gives a warning. */
void define_symbol(Symbol symbol, Cell cell);

/* Return a symbol, distinct from all symbols defined in the source program,
   whose value will be set, when resolve_generated_futures() is called,
   to the address of a cell which contains value. */
Symbol generate_future_sym(Cell value);

void resolve_generated_futures(void);

#endif
