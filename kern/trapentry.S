/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
/* 由于已经做过一遍了这里就来个全的吧。*/

	TRAPHANDLER_NOEC(divzero_entry, T_DIVIDE);
	TRAPHANDLER_NOEC(debug_entry, T_DEBUG);
	TRAPHANDLER_NOEC(nmi_entry, T_NMI);
	TRAPHANDLER_NOEC(brkpt_entry, T_BRKPT);
	TRAPHANDLER_NOEC(oflow_entry, T_OFLOW);
	TRAPHANDLER_NOEC(bound_entry, T_BOUND);
	TRAPHANDLER_NOEC(illop_entry, T_ILLOP);
	TRAPHANDLER_NOEC(device_entry, T_DEVICE);
	TRAPHANDLER(dblflt_entry, T_DBLFLT);
	TRAPHANDLER(tss_entry, T_TSS);
	TRAPHANDLER(segnp_entry, T_SEGNP);
	TRAPHANDLER(stack_entry, T_STACK);
	TRAPHANDLER(gpflt_entry, T_GPFLT);
	TRAPHANDLER(pgflt_entry, T_PGFLT);
	TRAPHANDLER_NOEC(fperr_entry, T_FPERR);
	TRAPHANDLER(align_entry, T_ALIGN);
	TRAPHANDLER_NOEC(mchk_entry, T_MCHK);
	TRAPHANDLER_NOEC(simderr_entry, T_SIMDERR);
	TRAPHANDLER_NOEC(syscall_entry, T_SYSCALL);

	TRAPHANDLER_NOEC(irq0_entry, IRQ_OFFSET+0); //IRQ_TIMER
	TRAPHANDLER_NOEC(irq1_entry, IRQ_OFFSET+1); //IRQ_KBD
	TRAPHANDLER_NOEC(irq2_entry, IRQ_OFFSET+2);
	TRAPHANDLER_NOEC(irq3_entry, IRQ_OFFSET+3);
	TRAPHANDLER_NOEC(irq4_entry, IRQ_OFFSET+4); //IRQ_SERIAL
	TRAPHANDLER_NOEC(irq5_entry, IRQ_OFFSET+5);
	TRAPHANDLER_NOEC(irq6_entry, IRQ_OFFSET+6);
	TRAPHANDLER_NOEC(irq7_entry, IRQ_OFFSET+7); //IRQ_SPURIOUS
	TRAPHANDLER_NOEC(irq8_entry, IRQ_OFFSET+8);
	TRAPHANDLER_NOEC(irq9_entry, IRQ_OFFSET+9);
	TRAPHANDLER_NOEC(irq10_entry, IRQ_OFFSET+10);
	TRAPHANDLER_NOEC(irq11_entry, IRQ_OFFSET+11);
	TRAPHANDLER_NOEC(irq12_entry, IRQ_OFFSET+12);
	TRAPHANDLER_NOEC(irq13_entry, IRQ_OFFSET+13);
	TRAPHANDLER_NOEC(irq14_entry, IRQ_OFFSET+14); //IRQ_IDE

/*
 * Lab 3: Your code here for _alltraps
 * This code has finished for you ,so just read it.
 */
_alltraps:
	# Build trap frame.
	pushl %ds
	pushl %es
	pushal
	# Set up data segments.
	movl $GD_KD, %eax
	movw %ax,%ds
	movw %ax,%es

	# Call trap(tf), where tf=%esp
	pushl %esp
	call trap
	popl %esp

	# Cleanup pushes and ret
	popal
	popl %es
	popl %ds
	iret

