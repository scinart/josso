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
#include <kern/sched.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	uintptr_t start = (uintptr_t) ROUNDDOWN(s, PGSIZE);
	uintptr_t end = (uintptr_t) ROUNDUP(s+len, PGSIZE);
	uintptr_t iter;
	int die = 0;
	for (iter=start; iter<end; iter+=PGSIZE)
	{
		int non_create = 0;
		pte_t * ppte = pgdir_walk(curenv->env_pgdir, (void*) iter, non_create);
		if (!ppte)
		{ //secondary page table not present or page not present.
			die = 1;
			break;
		}
		else if (!PAGE_PRESENT(*ppte))
		{
			die = 2;
			break;
		}
		else if (iter > ULIM)
		{ // 越界了
			die = 3;
			break;
		}
		else if ((*ppte & PTE_U) == 0)
		{ // no user permission.
			die = 4;
			break;
		}
	}
	if (die)
	{
		cprintf("Memory fault #%d, s is %p, len is %x\n", die, s, len);
		cprintf("[%08x] user_mem_check assertion failure for va %08x\n", curenv->env_id, iter+((uintptr_t)s-start));
		env_destroy(curenv);
		// maybe sys_yield() later.
		// fixme: no else clause.
	}

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

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env* pEnv;
	int parentid = sys_getenvid();
	int r = env_alloc(&pEnv, parentid);
	if (r < 0)
		return -E_NO_FREE_ENV;

	pEnv->env_status = ENV_NOT_RUNNABLE;
	pEnv->env_tf = curenv->env_tf;
	(pEnv->env_tf).tf_regs.reg_eax = 0;

	return pEnv->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.

	if (!(status == ENV_RUNNABLE || status == ENV_NOT_RUNNABLE))
		return -E_INVAL;

	struct Env* pEnv;
	int r;
	if ((r = envid2env(envid, &pEnv, 1))) //aka -E_BAD_ENV
	{
		assert(r == -E_BAD_ENV);
		return r;
	}
	else
		pEnv->env_status = status;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env* pEnv;
	int r;
	if ((r = envid2env(envid, &pEnv, 1)))
		return -E_BAD_ENV;

	pEnv->env_pgfault_upcall = func;
	return 0;

	// panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	struct Env* pe;
	int r;
	if ((r = envid2env(envid, &pe, 1)))
		return r;//-E_BAD_ENV;
	if ((ROUNDUP(va, PGSIZE) != va) || va >= (void*)UTOP)
		return -E_INVAL;
	if ((!(perm&PTE_U)) || !(perm&PTE_P) || (perm&~PTE_U&~PTE_P&~PTE_AVAIL&~PTE_W))
		return -E_INVAL;
	struct PageInfo * ppi = page_alloc(ALLOC_ZERO);
	if (!ppi)
		return -E_NO_MEM;

	if ((r = page_insert(pe->env_pgdir, ppi, va, perm | PTE_U | PTE_P)))
		return r;

	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	// ref to 北大报告 (懒着写了)。
	if ((srcva >= (void *)UTOP) || (srcva != ROUNDUP(srcva, PGSIZE)) ||
		(dstva >= (void *)UTOP) || (dstva != ROUNDUP(dstva, PGSIZE)))
		return -E_INVAL;
	if ( !(perm & PTE_U) || !(perm & PTE_P) || ((perm & ~PTE_SYSCALL) != 0))
		return -E_INVAL;
	struct Env *srcenv, *dstenv;
	int r;
	if ((r = envid2env(srcenvid, &srcenv, 1)) < 0)
		return -E_BAD_ENV;
	if ((r = envid2env(dstenvid, &dstenv, 1)) < 0)
		return -E_BAD_ENV;
	pte_t *ppte;
	struct PageInfo *ppi;
	ppi = page_lookup(srcenv->env_pgdir, srcva, &ppte);
	if ((ppi == NULL) || ((perm & PTE_W) != 0 && (*ppte & PTE_W) == 0))
		return -E_INVAL;
	if ((r = page_insert(dstenv->env_pgdir, ppi, dstva, perm)) < 0)
		return -E_NO_MEM;
	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	if((va >= (void *)UTOP) || (va != ROUNDUP(va, PGSIZE)))
		return -E_INVAL;
	struct Env *pe;
	int r;
	if ((r = envid2env (envid, &pe, 1)) < 0)
		return -E_BAD_ENV;
	page_remove(pe->env_pgdir, va);
	return 0;
}

