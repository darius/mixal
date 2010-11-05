/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"
#include "asm.h"    /* for entry_point */
#include "charset.h"
#include "io.h"
#include "run.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void stop(const char *message, va_list args)
{
    fprintf(stderr, "RUNTIME ERROR: ");
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    print_CPU_state();
    exit(1);
}

/* --- Execution statistics --- */

static unsigned long elapsed_time = 0;      /* in Tyme units */

/* --- The CPU state --- */

Cell memory[memory_size];

#define A 0
#define X 7
#define J 8
static Cell r[10];      /* the registers; except that r[9] == zero. */

static int comparison_indicator;    /* the overflow toggle is defined in cell.c */
static Address pc;      /* the program counter */

void set_initial_state(void)
{
    overflow = false;
    comparison_indicator = 0;
    {
	unsigned i;
	for (i = 0; i < 10; ++i)
	    r[i] = zero;
    }
    pc = entry_point;       /*** need to check for no entry point */
}

void print_CPU_state(void)
{
    printf ("A:");
    print_cell (r[A]);
    printf ("\t");
    {				/* Print the index registers: */
      unsigned i;
      for (i = 1; i <= 6; ++i)
	  printf ("I%u:%s%04lo  ",
		  i, is_negative (r[i]) ? "-" : " ", magnitude (r[i]));
    }
    printf ("\nX:");
    print_cell (r[X]);
    printf ("\t J: %04lo", magnitude (r[J]));	/* (it's always nonnegative) */
    printf ("  PC: %04o", pc);
    printf ("  Flags: %-7s %-8s",
	    comparison_indicator < 0 ? "less" :
	      comparison_indicator == 0 ? "equal" : "greater",
	    overflow ? "overflow" : "");
    printf (" %11lu elapsed\n", elapsed_time);
}

/* --- The interpreter --- */

/* --- I've followed Knuth's MIX interpreter quite closely. */

static jmp_buf escape_k;    /* continuation to escape from interpreter */

/* C, F, M, and V as defined in Knuth: */

static Byte C;
static Byte F;
static Cell M;

static Cell get_V(void)
{
    return field(F, memory[cell_to_address(M)]);
}

/* do_foo performs the action of instruction type foo. */

static void do_nop(void)    { }

static void do_add(void)    { r[A] = add(r[A], get_V()); }
static void do_sub(void)    { r[A] = sub(r[A], get_V()); }

static void do_mul(void)    { multiply(r[A], get_V(), &r[A], &r[X]); }
static void do_div(void)    { divide(r[A], r[X], get_V(), &r[A], &r[X]); }

static void do_special(void)
{
    switch (F) {
	case 0: { /* NUM */
	    unsigned i;
	    Cell num = zero;
	    Cell ten = ulong_to_cell(10);
	    for (i = 1; i <= 5; ++i)
		num = add(mul(ten, num), (Cell)(get_byte(i, r[A]) % 10));
	    for (i = 1; i <= 5; ++i)
		num = add(mul(ten, num), (Cell)(get_byte(i, r[X]) % 10));
	    r[A] = is_negative(r[A]) ? negative(num) : num;
	    break;
	}
	case 1: { /* CHAR */
	    unsigned long num = magnitude(r[A]);
	    unsigned z = (unsigned) C_char_to_mix('0');
	    unsigned i;
	    for (i = 5; 0 < i; --i, num /= 10)
		r[X] = set_byte((Byte) (z + num % 10), i, r[X]);
	    for (i = 5; 0 < i; --i, num /= 10)
		r[A] = set_byte((Byte) (z + num % 10), i, r[A]);
	    break;
	}
	case 2: /* HLT */
	    longjmp(escape_k, 1);
	default: error("Unknown extended opcode");
    }
}

static void do_shift(void)
{
    Cell ignore;
    unsigned long count = magnitude(M);
    if (is_negative(M) && count != 0)
	error("Negative shift count");
    switch (F) {
	case 0: /* SLA */
	    shift_left(zero, r[A], count, &ignore, &r[A]);
	    break;
	case 1: /* SRA */
	    shift_right(r[A], zero, count, &r[A], &ignore);
	    break;
	case 2: /* SLAX */
	    shift_left(r[A], r[X], count, &r[A], &r[X]);
	    break;
	case 3: /* SRAX */
	    shift_right(r[A], r[X], count, &r[A], &r[X]);
	    break;
	case 4: /* SLC  */
	    shift_left_circular(r[A], r[X], (unsigned)(count % 10), &r[A], &r[X]);
	    break;
	case 5: { /* SRC */
	    unsigned c = (10 - count % 10) % 10;    /* -count modulo 10 */
	    shift_left_circular(r[A], r[X], c, &r[A], &r[X]);
	    break;
	}
	default: error("Unknown extended opcode");
    }
}

static void do_move(void)
{
    Address from = cell_to_address(M);
    Address to = cell_to_address(r[1]);
    unsigned count = F;
    for (; count != 0; --count) {
	if (memory_size <= from + count || memory_size <= to + count)
	    error("Address out of range");
	memory[to + count] = memory[from + count];
	elapsed_time += 2;
    }
    r[1] = address_to_cell(to + count);
}

static void do_lda(void)    { r[A] = get_V(); }
static void do_ldx(void)    { r[X] = get_V(); }
static void do_ldi(void) { 
    Cell cell = get_V();
    if (INDEX_MAX < magnitude(cell))
	error("Magnitude too large for index register: %10o", magnitude(cell));
    r[C & 7] = cell; 
}

