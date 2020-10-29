# Makefile for SGFC
# Copyright (C) 1996-2018 by Arno Hollosi
# (see 'COPYING' for more copyright information)

# System configuration
# SHELL = /bin/bash

# System environment
CC = gcc

OPTIONS = -pedantic -W -Wimplicit -Wreturn-type -Wswitch -Wformat\
	  -Wuninitialized -Wparentheses -Wpointer-arith -Wbad-function-cast\
	  -Wcast-qual -Wcast-align -Waggregate-return -Wstrict-prototypes\
	  -Wnested-externs -Wshadow -Wchar-subscripts -std=c99\
	  -Wimplicit-fallthrough=0

DIRECTORIES = -I ../src
OPTIMIZATION = -O1
CFLAGS = $(DIRECTORIES) $(OPTIMIZATION) $(OPTIONS)

LIB = -lcheck -lpthread -lrt -lsubunit -lm
OBJ = testrunner.o test-helper.o position.o parse-text.o check-value.o trigger_errors.o
SRC_OBJ = ../src/execute.o ../src/gameinfo.o ../src/load.o\
	../src/parse.o ../src/parse2.o ../src/options.o\
	../src/properties.o ../src/save.o ../src/strict.o ../src/util.o

sgfc-test: $(OBJ) $(SRC_OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(SRC_OBJ) -o $@ $(LIB)

tests: sgfc-test
	./sgfc-test

all: clean tests

clean:
	rm -f $(OBJ) sgfc-test

%.o: %.c test-common.h ../src/all.h ../src/protos.h
	$(CC) $(CFLAGS) -c $<