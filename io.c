/* MIX simulator, copyright 1994 by Darius Bacon */ 
#include "mix.h"
#include "charset.h"
#include "io.h"
#include "run.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>

/* --- Device tables --- */

/* Device types: */
enum DeviceType { tape, disk, card_in, card_out, printer, console };

/* The device table: */
static struct {
    const enum DeviceType type;
    FILE *file;
    long position;		/* used by random-access devices */
  /*    const char *filename; */
} devices[] = {
    {tape}, {tape}, {tape}, {tape}, {tape}, {tape}, {tape}, {tape}, 
    {disk}, {disk}, {disk}, {disk}, {disk}, {disk}, {disk}, {disk}, 
    {card_in},
    {card_out},
    {printer},
    {console}
};

#define num_devices ( sizeof devices / sizeof devices[0] )

/* add an assign_file(device, filename) operation, too */
/* and unassign? */

typedef void IOHandler(unsigned, Cell, Address);
typedef void IOCHandler(unsigned, Cell);

static IOHandler tape_in, disk_in, text_in, console_in, no_in;
static IOHandler tape_out, disk_out, text_out, console_out, no_out;
static IOCHandler tape_ioc, disk_ioc, no_ioc, printer_ioc;

/* The device-class table: */
/*** need to distinguish read/write permission... */
static const struct Device_attributes {
    const char *base_filename;
    unsigned block_size;
    IOHandler *in_handler;
    IOHandler *out_handler;
    IOCHandler *ioc_handler;
} methods[] = {

/* tape */      { "tape", 100, tape_in,    tape_out,    tape_ioc },
/* disk */      { "disk", 100, disk_in,    disk_out,    disk_ioc },
/* card_in */   {  NULL,   16, text_in,    no_out,      no_ioc },
/* card_out */  {  NULL,   16, no_in,      text_out,    no_ioc },
/* printer */   {  NULL,   24, no_in,      text_out,    printer_ioc },
/* console */   {  NULL,   14, console_in, console_out, no_ioc }

};

static const struct Device_attributes *attributes (unsigned device)
{
    return &methods[devices[device].type];
}

static unsigned block_size(unsigned device)
{
    return attributes(device)->block_size;
}

static FILE *assigned_file(unsigned device)
{
    /* glibc2.1 fix -ajk */
    switch (devices[device].type) {
    case card_in:
        return stdin; break;
    case card_out: case printer:
        return stdout; break;
    default:
        return devices[device].file;
    }
}

static char *device_filename(Byte device)
{
    static char filename[FILENAME_MAX];
    sprintf(filename, "%s%02d",
	    attributes(device)->base_filename, device);
    return filename;
}

static void ensure_open(Byte device)
{
    if (num_devices <= device)
        error("Unknown device - %02o", device);
    if (!assigned_file(device)){
	if (attributes(device)->base_filename) {
	    const char *filename = device_filename(device);
	    if (!(devices[device].file = fopen(filename, "r+b"))
		&& !(devices[device].file = fopen(filename, "w+b")))
	        error("%s: %s", filename, strerror(errno));
	    devices[device].position = 0;
	} else
	    error("No file assigned to device %02o (type %d)", device, devices[device].type);
    }
}

void io_control(Byte device, Cell argument)
{
    ensure_open(device);
    attributes(device)->ioc_handler(device, argument);
}

void do_input(Byte device, Cell argument, Address buffer)
{
    ensure_open(device);
    attributes(device)->in_handler(device, argument, buffer);
}

void do_output(Byte device, Cell argument, Address buffer)
{
    ensure_open(device);
    attributes(device)->out_handler(device, argument, buffer);
}

/* --- Unsupported input or output --- */

static void no_ioc(unsigned device, Cell argument)
{
    error("IOC undefined for device %02o", device);
}

static void no_in(unsigned device, Cell argument, Address buffer)
{
    error("Input not allowed for device %02o", device);
}

static void no_out(unsigned device, Cell argument, Address buffer)
{
    error("Output not allowed for device %02o", device);
}

/* --- Text devices --- */

/* Read a line from -file- into memory[buffer..buffer+size). 
   (Big-endian byte order, padded with 0 bytes if the line is less 
   than -size- cells long.  The signs of the cells are set to '+'. 
   If the line is longer than 5*size bytes, only the first 5*size  
   bytes get read. */
