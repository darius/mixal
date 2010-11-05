/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"

#ifndef CELL_H
#define CELL_H

/* --- Call this before anything else --- */
void precompute_field_data(void);

/*
 * MIX values ---
 *
 * Currently represented as a long.  (A long must have >= 31 bits.)
 * Bit 30 is the sign, higher bits are 0, and bits 0-29 are the magnitude.
 * (If we made the high bit the sign, would it be a little faster to check?)
 * Each MIX 'byte' is a 6-bit substring of the magnitude.
 *
 * Other reps which might make sense:
 *   array of bytes
 *   pair (sign, magnitude)
 *   1's-complement??? 
 *       (a kind of halfway house between 2's-complement and sign/magnitude)
 *       (alas, field operations would be more work...)
 * Unfortunately, in several places I've made the convenient assumption that
 * the magnitude of a MIX cell can be converted to a C long int; that would 
 * be wrong for, e.g., a decimal MIX machine if a long int is only 32 bits.
 */

typedef long Cell;
typedef unsigned Byte;  /* 0..63 */

/* These are to allow symbol.c to convert a Cell to or from a unique string
   representation.  We could just as easily use unparse() if we defined a
   complementary parse() function. */
#define cell_to_long(c) (c)
#define long_to_cell(l) (l)

/* Largest possible magnitude of a Cell */
#define CELL_MAX    ((1L << 30) - 1)
/* Largest possible magnitude of an index register (i.e., 2 bytes) */
#define INDEX_MAX   ((1L << 12) - 1)

#define zero ((Cell) 0)

#define the_sign_bit    (1L << 30)

#define negative(cell)      ( (cell) ^ the_sign_bit )
#define sign_bit(cell)      ( (cell) & the_sign_bit )
#define magnitude(cell)     ( (cell) & (the_sign_bit - 1) )

#define is_negative(cell)   ( sign_bit(cell) != 0 )

Cell ulong_to_cell(unsigned long n);

Byte make_field_spec(unsigned L, unsigned R);
void assert_valid_field(Cell field_spec);

/*** should separate into fast/safe versions */
Cell field(Byte field_spec, Cell cell);
Cell set_field(Cell value, Byte field_spec, Cell into);

Byte get_byte(unsigned field, Cell cell);
Cell set_byte(Byte value, unsigned field, Cell into);

extern Flag overflow;

Cell add(Cell x, Cell y);
Cell sub(Cell x, Cell y);

void multiply(Cell x, Cell y, Cell *high_word, Cell *low_word);
Cell mul(Cell x, Cell y);

void divide(Cell n1, Cell n0, Cell d, Cell *quotient, Cell *remainder);
Cell slash(Cell x, Cell y);     /* the name 'div' is taken... */

void shift_left(Cell A, Cell X, unsigned long count, Cell *pA, Cell *pX);
void shift_right(Cell A, Cell X, unsigned long count, Cell *pA, Cell *pX);
void shift_left_circular(Cell A, Cell X, unsigned count, Cell *pA, Cell *pX);

/* Break a cell into its component fields. */
#define destructure_cell(cell, A, I, F, C)  \
    do {                                    \
	Cell temp_ = (cell);                \
	Cell sign_ = sign_bit(temp_);       \
	C = temp_ & 63;                     \
	F = (temp_ >>= 6) & 63;             \
	I = (temp_ >>= 6) & 63;             \
	A = sign_ | (4095 & (temp_ >> 6));  \
    } while (0)

/* --- Printable representation --- */
void print_cell(Cell cell);
void unparse_cell(char *buffer, Cell cell); /* Pre: 12 <= sizeof(buffer) */

/* --- Addresses --- */
typedef unsigned Address;

Cell address_to_cell(Address addr);
Address cell_to_address(Cell cell);

#endif
