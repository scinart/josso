/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
    int32_t ret = 0;
	
	switch (syscallno) {
	  case SYS_cputs:
		  sys_cputs((char *)a1, (size_t)a2);
		  break;
	  case SYS_cgetc:
		  ret = sys_cgetc();
		  break;
	  case SYS_getenvid:
		  ret = sys_getenvid();
		  break;
	  case SYS_env_destroy:
		  ret = sys_env_destroy((envid_t)a1);
		  break;
	  /* case SYS_yield: */
	  /* 	  sys_yield(); */
	  /* 	  break; */
	  /* case SYS_exofork: */
	  /* 	  ret = sys_exofork(); */
	  /* 	  break; */
	  /* case SYS_env_set_status: */
	  /* 	  ret = sys_env_set_status((envid_t)a1, a2); */
	  /* 	  break; */
	  /* case SYS_page_alloc: */
	  /* 	  ret = sys_page_alloc((envid_t)a1, (void *)a2, a3); */
	  /* 	  break; */
	  /* case SYS_page_map: */
	  /* 	  ret = sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, a5); */
	  /* 	  break; */
	  /* case SYS_page_unmap: */
	  /* 	  ret = sys_page_unmap((envid_t)a1, (void *)a2); */
	  /* 	  break; */
	  /* case SYS_env_set_pgfault_upcall: */
	  /* 	  ret = sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2); */
	  /* 	  break; */
	  /* case SYS_ipc_recv: */
	  /* 	  ret = sys_ipc_recv((void *)a1); */
	  /* 	  break; */
	  /* case SYS_ipc_try_send: */
	  /* 	  ret = sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4); */
	  /* 	  break; */
	  /* case SYS_env_set_trapframe: */
	  /* 	  ret = sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2); */
	  /* 	  break; */
	  /* case SYS_fs_wait: */
	  /* 	  ret = sys_fs_wait(); */
	  /* 	  break; */
	  default:
		  cprintf("\nUnhandled syscall %e.\n", syscallno);
		  return -E_INVAL;
    }
    return ret;
}

