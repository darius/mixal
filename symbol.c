/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"
#include "asm.h"        /* this is a sign of bad factoring */
#include "symbol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Here's the theory on forward references.
 
We need to handle 3 sorts of references: ordinary symbols, local symbols
like 7B and 7F, and implicit constants like =123=.  First let's discuss
ordinary symbols.  Any symbol that's been referred to but not yet defined
has a list of all locations, in the object program built so far, that 
refer to it.  That list is added to by the forward_reference() function.
When the symbol is defined, we fix up all the references to it, using the 
list.  After assembly is done, we check if there are any unresolved 
references, in resolve_generated_futures() (whose main job I'll get to 
shortly).  A minor problem: we don't hold onto the source-line number of a
reference, so if it's unresolved we get a misleading error message.
 
The forward_reference() function returns a meaningless value just for
historical reasons: originally the list of unresolved references for a
symbol was threaded through the simulated memory locations themselves,
and so forward_reference() had to return the value to put in the 
latest referencing location to maintain the list.  Now that I've come
to my senses, I suppose I should get rid of the return value.
 
Local symbols work just like regular ones, except that a reference to,
say, 7B, gets the value defined in the latest definition of 7H, and
similarly a reference to 7F is always a forward ref to the next definition
of 7H.  This can be arranged by having define_symbol() adjust 7B and 7F
appropriately when it sees 7H.
 
We handle implicit constants like =123= by generating a unique symbol for
each value that occurs in such a constant.  (It's of the form "=123", which
can't conflict with ordinary or local symbols.)  Then a reference to such a
constant is simply a forward reference to the generated symbol.  After 
assembly is complete resolve_generated_futures() resolves all the generated
symbols with appropriate addresses.  Note that this scheme uniquifies the 
constant references for free.
*/

/* Pre: size != 0 */
static void *allot(size_t size)
{
    void *result = malloc(size);
    if (!result)
	fatal_error("Out of heap space");
    return result;
}

static int stricmp(const char *s, const char *t)
{
    while (*s && *t)
	if (tolower(*s) != tolower(*t))
	    return 1;
	else
	    s++, t++;
    return tolower(*s) != tolower(*t);
}

/* --- The symbol table --- */

static Symbol symbol_table[17];

/* Apply proc to each symbol in the symbol table. */
static void for_each_symbol(void (*proc)(Symbol))
{
    unsigned i;
    for (i = 0; i < sizeof symbol_table / sizeof symbol_table[0]; ++i) {
	Symbol s = symbol_table[i];
	for (; s; s = s->next)
	    proc(s);
    }
}

static unsigned hash(const char *s)
{
    unsigned h = 0;
    for (; *s; s++)
	h = (h << 5) - h + toupper(*s);
    return h % (sizeof symbol_table / sizeof symbol_table[0]);
}

Symbol string_to_symbol(const char *name)
{
    Symbol *bucket = symbol_table + hash(name);
    Symbol p = *bucket;
    for (; p; p = p->next)
	if (stricmp(name, p->name) == 0)
	    return p;
    {                   /* Symbol doesn't exist yet. */
	Symbol result = allot(sizeof(struct Symbol));
	strcpy(result->name = allot(strlen(name) + 1), name);
	result->defined = false;
	result->references = NULL;
	result->next = *bucket;
	result->cell = zero;
	*bucket = result;
	return result;
    }
}

Flag is_defined(Symbol symbol)  { return symbol->defined; }

Cell symbol_value(Symbol symbol)
{
    assert(symbol->defined);
    return symbol->cell;
}

void forward_reference(Symbol symbol, Address address)
{
    assert(!symbol->defined);
    {
	AddressList refs = allot(sizeof(struct AddressList));
	refs->address = address;
	refs->next = symbol->references;
	symbol->references = refs;
    }
}

/* Pre: !is_defined(symbol) 
   Set the address field of each cell in symbol's set of references 
   to definition.  Frees the set of references. */
static void resolve_references(Symbol symbol, Cell definition)
{
    AddressList refs, rest;
    assert(!symbol->defined);
    for (refs = symbol->references; refs; refs = rest) {
	asm_store_field(refs->address, 0, 2, definition);
	rest = refs->next;
	free(refs);
    }
}

void define_symbol(Symbol symbol, Cell cell)
{
    {   /* handle local symbols */
	const char *name = symbol->name;
	if (isdigit(name[0]) && strlen(name) == 2) {
	    char c = toupper(name[1]);
	    if (c == 'F' || c == 'B') {
		error("Definition of local ref: %s", name);
		return;
	    } else if (c == 'H') {
		char new_name[3];
		Symbol nB, nF;
		strcpy(new_name, name);
		new_name[1] = 'B'; nB = string_to_symbol(new_name);
		nB->defined = true, nB->cell = cell;
		new_name[1] = 'F'; nF = string_to_symbol(new_name);
		resolve_references(nF, cell);
		nF->defined = false, nF->references = NULL;
		return;
	    } else
		;       /* not a local; fall through */
	}
    }
    if (symbol->defined)
	warn("'%s' multiply-defined", symbol->name);
    else
	resolve_references(symbol, cell);
    symbol->defined = true, symbol->cell = cell;
}

Symbol generate_future_sym(Cell value)
{
    char name[12];
    sprintf(name, "=%ld", cell_to_long(value));
    return string_to_symbol(name);
}

static void resolve_if_dangling(Symbol symbol)
{
    if (symbol->defined)
	return;
    if (symbol->name[0] == '=') {   /* it's a generated symbol */
	define_symbol(symbol, address_to_cell(here));
	assemble(long_to_cell(atol(symbol->name + 1)));
    } else {    /* it's a user symbol */
	if (symbol->references)
	    error("Unresolved reference: %s", symbol->name);
	      /* it would be nice to print the reference list, too */
    }
}

void resolve_generated_futures(void)
{
    for_each_symbol(resolve_if_dangling);
}