static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	struct Env *dstenv;
	int r;
// get target env (may not exist)
	if ((r = envid2env (envid, &dstenv, 0)) < 0)
		return -E_BAD_ENV;
// if envid is not currently blocked in sys_ipc_recv, or another environment managed to send first.
		if (!dstenv->env_ipc_recving || dstenv->env_ipc_from != 0)
			return -E_IPC_NOT_RECV;
// if srcva < UTOP but srcva is not page-aligned
	if (srcva < (void *)UTOP && ROUNDDOWN(srcva, PGSIZE) != srcva)
		return -E_INVAL;
// if srcva < UTOP and perm is inappropriate (see sys_page_alloc)
	if (
		srcva < (void *)UTOP
		&&(!(perm & PTE_U)||!(perm & PTE_P)||(perm & ~PTE_SYSCALL)!=0))
		return -E_INVAL;
	pte_t *pte;
	struct PageInfo *pp;
// if srcva < UTOP but srcva is not mapped in the caller's address space.
		if (srcva < (void *)UTOP && (pp = page_lookup (curenv->env_pgdir, srcva, &pte)) == NULL)
			return -E_INVAL;
// if (perm & PTE_W), but srcva is read-only in the current environment's address space.
	if (srcva < (void *)UTOP && (perm & PTE_W) > 0 && (*pte & PTE_W) == 0)
		return -E_INVAL;
// send a page
// if there's not enough memory to map srcva in envid's address space.
	if (
		srcva < (void *)UTOP
		&& dstenv->env_ipc_dstva != 0) {
		r = page_insert (dstenv->env_pgdir, pp, dstenv->env_ipc_dstva,
						 perm);
		if (r < 0)
			return -E_NO_MEM;
		dstenv->env_ipc_perm = perm;
	}
	dstenv->env_ipc_from = curenv->env_id;
	dstenv->env_ipc_value = value;
	dstenv->env_status = ENV_RUNNABLE;
	dstenv->env_ipc_recving = 0;
	dstenv->env_tf.tf_regs.reg_eax = 0;
// sys_ipc_recv will return 0
	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if (dstva < (void *)UTOP && ROUNDDOWN (dstva, PGSIZE) != dstva)
		return -E_INVAL;
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_from = 0;
	curenv->env_status = ENV_NOT_RUNNABLE;
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
	  case SYS_yield:
	  	  sys_yield();
	  	  break;
	  case SYS_exofork:
	  	  ret = sys_exofork();
	  	  break;
	  case SYS_env_set_status:
	  	  ret = sys_env_set_status((envid_t)a1, a2);
	  	  break;
	  case SYS_page_alloc:
	  	  ret = sys_page_alloc((envid_t)a1, (void *)a2, a3);
	  	  break;
	  case SYS_page_map:
	  	  ret = sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, a5);
	  	  break;
	  case SYS_page_unmap:
	  	  ret = sys_page_unmap((envid_t)a1, (void *)a2);
	  	  break;
	  case SYS_env_set_pgfault_upcall:
	  	  ret = sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
	  	  break;
	  case SYS_ipc_recv:
	  	  ret = sys_ipc_recv((void *)a1);
	  	  break;
	  case SYS_ipc_try_send:
	  	  ret = sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
	  	  break;
	  // case SYS_env_set_trapframe:
	  // 	  ret = sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
	  // 	  break;
	  /* case SYS_fs_wait: */
	  /* 	  ret = sys_fs_wait(); */
	  /* 	  break; */
	  default:
		  cprintf("\nUnhandled syscall %e.\n", syscallno);
		  return -E_INVAL;
    }
    return ret;
}

