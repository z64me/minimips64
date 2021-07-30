/* <z64.me> barebones mips r4300i disassembler */

#include <unistd.h> /* chdir */
#include "common.h"

#define DELIM           " \r\n\t(),="
#define TOK             strtok(0, DELIM)
#define STREQ(S0, S1)   (!strcasecmp(S0, S1))

struct map
{
	void  *next;
	char  *name;
	int    value;
};

static struct map *g_map = 0;
static unsigned g_baseofs = 0;
static int g_has_sought = 0;
static int g_was_label = 0;

/* seek directives */
static void seekdummy(FILE *dst, unsigned ofs)
{
	/* do nothing */
	(void)dst;
	(void)ofs;
}
static void seekrom(FILE *dst, unsigned ofs)
{
	fseek(dst, ofs + g_baseofs, SEEK_SET);
	g_has_sought = 1;
}
static void seekpatch(FILE *dst, unsigned ofs)
{
	static char *nl = 0;
	if (!nl) nl = "";
	else nl = "\n";
	fprintf(dst, "%s0x%X,", nl, ofs + g_baseofs);
	g_has_sought = 1;
}

/* put a 32-bit opcode in destination rom */
static void putrom(FILE *dst, unsigned op)
{
	fputc(op >> 24, dst);
	fputc(op >> 16, dst);
	fputc(op >>  8, dst);
	fputc(op >>  0, dst);
}

/* put a 32-bit opcode in destination patch */
static void putpatch(FILE *dst, unsigned op)
{
	fprintf(dst, "%08X", op);
}

/* enter directory of a file */
static void gofiledir(const char *fn)
{
	char *dir = strdup(fn);
	char *s;
	
	s = strrchr(dir, '\\');
	if (!s)
		s = strrchr(dir, '/');
	if (!s)
		goto L_cleanup;
	*s = 0;
	chdir(dir);
L_cleanup:
	free(dir);
}

/* process token as integer */
static int tokint(char *tok)
{
	int v;
	/* yes, i know %i covers both %x and %d, but
	 * it also does octal; i don't want error
	 * reports from people who prefix decimal
	 * values with zeroes...
	 */
	if (*tok == '0' && tolower(tok[1]) == 'x')
	{
		if (sscanf(tok, "%x", &v) != 1)
			goto L_fail;
	}
	else if (*tok == '$')
	{
		if (sscanf(tok + 1, "%x", &v) != 1)
			goto L_fail;
	}
	else
	{
		if (sscanf(tok, "%d", &v) != 1)
			goto L_fail;
	}
	return v;
L_fail:
	die("'%s' undefined or not int", tok);
	return 0;
}

/* grab int value mapped to a name in the remap list */
static int map(char *tok)
{
	struct map *m;
	
	/* search matching name in global map */
	g_was_label = 1;
	for (m = g_map; m; m = m->next)
		if (STREQ(tok, m->name))
			return m->value;
	
	/* no match found; try processing as ordinary integer */
	g_was_label = 0;
	return tokint(tok);
}

/* link name and value combination into remap list */
static void maplink(char *name, char *value)
{
	struct map *m = malloc(sizeof(*m));
	
	m->next = g_map;
	m->name = strdup(name);
	m->value = tokint(value);
	g_map = m;
}

/* link name and value combination into remap list */
static void maplink_int(char *name, int value)
{
	struct map *m = malloc(sizeof(*m));
	
	m->next = g_map;
	m->name = strdup(name);
	m->value = value;
	g_map = m;
}

/* cleanup maps */
static void map_cleanup(struct map *m)
{
	while (m)
	{
		struct map *next = m->next;
		if (m->name)
			free(m->name);
		free(m);
		m = next;
	}
}

/* get value associated with token */
static int getval(char *tok)
{
	int v;
	
	/* special stuff */
	if (*tok == '%')
	{
		v = map(TOK);
		++tok;
		if (STREQ(tok, "hi"))
		{
			if (v & 0x8000)
				v += 0x10000;
			v >>= 16;
		}
		else if (STREQ(tok, "lo"))
		{
			v &= 0xFFFF;
		}
		else
			die("invalid special '%s'", tok);
		
		return v;
	}
	
	/* ordinary token */
	return map(tok);
}

/* given a token, return a register number */
static int reg(const char *tok)
{
	unsigned i;
	const char *lut[] = {
		#include "reg.h"
	};
	
	/* skip $ if there is one */
	tok += *tok == '$';
	
	/* floating point register */
	if (tolower(*tok) == 'f')
	{
		/* if followed immediately by a number less than 32 */
		if (sscanf(tok + 1, "%d", &i) == 1 && i < 32)
			return i;
	}
	
	/* search register lut for matching name */
	for (i = 0; i < sizeof(lut) / sizeof(*lut); ++i)
		if (STREQ(lut[i], tok))
			return i;
	
	/* failed to locate register */
	die("invalid register '%s'", tok);
	return -1;
}

