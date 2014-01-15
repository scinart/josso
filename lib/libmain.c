// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.
	envid_t curenvid = sys_getenvid();
	thisenv = &envs[curenvid%NENV]; //ENVid的各个位的意思又变了。好容易模个NENV对了。
	//cprintf("envs is %p\n", envs);
	//cprintf("libmain: thisenv is %p\n", thisenv);
	//cprintf("thisenv->env_id is %d\n", thisenv->env_id);

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

