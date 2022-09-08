SHELL=/bin/bash
MYAS=/usr/bin/nasm
MYASFLAGS=-felf

CC=gcc
CFLAGS=-std=c99 -m32
LDFLAGS=-m32
MTP=../../mtp

PRGMS:= case fizzbuzz everything selftest scopes largecase

.PHONY: all clean test

all: $(PRGMS)

%.s: %.mtp
	$(MTP) -o $@ $<

%.o: %.s
	$(MYAS) $(MYASFLAGS) $< -o $@

%: %.o
	$(CC) $(CFLAGS) io.c io.h $< -o $@


clean:
	@rm -f $(PRGMS) *.s

test:
	$(foreach p, $(PRGMS), $(SHELL) -c 'diff $(p:=.expected) <(echo -e "1\n2\n3" | ./$(p))';)
