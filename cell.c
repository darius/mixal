/* MIX simulator, copyright 1994 by Darius Bacon */ 
/* Corrections to multiply and divide, Nov. 1998, Larry Gately */
#include "mix.h"

#include <stdio.h>

Cell ulong_to_cell(unsigned long n)
{
    if (CELL_MAX < n) {
	warn("Value out of range: %lu", n);
	return CELL_MAX;
    } else
	return n;
}

/* --- Field operations --- */

Byte get_byte(unsigned F, Cell cell)
{
    assert(F != 0);
    return (Byte) field(make_field_spec(F, F), cell);
}

Cell set_byte(Byte value, unsigned F, Cell into)
{
    assert(F != 0);
    return set_field((Cell) value, make_field_spec(F, F), into);
}

static Flag bad_field[64];  /* 64 = number of byte values */
static unsigned shift[64];
static long mask[64];

Cell field(Byte F, Cell cell)
{
    if (bad_field[F])
	error("Bad field spec: %02o", F);
    if (F < 8)      /* if F is of the form (0:R), retain the sign of -cell- */
	return ((cell & mask[F]) >> shift[F]) | sign_bit(cell);
    else
	return (cell & mask[F]) >> shift[F];
}

Cell set_field(Cell value, Byte F, Cell into)
{
    long m = mask[F];
    if (bad_field[F])
	error("Bad field spec: %02o", F);
    if (F < 8)      /* if F is of the form (0:R), use the sign of -value- */
	return (into & ~m & ~the_sign_bit) | ((value << shift[F]) & m) | sign_bit(value);
    else
	return (into & ~m) | ((value << shift[F]) & m);
}

void precompute_field_data(void)
{
    unsigned L, R;
    for (L = 0; L < 8; ++L)
	for (R = 0; R < 8; ++R) {
	    unsigned F = 8 * L + R;
	    bad_field[F] = R < L || 5 < R;
	    if (bad_field[F])
		shift[F] = 0, mask[F] = 0;
	    else {
		unsigned width = R - (L == 0 ? 1 : L) + 1;
		shift[F] = 6 * (5 - R);
		mask[F] = ((1L << (6 * width)) - 1) << shift[F];
	    } 
	}
}

Byte make_field_spec(unsigned L, unsigned R)
{
    unsigned F = 8 * L + R;
    assert(!bad_field[F]);
    return F;
}

void assert_valid_field(Cell field_spec)
{
    if (is_negative(field_spec) 
	 || 64 <= magnitude(field_spec)
	 || bad_field[(unsigned)magnitude(field_spec)]) {
	char buffer[12];
	unparse_cell(buffer, field_spec);
	error("Invalid field specifier: %s", buffer);
    }
}

/* --- Arithmetic --- */

Flag overflow = false;

Cell add(Cell x, Cell y)
{   /* This is kinda clumsy. Should I combine code at the cost (?) of speed */
    /* and functional style? */
    if (sign_bit(x) == sign_bit(y)) {
	long sum = magnitude(x) + magnitude(y);
	long magnitude_of_sum = magnitude(sum);
	if (magnitude_of_sum != sum) overflow = true;
	return sign_bit(x) | magnitude_of_sum;
    } else {
	long diff = magnitude(x) - magnitude(y);
	return diff < 0 ? sign_bit (y) | -diff : sign_bit (x) | diff;
    }
}

Cell sub(Cell x, Cell y)
{
    return add(x, negative(y));     /* should inline this, maybe */
}

void multiply(Cell x, Cell y, Cell *high_word, Cell *low_word)
{
    unsigned long sign = sign_bit(x) ^ sign_bit(y);

    /*
       x = x0 + x1 * 2 ^ 10 + x2 * 2 ^ 20
       y = y0 + y1 * 2 ^ 10 + y2 * 2 ^ 20
       x0, x1, x2, y0, y1, y2 are < 2 ^ 10
    */
    unsigned long x0 = (x & 0x000003FF);
    unsigned long x1 = (x & 0x000FFC00) >> 10;
    unsigned long x2 = (x & 0x3FF00000) >> 20;
    unsigned long y0 = (y & 0x000003FF);
    unsigned long y1 = (y & 0x000FFC00) >> 10;
    unsigned long y2 = (y & 0x3FF00000) >> 20;

    /*
       x * y = partial0 +
               partial1 * 2 ^ 10 +
               partial2 * 2 ^ 20 +
               partial3 * 2 ^ 30 +
               partial4 * 2 ^ 40
       partial0 and partial4 are <     2 ^ 20
       partial1 and partial3 are <     2 ^ 21
       partial2 is               < 3 * 2 ^ 20
    */
    unsigned long partial0 = x0 * y0;
    unsigned long partial1 = x0 * y1 + x1 * y0;
    unsigned long partial2 = x0 * y2 + x1 * y1 + x2 * y0;
    unsigned long partial3 = x1 * y2 + x2 * y1;
    unsigned long partial4 = x2 * y2;

    /*  sum1 has a place value of 1 and is < 2 ^ 32 */
    unsigned long sum1   = partial0 + (partial1 << 10);
    unsigned long carry1 = (sum1 & 0xFFF00000) >> 20;

    /* sum2 has a place value of 2 ^ 20 and is < 2 ^ 32 */
    unsigned long sum2   = partial2 + (partial3 << 10) + carry1;
    unsigned long carry2 = (sum2 & 0xFFF00000) >> 20;

    /* sum3 has a place value of 2 ^ 40 and is < 2 ^ 20 */
    unsigned long sum3   = partial4 + carry2;

    sum1 &= ~0xFFF00000;
    sum2 &= ~0xFFF00000;

    /*
       Now paste the three values back into two.
    */
    *low_word   = sum1 | ((sum2 & 0x000003FF) << 20);
    *low_word  |= sign;
    *high_word  = ((sum2 & 0x000FFC00) >> 10) | (sum3 << 10);
    *high_word |= sign;

}