/* remove quotes from a name */
static char *rmquote(char *str)
{
	char *end;
	
	/* remove open quote */
	while (*str == '\'' || *str == '"')
		++str;
	/* remove end quote */
	end = strchr(str, '\'');
	if (!end)
		end = strchr(str, '"');
	if (end)
		*end = '\0';
	
	return str;
}

/* "import" directive */
static void import(void)
{
	char *sp = 0;
	char *tok;
	char *f;
	char *fn = rmquote(TOK);
	/* load file */
	f = loadfile(fn, 0, 0);
	/* tokenize */
	tok = strtok_r(f, DELIM, &sp);
	while (tok)
	{
		char *tok1 = strtok_r(0, DELIM, &sp);
		if (!tok1)
			die("import '%s': broken symbol '%s'", fn, tok);
		//fprintf(stderr, "%s %s\n", tok, tok1);
		maplink(tok, tok1);
		tok = strtok_r(0, DELIM, &sp);
	}
	/* cleanup */
	free(f);
}

/* "define" directive */
static void define(void)
{
	/* XXX careful here, do not do maplink(TOK, TOK),
	 *     as that would be undefined behavior
	 */
	char *name = TOK;
	char *value = TOK;
	maplink(name, value);
}

/* "incbin" directive */
static void incbin(void put(FILE *, unsigned), FILE *fp)
{
	unsigned sz;
	void *bin = loadfile(rmquote(TOK), &sz, 0);
	unsigned char *b;
	/* ensure multiple of 4 */
	if (sz & 3)
		sz += 4 - (sz & 3);
	/* write bytes */
	for (b = bin; sz; sz -= 4)
	{
		put(fp, u32r(b));
		b += 4;
	}
	free(bin);
}

/* generate an instruction */
static unsigned int inst(char *opname)
{
	const struct inst *i;
	const struct pack *p;
	unsigned int result = 0;
	static int ofs = 0;
	
	/* no match found */
	if (!(i = findinstFromString(opname)))
		die("invalid instruction '%s'", opname);
	
	++ofs;
	
	/* construct result */
	for (p = i->pack; p->and; ++p)
	{
		int v;
		
		/* is register */
		if (p->is_reg)
			v = reg(TOK);
		
		/* is value */
		else if (p->is_v)
			v = p->v;
		
		/* is immediate */
		else
		{
			v = getval(TOK);
		
			/* if branch, make v relative to ofs */
			if (i->is_branch && g_was_label)
				v -= ofs;
		}
		
		/* transform */
		v &= p->and;
		if (p->shift < 0)
			v <<= -p->shift;
		else
			v >>= p->shift;
		
		/* apply */
		result |= v;
	}
	
	return result;
}

static int once(
	void seek(FILE *, unsigned)
	, void put(FILE *, unsigned)
	, FILE *fp
)
{
	char *opname = TOK;
	unsigned v;
	
	/* no more opcodes */
	if (!opname)
		return 1;
	
	/* handle directives */
	while (*opname == '>')
	{
		char *d = TOK;
		
		if (STREQ(d, "seek"))
			seek(fp, getval(TOK));
		else if (STREQ(d, "define"))
			define();
		else if (STREQ(d, "import"))
			import();
		else if (STREQ(d, "incbin"))
			incbin(put, fp);
		else if (STREQ(d, "write32"))
			put(fp, getval(TOK));
		else
			die("unknown directive '%s'", d);
		
		opname = TOK;
		
		/* file ended with a directive */
		if (!opname)
			return 1;
	}
	
	if (!g_has_sought && seek && fp)
		seek(fp, 0);
	
	v = inst(opname);
	if (v == (unsigned)-1)
		return 1;
	if (put && fp)
		put(fp, v);
	else
		fprintf(stderr, "%08X\n", v);
	
	return 0;
}

static void labels(char *src)
{
	int ofs = 0;
	char *tok;
	char *cpy = strdup(src);
	strtok(cpy, DELIM);
	while ((tok = TOK))
	{
		/* is an instruction */
		if (findinstFromString(tok))
		{
			++ofs;
			continue;
		}
		
		/* too short for LABEL_x or does not start with LABEL_ */
		if (strlen(tok) <= 6 || memcmp(tok, "LABEL_", 6) || !strchr(tok, ':') )
			continue;
		
		/* is a label */
		/* strip from original source */
		memset(src + (tok - cpy), ' ', strlen(tok));
		/* zero-terminate before registering */
		*strchr(tok, ':') = '\0';
		maplink_int(tok, ofs);
		//fprintf(stderr, "made label %s, %d\n", tok, ofs);
	}
	free(cpy);
}