static void do_ldan(void)   { r[A] = negative(get_V()); }
static void do_ldxn(void)   { r[X] = negative(get_V()); }
static void do_ldin(void) {
    Cell cell = get_V();
    if (INDEX_MAX < magnitude(cell))
	error("Magnitude too large for index register: %10o", magnitude(cell));
    r[C & 7] = negative(cell);
}

static void do_store(void)
{
    Address a = cell_to_address(M);
    memory[a] = set_field(r[C-24], F, memory[a]);
}

static void jump(void)
{
    r[J] = address_to_cell(pc);
    pc = cell_to_address(M);
}

static void branch(unsigned condition, int sign)
{
    switch (condition) {
	case 0: jump(); break;
	case 1: pc = cell_to_address(M); break;
	case 2: if (overflow)  jump(); overflow = false; break;
	case 3: if (!overflow) jump(); overflow = false; break;
	case 4: if (sign <  0) jump(); break;
	case 5: if (sign == 0) jump(); break;
	case 6: if (sign  > 0) jump(); break;
	case 7: if (sign >= 0) jump(); break;
	case 8: if (sign != 0) jump(); break;
	case 9: if (sign <= 0) jump(); break;
	default: error("Bad branch condition");
    }
}

static void do_jump(void)
{
    branch(F, comparison_indicator);
}

static int sign_of_difference(Cell difference)
{
    return magnitude(difference) == 0 ? 0 : is_negative(difference) ? -1 : 1;
}

static void do_reg_branch(void)
{
    branch(F + 4, sign_of_difference(r[C & 7]));
}

static void do_jbus(void)
{
    /* no channel is ever busy, because we're using C's blocking I/O */
}

static void do_jred(void)
{
    jump();     /* conversely, all channels are always ready */
}

static void do_ioc(void)    { io_control(F, M); }
static void do_in(void)     { do_input(F, r[X], cell_to_address(M)); }
static void do_out(void)    { do_output(F, r[X], cell_to_address(M)); }

static void do_addr_op(void)
{
    Cell cell;
    unsigned reg = C & 7;
    switch (F) {
	case 0: cell = add(r[reg], M); break;
	case 1: cell = sub(r[reg], M); break;
	case 2: cell = M; break;
	case 3: cell = negative(M); break;
	default: error("Unknown extended opcode"); cell = zero;
    }
    if (reg - 1 < 6)        /* same as: 1 <= reg && reg <= 6 */
	if (INDEX_MAX < magnitude(cell))
	    error("Magnitude too large for index register: %10o", 
		  magnitude(cell));
    r[reg] = cell;
}

static void do_compare(void)
{
    Flag saved = overflow;
    Cell difference = sub(field(F, r[C & 7]), 
			  field(F, memory[cell_to_address(M)]));
    comparison_indicator = sign_of_difference(difference);
    overflow = saved;
}

static const struct {
    void (*action)(void);
    unsigned clocks;
} op_table[64] = {
    { do_nop, 1 },
    { do_add, 2 },
    { do_sub, 2 },
    { do_mul, 10 },
    { do_div, 12 },
    { do_special, 1 },
    { do_shift, 2 },
    { do_move, 1 },

    { do_lda, 2 },
    { do_ldi, 2 },
    { do_ldi, 2 },
    { do_ldi, 2 },
    { do_ldi, 2 },
    { do_ldi, 2 },
    { do_ldi, 2 },
    { do_ldx, 2 },

    { do_ldan, 2 },
    { do_ldin, 2 },
    { do_ldin, 2 },
    { do_ldin, 2 },
    { do_ldin, 2 },
    { do_ldin, 2 },
    { do_ldin, 2 },
    { do_ldxn, 2 },

    { do_store, 2 },
    { do_store, 2 },
    { do_store, 2 },
    { do_store, 2 },
    { do_store, 2 },
    { do_store, 2 },
    { do_store, 2 },
    { do_store, 2 },

    { do_store, 2 },
    { do_store, 2 },
    { do_jbus, 1 },
    { do_ioc, 1 },
    { do_in, 1 },
    { do_out, 1 },
    { do_jred, 1 },
    { do_jump, 1 },

    { do_reg_branch, 1 },
    { do_reg_branch, 1 },
    { do_reg_branch, 1 },
    { do_reg_branch, 1 },
    { do_reg_branch, 1 },
    { do_reg_branch, 1 },
    { do_reg_branch, 1 },
    { do_reg_branch, 1 },

    { do_addr_op, 1 },
    { do_addr_op, 1 },
    { do_addr_op, 1 },
    { do_addr_op, 1 },
    { do_addr_op, 1 },
    { do_addr_op, 1 },
    { do_addr_op, 1 },
    { do_addr_op, 1 },

    { do_compare, 2 },
    { do_compare, 2 },
    { do_compare, 2 },
    { do_compare, 2 },
    { do_compare, 2 },
    { do_compare, 2 },
    { do_compare, 2 },
    { do_compare, 2 },
};

void run(void)
{
    install_error_handler(stop);
    if (setjmp(escape_k) != 0)
	return;
    for (;;) {
/*      print_CPU_state(); */
	if (memory_size <= pc)
	    error("Program counter out of range: %4o", pc);
	{
	    Byte I;
	    destructure_cell(memory[pc++], M, I, F, C);
	    if (6 < I)
		error("Invalid I-field: %u", I);
	    if (I != 0)
	        M = add(M, r[I]);  /* (the add can't overflow because the numbers are too small) */
	    op_table[C].action();
	    elapsed_time += op_table[C].clocks;
	}
    }
}
