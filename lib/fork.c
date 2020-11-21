// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.


	if (!(err & FEC_WR)) {
		panic("pgfault: faulting access was not write\n");
	}	
	if (!(((pte_t *)uvpt)[PGNUM(addr)] & PTE_COW)) {
		panic("pgfault: faulting access was not to a copy-on-write page\n");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	// panic("pgfault not implemented");

	envid_t envid = sys_getenvid();

	r = sys_page_alloc(envid, (void *)PFTEMP, PTE_U | PTE_P | PTE_W);
	if (r < 0) {
		panic("pgfault: can't allocate a new page\n");
	}

	// copy the data from the old page to the new page
	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy((void *)PFTEMP, addr, PGSIZE);

	// move the new page to the old page's address
	r = sys_page_map(envid, (void *)PFTEMP, envid, addr, PTE_U | PTE_P | PTE_W);
	if (r < 0) {
		panic("pgfault: can't move the new page to old page's address\n");
	}

	// unmap the temporary location
	r = sys_page_unmap(envid, PFTEMP);
	if (r < 0) {
		panic("pgfault: can't unmap PFTEMP\n");
	}

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	// panic("duppage not implemented");

	uintptr_t va = pn * PGSIZE;
	envid_t current_envid = sys_getenvid();
	pte_t pte = uvpt[pn];

	int perm = PTE_U | PTE_P | PTE_COW;
	
	if (pte & PTE_SHARE) {
		// both env have read and write permission
		sys_page_map(current_envid, (void *)va, envid, (void *)va, PTE_SYSCALL);
	} else if ((pte & PTE_W) || (pte & PTE_COW)) {
		if ((r = sys_page_map(current_envid, (void *)va, envid, (void *)va, perm)) < 0) {
			return r;
		}
		if ((r = sys_page_map(current_envid, (void *)va, current_envid, (void *)va, perm)) < 0) {
			return r;
		}
	} else {
		// read-only page
		if ((r = sys_page_map(current_envid, (void *)va, envid, (void *)va, PTE_U | PTE_P)) < 0) {
			return r;
		}
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	// panic("fork not implemented");

	// set up page fault handler
	set_pgfault_handler(pgfault);

	// create a child
	envid_t child_id = sys_exofork();
	if (child_id < 0) {
		return -1;
	} else if (child_id == 0) {
		// fix thisenv in the child process
		thisenv = envs + ENVX(sys_getenvid());
		return child_id;
	}

	// copy address space to the child
	uintptr_t va = 0;
	int e;
	for (; va < UTOP; va += PGSIZE) {
		// check whether pde and pte exist
		if ((((pde_t *)uvpd)[PDX(va)] & PTE_P) && (((pte_t *)uvpt)[PGNUM(va)] & PTE_P)) {
			if (va == UXSTACKTOP - PGSIZE) {
				// ignore user exception stack
				continue;
			}
			e = duppage(child_id, PGNUM(va));
			if (e < 0) {
				return e;
			}
		}
	}

	// allocate a new page for the child's user exception stack
	e = sys_page_alloc(child_id, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W);
	if (e < 0) {
		return e;
	}

	// copy page fault handler to the childe
	extern void _pgfault_upcall(void);
	e = sys_env_set_pgfault_upcall(child_id, _pgfault_upcall);
	if (e < 0) {
		return e;
	}

	// mark the child as runnable
	e = sys_env_set_status(child_id, ENV_RUNNABLE);
	if (e < 0) {
		return e;
	}

	return child_id;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
