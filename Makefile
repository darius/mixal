# UNIX makefile for the MIX interpreter

# Must be changed in the spec file as well.
VERS=$(shell sed <mixal.spec -n -e '/Version: \(.*\)/s//\1/p')

CFLAGS = -O2 -Wall

CSRCS = asm.c cell.c charset.c driver.c io.c main.c parse.c run.c symbol.c
HSRCS = asm.h cell.h charset.h driver.h io.h mix.h parse.h run.h symbol.h
SOURCES = Makefile opcodes op2c.awk $(CSRCS) $(HSRCS)
DOCS = README NOTES MIX.DOC mixal.xml mixal.1
TEST = elevator.mix prime.mix mystery.mix prime.out mystery.out mixtest ops.inc
MISC = mixal.spec
DOSFILES = makefile.dos test.bat
DISTRIBUTION = $(SOURCES) $(DOCS) $(TEST) $(MISC) $(DOSFILES)

OBJS  = asm.o cell.o charset.o driver.o io.o main.o parse.o run.o symbol.o

mixal: $(OBJS)
	$(CC) $(OBJS) -o mixal

mixal.1: mixal.xml
	xmlto man mixal.xml

mixal.html: mixal.xml
	xmlto html-nochunks mixal.xml

clean:
	rm -f *.o mixal ops.inc mnemonic mixal.shar core.mix *.tar.gz *.rpm *~
	rm -f *.1 MANIFEST SHIPPER.*

dist: mixal-$(VERS).tar.gz

mixal-$(VERS).tar.gz: $(DISTRIBUTION)
	@ls $(DISTRIBUTION) | sed "s:^:mixal-$(VERS)/:" >MANIFEST
	@(cd ..; ln -s mixal mixal-$(VERS))
	(cd ..; tar -czf mixal/mixal-$(VERS).tar.gz `cat mixal/MANIFEST`)
	@(cd ..; rm mixal-$(VERS))

shar:
	shar $(DISTRIBUTION) >mixal.shar

ops.inc: opcodes
	sort <opcodes | awk -f op2c.awk >ops.inc

asm.o: asm.c mix.h cell.h asm.h run.h
cell.o: cell.c mix.h cell.h
symbol.o: symbol.c mix.h cell.h asm.h symbol.h
charset.o: charset.c mix.h cell.h charset.h
driver.o: driver.c mix.h cell.h asm.h driver.h parse.h symbol.h ops.inc
main.o: main.c mix.h cell.h asm.h driver.h parse.h run.h symbol.h
parse.o: parse.c mix.h cell.h asm.h charset.h parse.h symbol.h
run.o: run.c mix.h cell.h asm.h charset.h io.h run.h
io.o: io.c mix.h cell.h charset.h io.h run.h

release: mixal-$(VERS).tar.gz mixal.html
	shipper -u -m -t; make clean
