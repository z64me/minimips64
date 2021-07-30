/* <z64.me> just some common functions */

#ifndef MINIMIPS64_COMMON_H_INCLUDED
#define MINIMIPS64_COMMON_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct pack
{
	int   is_reg; /* is a register value                */
	int   is_fp;  /* is a floating-point register       */
	int   is_v;   /* use value v below for packing      */
	int   v;      /* value to use if (is_v)             */
	int   shift;  /* bit shift by (negative means left) */
	int   and;    /* bitwise & by (0 = last entry)      */
};

struct inst
{
	const char    *name;
	struct pack    pack[8];
	int            is_branch;
	unsigned       code;
	unsigned       mask;
};

unsigned u32r(void *_b);
void u32w(void *dst, unsigned src);
void *loadfile(const char *fn, unsigned *sz, const char *prefix);
const struct inst *findinstFromBin(unsigned v);
const struct inst *findinstFromString(char *name);
const char *regName(unsigned i);
void die(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

int assemble(int argc, char *argv[]);
int disassemble(int argc, char *argv[]);

#endif /* ! MINIMIPS64_COMMON_H_INCLUDED */

