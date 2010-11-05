/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"
#include "asm.h"        /* for 'here' */
#include "charset.h"
#include "parse.h"
#include "symbol.h"

#include <ctype.h>
/* #include <stdio.h> */
#include <stdlib.h>
#include <string.h>

/* --- The scanner --- */

static const char *the_string, *token_start;

/* Token types */
#define defined_sym 'd'
#define future_sym  'f'
#define number      '0'
#define slash_slash '%'     /* the // operator */
#define eos         'z'     /* end of string */

/* The current token: */
static char token;
static Cell token_value;
static Symbol token_symbol;

/* Scan the next token. */
static void advance(void)
{
    char token_text[max_identifier_length + 1];
    if (isspace(the_string[0]) || the_string[0] == '\0')
	token = eos;
    else if (isalnum(the_string[0])) {  /* symbol or number */
	size_t length;
	const char *start = the_string;
	do {
	    ++the_string;
	} while (isalnum(the_string[0]));
	length = the_string - start;
	if (sizeof token_text - 1 < length) {
	    warn("Atom truncated: %*.*s", length, length, start);
	    length = sizeof token_text - 1;
	}
	memcpy(token_text, start, length);
	token_text[length] = '\0';
	if (strspn(token_text, "0123456789") == length) {   /* a number */
	    token = number;
	    token_value = ulong_to_cell(strtoul(token_text, NULL, 10));
	} else {    /* token is a symbol, either defined or future */
	    token_symbol = string_to_symbol(token_text);
	    token = is_defined(token_symbol) ? defined_sym : future_sym;
	}
    } else if (the_string[0] == '/' && the_string[1] == '/') {
	the_string += 2;
	token = slash_slash;
    } else if (the_string[0] == '"') {      /* a quoted string */
	token = number;
	token_value = zero;
	++the_string;
	{
	    const char *end = strchr(the_string, '"');
	    if (!end) {
		error("Missing '\"'");
		the_string += strlen (the_string);
	    } else {
		unsigned length = end - the_string;
		if (5 < length) {
		    warn("String doesn't fit in a machine word");
		    length = 5;
		}
		{
		    unsigned i;
		    for (i = 0; i < length; ++i)
			token_value = 
			  set_byte(C_char_to_mix(the_string[i]), i+1, token_value);
		}
	    }
	    the_string = end + 1;
	}
    } else 
	token = *the_string++;
}

static void expect(char expected_token)
{
    if (token != expected_token)
        error ("Expected '%c', not '%*.*s'",
	       expected_token,
	       the_string - token_start, the_string - token_start, token_start);
    advance();
}

void setup_scanner(const char *s)
{
    the_string = s;
    advance();
}

/* --- The parser for the operand field --- */

/* future ::= future_sym | '=' expr '=' */
static Flag is_future(void)
{
    return token == future_sym || token == '=';
}

static Cell parse_future(void)
{
    Symbol symbol;
    if (token == future_sym) {
	symbol = token_symbol;
	advance();
    } else {
	Cell value;
	expect('=');
	value = parse_W();
	expect('=');
	symbol = generate_future_sym(value);
    }
    forward_reference(symbol, here);
    return zero;   /* The real value will be filled in when symbol is defined */
}

/* atomic ::= '*' | number | defined */
static Flag is_atomic(void)
{
    return token == '*' || token == number || token == defined_sym;
}

static Cell parse_atomic(void)
{
    Cell result;
    switch (token) {
	case '*':           result = address_to_cell(here); break;
	case number:        result = token_value; break;
	case defined_sym:   result = symbol_value(token_symbol); break;
	default: 
	    error("Expected an atomic value");
    }
    advance();
    return result;
}

/* expr ::= [+-]? atomic ( bin-op atomic )* */
static Flag is_expr(void)
{
    return token == '+' || token == '-' || is_atomic();
}

typedef Cell BinOp(Cell, Cell);

/* The : and // operators: (other operators are defined in cell.c) */

static Cell colon(Cell L, Cell R)
{
    return add(mul(ulong_to_cell(8), L), R);
}

static Cell double_slash(Cell x, Cell y)
{
    Cell quotient, remainder;
    divide(x, zero, y, &quotient, &remainder);
    return quotient;
}

/* (Clobbers -overflow-.) */
static Cell parse_expr(void)
{
    Cell sign = ulong_to_cell(1);
    if (token == '+' || token == '-') {
	if (token == '-')
	    sign = negative(sign);
	advance();
    }
    overflow = false;
    {
	static const char ops[] = "+-*/%:";
	static BinOp * const handlers[] = 
	       { add, sub, mul, slash, double_slash, colon };
	Cell cell = mul(sign, parse_atomic());
	for (;;) {
	    const char *s = strchr(ops, token);
	    if (s) {
		advance();
		cell = handlers[s - ops](cell, parse_atomic());
	    } else {
		if (overflow)
		    warn("Overflow in evaluation of expression");
		return cell;
	    }
	}
    }
}

/* A ::= expr | future | '' */
static Cell parse_A(void)
{
    if (is_future())
	return parse_future();
    else if (is_expr())
	return parse_expr();
    else
	return zero;
}

/* F ::= '(' expr ')' | '' */
static Cell parse_F(Cell default_F)
{
    if (token == '(') {
	Cell result;
	advance();
	result = parse_expr();
	expect(')');
	return result;
    } else
	return default_F;
}

/* I ::= ',' expr | '' */
static Cell parse_I(void)
{
    if (token == ',') {
	advance();
	return parse_expr();
    } else
	return zero;
}

/* operand := A F I */
Cell parse_operand(Flag F_must_be_default, Byte default_F)
{
    Cell A = parse_A();
    Cell I = parse_I();
    Cell F = parse_F((Cell)default_F);
    if (F_must_be_default && F != (Cell)default_F)
	error("Illegal operand to extended-opcode instruction");
    return set_field(A, make_field_spec(0, 2),
	     set_field(I, make_field_spec(3, 3),
	       set_field(F, make_field_spec(4, 4), zero)));
}

/* W ::= expr F ( ',' expr F )* */
Cell parse_W(void)
{
    Cell result = zero;
    for (;;) {
	Cell expr = parse_expr();
	Cell F = parse_F(5);
	assert_valid_field(F);
	result = set_field(expr, (Byte) F, result);
	if (token != ',')
	    break;
	advance();
    }
    return result;
}

void done_parsing(void)
{
    if (token != eos)
	error("Bad syntax in operand field");
}
