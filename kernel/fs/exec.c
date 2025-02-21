#include "fs/elf.h"
#include "fs/inode.h"
#include "fs/log.h"
#include "lib/string.h"
#include "memlayout.h"
#include "mm/mm.h"
#include "printk.h"
#include "riscv.h"
#include "sched/cpu.h"

static int flags2perm(int flags)
{
	int perm = 0;
	if (flags & 0x1)
		perm = PTE_X;
	if (flags & 0x2)
		perm |= PTE_W;
	return perm;
}

static int load_seg(pte_t *page_table, uint64_t va, struct m_inode *inode,
		    size_t off, size_t sz)
{
	size_t i, n;
	uint64_t pa;

	for (i = 0; i < sz; i += PAGE_SIZE) {
		pa = uvm_walk_addr(page_table, va + i);
		if (!pa)
			panic("the address does not exist when loading segment");
		if (sz - i < PAGE_SIZE)
			n = sz - i;
		else
			n = PAGE_SIZE;
		if (readi(inode, false, pa, off + i, n) != n)
			return -1;
	}
	return 0;
}

int do_execve(char *path, char **argv, char **env)
{
	struct elfhdr elf;
	struct proghdr ph;
	pte_t *new_page_table, *old_page_table;
	uint64_t new_sz, old_sz;
	uint64_t uargc, uargv[MAX_ARGS], uenv[MAX_ENVS];
	uint64_t sp;
	uint16_t i;
	uint64_t off;
	size_t len;
	uint64_t ret;
	struct m_inode *inode;
	struct process *p;
	char *name;

	p = running_proc();
	new_page_table = NULL;
	new_sz = 0;

	begin_op();

	if (!(inode = namei(path))) {
		end_op();
		return -1;
	}

	ilock(inode);

	if (readi(inode, false, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf))
		goto bad;

	if (elf.magic != ELF_MAGIC)
		goto bad;

	if (!(new_page_table = get_user_page_table(p)))
		goto bad;

	for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
		if (readi(inode, false, (uint64_t)&ph, off, sizeof(ph)) !=
		    sizeof(ph))
			goto bad;
		if (ph.type != ELF_PROG_LOAD)
			continue;
		if (ph.memsz < ph.filesz)
			goto bad;
		if (ph.vaddr + ph.memsz < ph.vaddr)
			goto bad;
		if (ph.vaddr % PAGE_SIZE != 0)
			goto bad;
		if (!(ret = uvm_alloc(new_page_table, new_sz,
				      ph.vaddr + ph.memsz,
				      flags2perm(ph.flags))))
			goto bad;
		new_sz = ret;
		if (load_seg(new_page_table, ph.vaddr, inode, ph.off,
			     ph.filesz))
			goto bad;
	}

	iunlock(inode);
	iput(inode);
	end_op();
	inode = NULL;

	/* copy 'argv' strings */
	sp = USER_STACK_TOP;
	for (uargc = 0; argv[uargc]; uargc++) {
		if (uargc >= MAX_ARGS)
			goto bad;
		len = strlen(argv[uargc]) + 1;
		sp -= len;
		sp -= sp % 16;
		if (sp < USER_STACK_BASE)
			goto bad;
		if (copy_out(new_page_table, sp, argv[uargc], len))
			goto bad;
		uargv[uargc] = sp;
	}
	uargv[uargc] = 0;
	len = (uargc + 1) * sizeof(uint64_t);
	sp -= len;
	sp -= sp % 16;
	if (sp < USER_STACK_BASE)
		goto bad;
	if (copy_out(new_page_table, sp, uargv, len))
		goto bad;
	p->tf->a1 = sp;

	/* copy 'env' strings */
	for (i = 0; env[i]; i++) {
		if (i >= MAX_ENVS)
			goto bad;
		len = strlen(env[i]) + 1;
		sp -= len;
		sp -= sp % 16;
		if (sp < USER_STACK_BASE)
			goto bad;
		if (copy_out(new_page_table, sp, env[i], len))
			goto bad;
		uenv[i] = sp;
	}
	uenv[i] = 0;
	len = (i + 1) * sizeof(uint64_t);
	sp -= len;
	sp -= sp % 16;
	if (sp < USER_STACK_BASE)
		goto bad;
	if (copy_out(new_page_table, sp, uenv, len))
		goto bad;
	p->tf->a2 = sp;

	/* save the pathname as the process name */
	name = strrchr(path, '/');
	if (!name)
		name = path;
	else
		name++;
	strncpy(p->name, name, sizeof(p->name));

	old_page_table = p->page_table;
	old_sz = p->size;
	p->page_table = new_page_table;
	p->size = new_sz;
	p->tf->epc = elf.entry;
	p->tf->sp = sp;
	free_user_page_table(old_page_table, old_sz);

	return uargc;

bad:
	if (new_page_table)
		free_user_page_table(new_page_table, new_sz);
	if (inode) {
		iunlock(inode);
		iput(inode);
		end_op();
	}
	return -1;
}