/* remove comments from source code */
static void rmnotes(char *src)
{
	int comment = 0;
	int block = 0;
	while (*src)
	{
		/* block comment start */
		if (!memcmp(src, "/*", 2))
			block = 1;
		
		/* line comment start */
		if (*src == '#' || *src == ';' || !memcmp(src, "//", 2))
			comment = 1;
		
		/* block comment end */
		if (block && !memcmp(src, "*/", 2))
		{
			*src = ' ';
			src[1] = ' ';
			block = 0;
		}
		
		/* line comment end */
		else if (*src == '\n')
			comment = 0;
		
		/* comments are simply made whitespace */
		if (comment || block)
			*src = ' ';
		
		++src;
	}
}

static char *argeq(const char *arg)
{
	char *s = strdup(arg + 3);
	char *ss = strchr(s, '=');
	if (!ss)
		die("invalid argument '%s' (no equals?)", arg);
	*ss = '\0';
	return s;
}

static void showargs(void)
{
	die(
		"args: minimips64 --assemble ... \"input.asm\"\n"
		"  ... refers to the following optional arguments:\n"
		"    --Dname=0x5678       for defining symbols\n"
		"    --address 0x1234     set base address \n"
		"    --cloud \"out.txt\"    write cloudpatch (overwrites if exists)\n"
		"    --rom \"rom.z64\"      update rom (file must exist)\n"
		"    --interactive        do not load file; instead, assemble\n"
		"                         instructions typed by user\n"
	);
}

static int interactiveMode(void)
{
	fprintf(stderr,
		"welcome to interactive mode; copy-paste or type\n"
		"instructions you wish to assemble (try jr $ra)\n"
	);
	while (1)
	{
		char src[512] = {'x', ' ', 0}; /* dummy token is skipped */
		int prefix = strlen(src);
		
		fgets(src + prefix, sizeof(src) - prefix, stdin);
		
		if (!isalnum(src[prefix]))
			break;
		
		fprintf(stderr, " -> ");
		strtok(src, DELIM);
		while (!once(0, 0, 0))
			;
	}
	return EXIT_SUCCESS;
}

int assemble(int argc, char *argv[])
{
	FILE *fp = 0;
	char *src;
	unsigned sz;
	void (*seek)(FILE *, unsigned) = seekdummy;
	void (*put)(FILE *, unsigned) = 0;
	const char *inFilename = argv[argc - 1];
	int i;
	
	if (argc < 1)
		showargs();
	
	for (i = 0; i < argc; ++i)
	{
		const char *this = argv[i];
		
		if(!strcmp(this, "--interactive"))
		{
			return interactiveMode();
		}
	}
	
	for (i = 0; i < argc - 1; ++i)
	{
		const char *this = argv[i];
		const char *next = argv[i + 1];
		
		if (!memcmp(this, "--D", 3))
		{
			char *s = argeq(this);
			maplink(s, s + strlen(s) + 1);
			free(s);
			continue;
		}
		
		/* the following arguments have parameters */
		if (!next)
			die("argument '%s' no parameter given", this);
		
		/* skips parameter on next loop */
		++i;
		
		if (!strcmp(this, "--address"))
		{
			static char times = 0;
			if (times++)
				die("--address used multiple times");
			
			if (sscanf(next, "%i", &g_baseofs) != 1)
				die("invalid base offset %s", next);
		}
		else if (!strcmp(this, "--rom") || !strcmp(this, "--cloud"))
		{
			if (fp)
				die("--rom or --cloud arg used multiple times");
			
			/* update existing rom */
			if (!strcmp(this, "--rom"))
			{
				seek = seekrom;
				put = putrom;
				fp = fopen(next, "rb+");
			}
			/* overwrite cloudpatch if exists */
			else if (!strcmp(this, "--cloud"))
			{
				seek = seekpatch;
				put = putpatch;
				fp = fopen(next, "w");
			}
			
			if (!fp)
				die("failed to open/create '%s'", next);
		}
		else
		{
			fprintf(stderr, "unknown argument '%s'\n", this);
			showargs();
		}
	}
	
	if (!argc)
		showargs();
	
	/* load file */
	src = loadfile(inFilename, &sz, "x "/*dummy token prefix*/);
	
	/* enter directory of file (if applicable) */
	gofiledir(inFilename);
	
	/* strip comments */
	rmnotes(src);
	
	/* find labels for branching */
	labels(src);
	
	/* tokenize */
	strtok(src, DELIM);
	
	while (!once(seek, put, fp))
		;
	
	/* cleanup */
	free(src);
	fclose(fp);
	map_cleanup(g_map);
	fprintf(stderr, "successfully assembled!\n");
	return 0;
}

