// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "ct", "Continue", mon_continue },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int move_up_arg(uint32_t* addr, int times)
{
	addr += times;
	return (*addr);
}

int __attribute__ ((noinline))
read_eip()
{
	int cur_ebp = read_ebp();
	int eip = move_up_arg((uint32_t*)cur_ebp, 1);
	return eip;
}

void print_debug(struct Eipdebuginfo * info)
{
	cprintf("       %s:%d: %.*s+%d\n", info->eip_file, info->eip_line, info->eip_fn_namelen, info->eip_fn_name, info->eip_fn_addr);
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("%$Y%$C%$RStack backtrace:\n");
	uint32_t current_ebp = read_ebp();
	uint32_t current_eip = read_eip();
	cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
			current_ebp, current_eip, move_up_arg((uint32_t*)current_ebp, 2),
			move_up_arg((uint32_t*)current_ebp, 3), move_up_arg((uint32_t*)current_ebp, 4),
			move_up_arg((uint32_t*)current_ebp, 4), move_up_arg((uint32_t*)current_ebp, 6));
	struct Eipdebuginfo info;
	//debuginfo_eip(current_eip, &info);
	debuginfo_eip(move_up_arg((uint32_t*)current_ebp, 1), &info);
	print_debug(&info);
	uint32_t pebp = current_ebp;

	pebp=*((uint32_t*)pebp);
	while(pebp)
	{
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
				pebp,
				move_up_arg((uint32_t*)pebp, 1), move_up_arg((uint32_t*)pebp, 2),
				move_up_arg((uint32_t*)pebp, 3), move_up_arg((uint32_t*)pebp, 4),
				move_up_arg((uint32_t*)pebp, 4), move_up_arg((uint32_t*)pebp, 6));
		debuginfo_eip(move_up_arg((uint32_t*)pebp, 1), &info);
		print_debug(&info);
		pebp=*((int*)pebp);
	}
	cprintf("%$V\n");
	return 0;
}

int mon_continue(int argc, char** argv, struct Trapframe *tf)
{
	if (curenv && curenv->env_status == ENV_RUNNING)
	{
		env_run(curenv);
	}
	else
	{
		cprintf("curenv not RUNNING. Hence not run it\n");
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
