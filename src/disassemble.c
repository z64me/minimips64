/* <z64.me> barebones mips r4300i disassembler */

#include "common.h"

static int unpack(unsigned v, const struct pack *p)
{
	if (p->shift < 0)
		v >>= -p->shift;
	else
		v <<= p->shift;
	return v & p->and;
}

/* disassemble an instruction */
static int dasm(unsigned op)
{
	const struct inst *i;
	const struct pack *p;
	const char *notation_imm = "0x%04x";
	const char *notation_fp = "f%d";
	const char *notation = "%s";
	
	if (!op)
		return fprintf(stdout, "nop\n");
	
	/* no match found */
	if (!(i = findinstFromBin(op)))
		die("invalid instruction %08X", op);
	
	/* unpack instruction */
	fprintf(stdout, "%-12s", i->name);
	for (p = i->pack; p->and; ++p)
	{
		/* is register */
		if (p->is_reg)
		{
			if (p->is_fp)
				fprintf(stdout, notation_fp, unpack(op, p));
			else
				fprintf(stdout, notation, regName(unpack(op, p)));
			notation = ", %s";
			notation_imm = ", 0x%04x";
			notation_fp = ", f%d";
		}
		
		/* is immediate */
		else if (!p->is_v)
		{
			fprintf(stdout, notation_imm, unpack(op, p));
			notation = "(%s)";
		}
	}
	fprintf(stdout, "\n");
	return 0;
}

static int interactiveMode(void)
{
	fprintf(stderr,
		"welcome to interactive mode; copy-paste or type\n"
		"bytes you wish to disassemble (try 03E00008)\n"
	);
	while (1)
	{
		char b[32] = {0};
		unsigned v;
		
		fgets(b, sizeof(b), stdin);
		
		if (sscanf(b, "%x", &v) != 1)
			break;
		fprintf(stdout, " -> ");
		dasm(v);
	}
	return EXIT_SUCCESS;
}

static void showargs(void)
{
	die(
		"args: minimips64 --disassemble ... \"input.bin\"\n"
		"  ... refers to the following optional arguments:\n"
		"    --address 0x1234    address within binary file to begin\n"
		"    --number 10         number of opcodes to disassemble\n"
		"    --jrra              stop parsing when jr $ra reached\n"
		"    --prefix            prefix each line with address\n"
		"    --hex               prefix each line with instruction hex\n"
		"    --start             locate the start of the function\n"
		"                        before disassembling it\n"
		"    --interactive       do not load binary file; instead,\n"
		"                        disassemble bytes typed by user\n"
	);
}

int disassemble(int argc, char *argv[])
{
	const char *inFilename = argv[argc - 1];
	void *bin;
	unsigned char *op;
	unsigned int address = 0;
	unsigned sz;
	unsigned numop = 0;
	int jrra = 0;
	int prefix = 0;
	int funcbody = 0;
	int hexPrefix = 0;
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
		
		if (!strcmp(this, "--prefix"))
		{
			prefix = 1;
			continue;
		}
		else if(!strcmp(this, "--hex"))
		{
			hexPrefix = 1;
			continue;
		}
		else if(!strcmp(this, "--jrra"))
		{
			jrra = 1;
			continue;
		}
		else if(!strcmp(this, "--start"))
		{
			funcbody = 1;
			continue;
		}
		
		/* the following arguments have parameters */
		if (!next)
			die("argument '%s' no parameter given", this);
		
		/* skips parameter on next loop */
		++i;
		
		if (!strcmp(this, "--number"))
		{
			if (sscanf(next, "%i", &numop) != 1)
				die("'%s %s' failed to parse parameter", this, next);
		}
		else if (!strcmp(this, "--address"))
		{
			if (sscanf(next, "%i", &address) != 1)
				die("'%s %s' failed to parse parameter", this, next);
			if (address & 3)
				die("'%s %s': address not 32-bit aligned", this, next);
		}
		else
		{
			fprintf(stderr, "unknown argument '%s'\n", this);
			showargs();
		}
	}
	
	if (!inFilename)
	{
		fprintf(stderr, "no in filename provided\n");
		showargs();
	}
	
	/* load file */
	op = bin = loadfile(inFilename, &sz, 0);
	
	/* seek address */
	if (address >= sz - 4)
		die("address 0x%X exceeds filesize 0x%X", address, sz);
	sz -= address;
	op += address;
	
	/* numop */
	if (numop && numop * 4 < sz)
		sz = numop * 4;
	
	/* search backwards for end of previous function (if applicable),
	 * which is a hacky way of guessing the beginning of this function
	 */
	if (funcbody)
	{
		while (address >= 4)
		{
			op -= 4;
			address -= 4;
			if (u32r(op) == 0x03E00008) /* jr ra */
			{
				/* skip jr ra and its delay slot */
				op += 8;
				address += 8;
				break;
			}
		}
	}
	
	/* process file four bytes (one 32-bit word) at a time */
	do
	{
		if (prefix && hexPrefix)
			fprintf(stdout, "/*%08X, %08X*/\t", address, u32r(op));
		else if (prefix)
			fprintf(stdout, "/*%08X*/\t", address);
		else if (hexPrefix)
			fprintf(stdout, "/*%08X*/\t", u32r(op));
		dasm(u32r(op));
		if (jrra && address >= 4 && u32r(op-4) == 0x03E00008) /* jr ra */
			break;
		sz -= 4;
		address += 4;
		op += 4;
	} while (sz >= 4);
	
	/* cleanup */
	free(bin);
	fprintf(stderr, "successfully disassembled!\n");
	return EXIT_SUCCESS;
}

