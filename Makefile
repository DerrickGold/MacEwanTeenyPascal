# CMPT 399 (Winter 2016) Assignment 4
# Code Generation
# Derrick Gold
#
# Makefile
# Rules to build MacEwan Teeny Pascal

SHELL=/bin/bash
CC=gcc
CFLAGS= -Wall -g -std=c99 -D_POSIX_C_SOURCE=200809L

#include math library for floor function in symtab.c
LDLIBS=-lm 

LEX=flex
# -i to make flex case insensitive
LFLAGS=-i --yylineno

# get the name of this directory for use in remote
# deployment.
DIRNAME= $(lastword $(subst /, , $(CURDIR)))

.PHONY: clean all test remote test-leaks

all: mtp

mtp: mtp.o parser.o lexer.o tokens.o tree.o parserSyntax.o symtab.o bittree.o analyze.o \
	codegen.o

mtp.o: mtp.c parser.h lexer.h tree.h symtab.h parserHelper.h analyze.h \
  codegen.h tokens.h


parser.o: parser.c lexer.h tree.h symtab.h tokens.h parserHelper.h \
  parser.h parserSyntax.h

parser.c parser.h: parser.y lempar.c lemon lexer.h
	./lemon -s $<

parserSyntax.o: parserSyntax.c parserSyntax.h lexer.h tokens.h parser.h \
  tree.h symtab.h defines.h

symtab.o: symtab.c symtab.h defines.h lexer.h

bittree.o: bittree.c bittree.h

analyze.o: analyze.c analyze.h tree.h lexer.h symtab.h parser.h \
  parserHelper.h defines.h bittree.h

tokens.o: tokens.c

tokens.c tokens.h: parser.h
	./mk_tokens.sh

tree.o: tree.c tree.h

codegen.o: codegen.c tree.h lexer.h symtab.h parser.h defines.h bittree.h

lexer.o: lexer.c parser.h tokens.h
	$(CC) $(CFLAGS) -Wno-unused-function -c $<

%.c %.h: %.l
	$(LEX) $(LFLAGS) --header-file=lexer.h -o $*.c $*.l

clean:
	$(RM) parser.{c,h,o,out} lexer.{c,h,o} mtp{,.o} lemon{,.o} tokens{.c,.h,.o} tree.o \
	parserSyntax.o symtab.o analyze.o bittree.o codegen.o \
	tests/semantic/*.s
	cd tests/codegen && make clean
test:
	tests/semantic/testAll.sh
	cd tests/codegen && make clean && make && make test
test-leaks:
	tests/bin/memcheckAll.sh tests/semantic

# Send this programs directory to the student server,
# build the program on that server, then run all the tests.
remote:
	tar -czvf - . | ssh student \
		"tar -C ~/$(DIRNAME) -xzf - && cd ~/$(DIRNAME) && make clean && make && make test test-leaks"  

