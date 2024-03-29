/**************************************************************************
*** Project: SGF Syntax Checker & Converter
***	File:	 tests/test-common.h
***
*** Copyright (C) 1996-2021 by Arno Hollosi
*** (see 'main.c' for more copyright information)
***
**************************************************************************/

#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdlib.h>
#include <check.h>

#include "all.h"
#include "protos.h"

/* test-helper.c */

extern struct SGFInfo *sgfc;
extern char *expected_output;

struct SaveFileHandler *SetupSaveTestIO(void);
void common_setup(void);
void common_teardown(void);

#endif /* TEST_COMMON_H_ */
