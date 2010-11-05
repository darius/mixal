/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"

#include "asm.h"
#include "driver.h"
#include "parse.h"
#include "run.h"
#include "symbol.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *current_file = NULL;
static unsigned line_number;
static unsigned num_errors = 0;

void fatal_error(const char *message, ...)
{
    va_list args;
    if (current_file)   /*** of course, the error may not have anything to do with the file... */
	fprintf(stderr, "FATAL ERROR (%s, line %u): ", current_file, line_number);
    else
	fprintf(stderr, "FATAL ERROR: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static void (*error_handler)(const char *, va_list) = NULL;

void install_error_handler(void (*handler)(const char *, va_list))
{
    error_handler = handler;
}

void error(const char *message, ...)
{
    va_list args;
    ++num_errors;
    va_start(args, message);
    if (error_handler)
	error_handler(message, args);
    else
        fatal_error("No error handler installed");
    va_end(args);
}

void warn(const char *message, ...)
{
    va_list args;
    if (current_file)
	fprintf(stderr, "WARNING (%s, line %u): ", current_file, line_number);
    else
	fprintf(stderr, "WARNING: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* --- Miscellany --- */

static FILE *open_file(const char *filename, const char *mode)
{
    if (strcmp(filename, "-") == 0) {
	assert(mode[0] == 'r' || mode[0] == 'w');
	return mode[0] == 'w' ? stdout : stdin;
    } else {
	FILE *result = fopen(filename, mode);
	if (!result)
	    fatal_error("%s: %s", filename, strerror(errno));
	return result;
    }
}

/* --- Main program --- */

static void assembler_error(const char *message, va_list args)
{
    if (current_file)
	fprintf(stderr, "ERROR (%s, line %u): ", current_file, line_number);
    else
	fprintf(stderr, "ERROR: ");
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
}

static void assemble_file(const char *filename)
{
    FILE *infile = open_file(filename, "r");
    char line[257];
    current_file = filename, line_number = 0;
    while (fgets(line, sizeof line, infile)) {
	++line_number;
	assemble_line(line);
	if (line[strlen(line) - 1] != '\n') {
	    if (!fgets(line, sizeof line, infile))
	        break;		/* the file's last line had no '\n' */
	    else {
	        error("Line length exceeds 256 characters");
		/* Skip the rest of the line */
		while (line[strlen(line) - 1] != '\n')
		    if (!fgets(line, sizeof line, infile))
		        break;
	    }
	}
    }
    if (ferror(infile))
        error("%s: %s", filename, strerror(errno));
    fclose(infile);
    current_file = NULL;
}

int main(int argc, char **argv)
{
    precompute_field_data();

    /* Assemble the input: */
    install_error_handler(assembler_error);
    if (argc == 1)
	assemble_file("-");
    else {
	int i;
	for (i = 1; i < argc; ++i)
	    assemble_file(argv[i]);
    }
    resolve_generated_futures();
    if (num_errors != 0) {
	fprintf(stderr, "%u errors found\n", num_errors);
	exit(1);
    }

    /* Now run it: */
    set_initial_state();
    {
	clock_t finish, start = clock();
	run();
	finish = clock();
	fprintf(stderr, "%g seconds elapsed\n", 
		(float)(finish - start) / CLOCKS_PER_SEC);
    }
    print_CPU_state();
    return 0;
}