Cell mul(Cell x, Cell y)
{
    Cell lo, hi;
    multiply(x, y, &hi, &lo);
    if (magnitude(hi) != 0) overflow = true;
    return lo;
}

void divide(Cell n1, Cell n0, Cell d, Cell *quotient, Cell *remainder)
{
    long magn1 = magnitude(n1);
    long magd = magnitude(d);
    if (magd == 0) {
	overflow = true;
	*quotient = *remainder = zero;  /* just so they have -some- valid value */
    } else if (magn1 == 0) {    /* special-cased for speed */
	*quotient = (sign_bit(n1) ^ sign_bit(d)) | (magnitude(n0) / magd);
	*remainder = sign_bit(n1) | (magnitude(n0) % magd);
    } else if (magd <= magn1) {
	overflow = true;
	*quotient = *remainder = zero;
    } else {
	long q = magnitude(n0);
	long r = magn1;
	unsigned i;
	for (i = 30; i != 0; --i) {
	    r <<= 1;
	    if (q & (1L << 29))
		++r;
	    q = (q << 1) & ((1L << 30) - 1);
	    if (magd <= r)
		++q, r -= magd;
	}
	*quotient = (sign_bit(n1) ^ sign_bit(d)) | q;
	*remainder = sign_bit(n1) | r;
    }
}

Cell slash(Cell x, Cell y)      /* the name 'div' is taken... */
{
    Cell quotient, remainder;
    divide(sign_bit(x), x, y, &quotient, &remainder);
    return quotient;
}

/* --- Shift operations --- */

void shift_right(Cell A, Cell X, unsigned long count, Cell *pA, Cell *pX)
{
    *pX = sign_bit(X);
    *pA = sign_bit(A);
    if (count < 5) {
	*pA |= magnitude(A) >> (6 * count);
	*pX |= CELL_MAX & (magnitude(X) >> (6 * count))
			& (A << (30 - 6 * count));
    } else if (count < 10)
	*pX |= magnitude(A) >> (6 * count - 30);
    else
	;
}

void shift_left(Cell A, Cell X, unsigned long count, Cell *pA, Cell *pX)
{
    *pX = sign_bit(X);
    *pA = sign_bit(A);
    if (count < 5) {
	*pX |= CELL_MAX & (X << (6 * count));
	*pA |= CELL_MAX & (A << (6 * count)) 
			& (magnitude(X) >> (30 - 6 * count));
    } else if (count < 10)
	*pA |= CELL_MAX & (X << (6 * count - 30));
    else
	;
}

/* Pre: count < 10 */
void shift_left_circular(Cell A, Cell X, unsigned count, Cell *pA, Cell *pX)
{
    Cell A1 = count < 5 ? A : X;
    Cell X1 = count < 5 ? X : A;
    unsigned c = 6 * (count < 5 ? count : count - 5);
    *pX = sign_bit(X)
	| (CELL_MAX & (X1 << c) & (magnitude(A1) >> (30 - c)));
    *pA = sign_bit(A)
	| (CELL_MAX & (A1 << c) & (magnitude(X1) >> (30 - c)));
}

/* --- Printable representation --- */

void print_cell(Cell cell)
{
    printf("%s%010lo", sign_bit(cell) == 0 ? " " : "-", magnitude(cell));
}

void unparse_cell(char *buffer, Cell cell)
{
    sprintf(buffer, "%s%010lo", sign_bit(cell) == 0 ? " " : "-", magnitude(cell));
}

/* --- Addresses --- */

Cell address_to_cell(Address addr) { return addr; }

Address cell_to_address(Cell cell)
{
    if ((sign_bit(cell) != 0 && magnitude(cell) != 0)
       || memory_size <= magnitude(cell)) {
	char buffer[12];
	unparse_cell(buffer, cell);
	error("Value is not an address: %s", buffer);
	return 0;
    }
    return (Address) magnitude(cell);
}
