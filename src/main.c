/* <z64.me> minimips64 entry point */

#include "common.h"

static void showargs(void)
{
	die("args: minimips64 --assemble or --disassemble\n");
}

int main(int argc, char *argv[])
{
	const char *mode = argv[1];
	
	fprintf(stderr, "welcome to minimips64 v1.0.0 <z64.me>\n");
	
	if (argc <= 1)
		showargs();
	
	/* skip the first two arguments now */
	argv += 2;
	argc -= 2;
	
	if (!strcmp(mode, "--assemble"))
		return assemble(argc, argv);
	
	if(!strcmp(mode, "--disassemble"))
		return disassemble(argc, argv);
	
	showargs();
	
	return EXIT_FAILURE;
}