static void read_line(FILE* file, Address buffer, unsigned size)
{
    unsigned i, b;
    Flag past_end = false;
    for (i = 0; i < size; ++i) {
	Cell cell = zero;
	if (memory_size <= buffer + i)
  /*** I think we need memory_fetch() and memory_store() functions... */
	    error("Address out of range");
	for (b = 1; b <= 5; ++b) {
	    Byte mix_char;
	    if (past_end)
		mix_char = (Byte) 0;
	    else {
		int c = fgetc(file);
		if (c == '\n' || c == EOF)
		    past_end = true, mix_char = (Byte) 0;
		else
		    mix_char = C_char_to_mix((char) c);
	    }
	    cell = set_byte(mix_char, b, cell);
	}
	memory[buffer + i] = cell;
    }
}

static void write_cell(Cell cell, FILE *outfile, Flag text)
{
    unsigned i;
    if (!text)
        fputc(is_negative(cell) ? '-' : ' ', outfile);
    for (i = 1; i <= 5; ++i)
        fputc(mix_to_C_char(get_byte(i, cell)), outfile);
}

static void write_line(FILE *file, Address buffer, unsigned size, Flag text)
{
    unsigned i;
    for (i = 0; i < size; ++i) {
	if (memory_size <= buffer + i)
	    error("Address out of range");
	write_cell(memory[buffer + i], file, text);
    }
    fputc('\n', file);
}

static void printer_ioc(Byte device, Cell argument)
{
    if (magnitude(argument) != 0)
        error("IOC argument undefined for printer device %02o", device);
    fputc('\f', assigned_file(device));
}

static void text_in(Byte device, Cell argument, Address buffer)
{
    read_line(assigned_file(device), buffer, block_size(device));
}

static void text_out(Byte device, Cell argument, Address buffer)
{
    write_line(assigned_file(device), buffer, block_size(device), true);
}

/* --- Block devices --- */
/*** Make this section more robust. */
/*** And either these errors should be fatal errors or we need to think
     about recovery. */

static void set_file_position(Byte device, unsigned block, Flag writing)
{
  if (fseek(assigned_file(device),
	    (long) block * (6 * block_size(device) + 1),
	    SEEK_SET))
      error("Device %02o: %s", device, strerror(errno));
}

/* Read a block from -device- into memory[buffer..buffer+block_size(device)). 
   (Big-endian byte order, with words represented by 6 native C characters: 
   the sign ('-' or ' '), followed by 5 characters whose MIX equivalents 
   code for the corresponding bytes.)  The block should end with a '\n'. */
static void read_block(Byte device, Address buffer)
{
    FILE *file = assigned_file(device);
    unsigned size = block_size(device);
    unsigned i, b;
    for (i = 0; i < size; ++i) {
	int c;
	Cell cell = zero;
	if (memory_size <= buffer + i)
	    error("Address out of range -- read_block");

	c = fgetc(file);
	if (c == EOF)
	    error("Unexpected EOF reading from device %02o", device);
	else if (c == '-')
	    cell = negative(cell);

	for (b = 1; b <= 5; ++b) {
	    c = fgetc(file);
	    if (c == EOF)
		error("Unexpected EOF reading from device %02o", device);
	    cell = set_byte(C_char_to_mix((char) c), b, cell);
	}
	memory[buffer + i] = cell;
    }
    fgetc(file);			/* should be '\n' */
}

/* The inverse of read_block. */
static void write_block(Byte device, Address buffer)
{
    write_line(assigned_file(device), buffer, block_size(device), false);
}

/* --- Tapes --- */

static void tape_ioc(unsigned device, Cell offset)
{
    error("Unimplemented");
}

static void tape_in(unsigned device, Cell argument, Address buffer)
{
    error("Unimplemented");
}

static void tape_out(unsigned device, Cell argument, Address buffer)
{
    error("Unimplemented");
}

/* --- Disks --- */

static void disk_ioc(Byte device, Cell offset)
{
    if (magnitude(offset) != 0)
        error("IOC argument undefined for disk device %02o", device);
}

static void disk_in(Byte device, Cell argument, Address buffer)
{
    unsigned block_num = (unsigned) field(make_field_spec(4, 5), argument);
    set_file_position(device, block_num, false);
    read_block(device, buffer);
}

static void disk_out(Byte device, Cell argument, Address buffer)
{
    unsigned block_num = (unsigned) field(make_field_spec(4, 5), argument);
    set_file_position(device, block_num, true);
    write_block(device, buffer);
}

/* --- The console (typewriter/paper tape) --- */
/* Always connected to stdin/stdout, for simplicity. */

static void console_in(Byte device, Cell argument, Address buffer)
{
    read_line(stdin, buffer, block_size(device));
}

static void console_out(Byte device, Cell argument, Address buffer)
{
    write_line(stdout, buffer, block_size(device), true);
}

