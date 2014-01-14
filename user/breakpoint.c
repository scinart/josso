// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("before BRKPT\n");
	asm volatile("int $3");
	int i;
	i=0x12345678;
	cprintf("%08x\n", i);
	asm volatile("int $3");
	cprintf("after BRKPT\n");
}

