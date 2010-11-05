/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"

#include "asm.h"
#include "driver.h"
#include "parse.h"
#include "symbol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Attributes of the current instruction or directive --- */

static char label[max_identifier_length+1];

static Byte the_C, default_F;
static Flag F_must_be_default;

/* --- Handlers for each type of directive --- */
/* Each must parse the operand field; see assemble_line(). */

static void do_opcode(void)
{
    assemble(set_byte(the_C, 5, parse_operand(F_must_be_default, default_F)));
}

static void do_con(void)
{
    assemble(parse_W());
}

static void do_alf(void)
{
    error("Use CON instead of ALF");
}

static void do_end(void)
{
    set_entry_point(cell_to_address(parse_W()));
}

static void do_equ(void)
{
    define_symbol(string_to_symbol(label), parse_W());
    if (VERBOSE) {
        print_cell(symbol_value(string_to_symbol(label)));
	printf("\n");
    }
}

static void do_orig(void)
{
    here = cell_to_address(parse_W());
}

/* --- The opcode/directive table --- */

typedef const struct OpDef {
    const char *name;
    void (*handler)(void);
    Byte C, F;
    Flag is_extended;
} OpDef;

#define extended true
#define def_opcode(name, C, F, is_ext)  { name, do_opcode, C, F, is_ext }
#define def_directive(name, handler)    { name, handler, 0, 0, 0 }

static OpDef op_table[] = {

#include "ops.inc"

};

/* --- The main driver --- */

static int delta_C;

/* Compare strings a la strcmp, ignoring case, and allowing a `-' in
   pattern to stand for any general register.  
   Pre: pattern is all lowercase and contains at most one `-'.  
   Post: if they match, delta_C is set to the register number of the
   register.  (delta_C unspecified if there was no `-'.) */
static int compare_mnemonics(const char *datum, const char *pattern)
{
    unsigned my_delta_C = 0;
    for (; *pattern; ++pattern, ++datum) {
	char d = tolower(*datum);
	if (*pattern == '-') {
	    static const char legals[] = "a123456x";
	    const char *temp = strchr(legals, d);
	    if (temp)
		my_delta_C = temp - legals;
	    else
		break;
	} else if (*pattern == d)
	    ;
	else
	    break;
    }
    {
	int result = tolower(*datum) - *pattern;
	if (result == 0)
	    delta_C = my_delta_C;
	return result;
    }
}

typedef void (*Handler)(void);

/* I'd prefer something faster than a linear search here.  Maybe have
   op2c.awk expand out the `-' characters so we only need look for a
   string equal to mnemonic. */
static Handler lookup_mnemonic(const char *mnemonic)
{
    unsigned i;
    delta_C = 0;
    for (i = 0; i < sizeof op_table / sizeof op_table[0]; ++i) {
	if (compare_mnemonics(mnemonic, op_table[i].name) == 0) {
	    the_C = op_table[i].C + delta_C;
	    default_F = op_table[i].F;
	    F_must_be_default = op_table[i].is_extended;
	    return op_table[i].handler;
	}
    }
    return NULL;
}

static const char *skip_blanks(const char *s)
{
    return s + strspn(s, " \t");
}

typedef const char *const_char_ptr;

static void eat_identifier(char *buffer, const_char_ptr *s)
{
    const char *scan = buffer;
    size_t size, truncated_size;

    for (scan = *s; !isspace (*scan) && *scan != '\0'; ++scan)
        ;
    size = scan - *s;

    {				/* ensure it's a legal identifier */
	Flag alpha = false, non_alphanum = false;
	for (scan = *s; scan < *s + size; ++scan) {
	    if (isalpha(*scan))
	        alpha = true;
	    else if (!isalnum (*scan))
	        non_alphanum = true;
	}
	if ((!alpha && size != 0) || non_alphanum) {
	    error ("Ill-formed label or mnemonic: %*.*s", size, size, *s);
	    buffer[0] = '\0';
	    *s += size;
	    return;
	}
    }

    if (max_identifier_length < size) {
        warn ("Truncated long label or mnemonic: %*.*s", size, size, *s);
	truncated_size = max_identifier_length;
    } else
        truncated_size = size;

    memcpy(buffer, *s, truncated_size);
    buffer[truncated_size] = '\0';
    *s += size;
}

void assemble_line(const char *line)
{
    char mnemonic[max_identifier_length+1];
    const char *scan = line;
    
    if (*skip_blanks(scan) == '*')      /* the comment character */
	return;

    eat_identifier(label, &scan);       /* eat label if any */
    scan = skip_blanks(scan);
    eat_identifier(mnemonic, &scan);    /* eat mnemonic */
    scan = skip_blanks(scan);
    setup_scanner(scan);                /* prepare to parse operand field */

    {
	Handler handler = lookup_mnemonic(mnemonic);
	if (handler != do_equ && label[0] != '\0')
	    define_symbol(string_to_symbol(label), address_to_cell(here));
	if (handler) {
	    handler();
	    done_parsing();  /* this goes here since *all* handlers parse an operand field */
	} else if (mnemonic[0] != '\0')
	    error("Unknown instruction: %s", mnemonic);
	else
	    ;
    }
}
