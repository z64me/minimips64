/* <z64.me> just some common functions */

#include <stdarg.h>
#include "common.h"

/*
 *
 * private
 *
 */
static const char *reg[] = {
	#include "reg.h"
};

static const struct inst inst[] = {
	#include "inst.h"
};


/*
 *
 * public
 *
 */
unsigned u32r(void *_b)
{
	unsigned char *b = _b;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

void u32w(void *dst, unsigned src)
{
	unsigned char *b = dst;
	
	b[0] = src >> 24;
	b[1] = src >> 16;
	b[2] = src >>  8;
	b[3] = src;
}

/* load a file */
void *loadfile(const char *fn, unsigned *sz, const char *prefix)
{
	FILE *fp;
	char *src;
	unsigned sz1;
	if (!sz)
		sz = &sz1;
	if (!prefix)
		prefix = "";
	if (
		!(fp = fopen(fn, "rb"))
		|| fseek(fp, 0, SEEK_END)
		|| !(*sz = ftell(fp))
		|| fseek(fp, 0, SEEK_SET)
		|| !(src = calloc(1, *sz + strlen(prefix) + 1))
		|| (fread(src + strlen(prefix), 1, *sz, fp) != *sz)
		|| fclose(fp)
	)
		die("could not load file '%s'\n", fn);
	
	memcpy(src, prefix, strlen(prefix));
	
	return src;
}

const struct inst *findinstFromBin(unsigned v)
{
	const struct inst *i;
	const struct inst *end = inst + sizeof(inst) / sizeof(*inst);
	
	/* search instruction lut for matching name */
	for (i = inst; i < end; ++i)
		/* code mask matches */
		if ((v & i->mask) == i->code)
			return i;
	
	/* no match found */
	return 0;
}

const struct inst *findinstFromString(char *name)
{
	const struct inst *i;
	const struct inst *end = inst + sizeof(inst) / sizeof(*inst);
	
	/* search instruction lut for matching name */
	for (i = inst; i < end; ++i)
		/* name matches */
		if (!strcasecmp(i->name, name))
			return i;
	
	/* no match found */
	return 0;
}

const char *regName(unsigned i)
{
	if (i >= sizeof(reg) / sizeof(*reg))
		return "error";
	
	return reg[i];
}

void die(const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	
	exit(EXIT_FAILURE);
}

