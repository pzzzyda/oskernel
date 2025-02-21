#include "mm/mm.h"
#include "lib/string.h"
#include "memlayout.h"
#include "printk.h"
#include "riscv.h"
#include "sched/cpu.h"

struct free_list_node {
	struct free_list_node *next;
};

struct free_list {
	struct free_list_node *head;
	struct spin_lock lock;
};

extern char _text_start[];
extern char _text_end[];
extern char _kernel_end[];
extern char trampoline[];
static struct free_list kernel_free_list;
static pte_t *kernel_page_table;

#define TEXT_START ((uint64_t)_text_start)
#define TEXT_END ((uint64_t)_text_end)
#define KERNEL_END ((uint64_t)_kernel_end)

void pm_init(void)
{
	uint64_t ptr;
	kernel_free_list.head = NULL;
	spin_lock_init(&kernel_free_list.lock, "free_list");
	for (ptr = KERNEL_END; ptr < MAX_PADDR; ptr += PAGE_SIZE) {
		((struct free_list_node *)ptr)->next = kernel_free_list.head;
		kernel_free_list.head = (struct free_list_node *)ptr;
	}
}

void *pm_alloc(void)
{
	void *ptr = NULL;
	spin_lock_acquire(&kernel_free_list.lock);
	if (kernel_free_list.head) {
		ptr = kernel_free_list.head;
		kernel_free_list.head = kernel_free_list.head->next;
	}
	spin_lock_release(&kernel_free_list.lock);
	return ptr;
}

void *pm_zalloc(void)
{
	void *ptr = pm_alloc();
	if (ptr)
		memset(ptr, 0, PAGE_SIZE);
	return ptr;
}

void pm_free(void *ptr)
{
	spin_lock_acquire(&kernel_free_list.lock);
	((struct free_list_node *)ptr)->next = kernel_free_list.head;
	kernel_free_list.head = ptr;
	spin_lock_release(&kernel_free_list.lock);
}

pte_t *walk(pte_t *page_table, uint64_t va, bool alloc)
{
	int level;
	pte_t *pte;

	if (va >= MAX_VADDR)
		return NULL;

	for (level = 2; level > 0; level--) {
		pte = &page_table[PX(va, level)];
		if (*pte & PTE_V) {
			page_table = (pte_t *)PTE2PA(*pte);
		} else if (alloc) {
			page_table = pm_zalloc();
			if (!page_table)
				return NULL;
			*pte = PA2PTE(page_table) | PTE_V;
		} else {
			return NULL;
		}
	}
	return &page_table[PX(va, 0)];
}

int map_pages(pte_t *page_table, uint64_t va, uint64_t pa, size_t size,
	      uint64_t perm)
{
	uint64_t cur_va, last;
	pte_t *pte;

	if ((va % PAGE_SIZE) != 0)
		panic("address is not aligned");
	if ((size % PAGE_SIZE) != 0)
		panic("memory size is not aligned");
	if (size == 0)
		panic("memory size is zero");

	cur_va = va;
	last = va + size;
	while (cur_va < last) {
		pte = walk(page_table, cur_va, true);
		if (!pte) {
			unmap_pages(page_table, va, cur_va - va, false);
			return -1;
		}
		if (*pte & PTE_V)
			panic("remapping");
		*pte = PA2PTE(pa) | PTE_V | perm;
		cur_va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	return 0;
}

void unmap_pages(pte_t *page_table, uint64_t va, size_t size, bool free)
{
	uint64_t cur_va, last;
	pte_t *pte;

	if ((va % PAGE_SIZE) != 0)
		panic("address is not aligned");
	if ((size % PAGE_SIZE) != 0)
		panic("memory size is not aligned");

	cur_va = va;
	last = va + size;
	while (cur_va < last) {
		pte = walk(page_table, cur_va, false);
		if (!pte)
			panic("walk invalid PTE");
		if ((*pte & PTE_V) == 0)
			panic("unmapped page");
		if (PTE_FLAGS(*pte) == PTE_V)
			panic("not a leaf");
		if (free)
			pm_free((void *)PTE2PA(*pte));
		*pte = 0;
		cur_va += PAGE_SIZE;
	}
}

static void free_page_table(pte_t *page_table)
{
	uint32_t i;
	pte_t pte;

	for (i = 0; i < 512; i++) {
		pte = page_table[i];
		if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
			free_page_table((pte_t *)PTE2PA(pte));
			page_table[i] = 0;
		} else if (pte & PTE_V) {
			panic("free a page table in use");
		}
	}

	pm_free(page_table);
}

pte_t *get_user_page_table(struct process *p)
{
	pte_t *page_table;
	void *ustack;

	if (!(page_table = pm_zalloc()))
		return 0;

	/* trampoline */
	if (map_pages(page_table, TRAMPOLINE, (uint64_t)trampoline, PAGE_SIZE,
		      PTE_R | PTE_X) != 0)
		goto bad0;
	/* trap frame */
	if (map_pages(page_table, TRAP_FRAME, (uint64_t)(p->tf), PAGE_SIZE,
		      PTE_R | PTE_W) != 0)
		goto bad1;
	/* user stack */
	if ((ustack = pm_alloc()) == 0)
		goto bad2;
	if (map_pages(page_table, USER_STACK_BASE, (uint64_t)ustack, PAGE_SIZE,
		      PTE_U | PTE_W | PTE_R) != 0)
		goto bad3;

	return page_table;

bad3:
	pm_free(ustack);
bad2:
	unmap_pages(page_table, TRAP_FRAME, PAGE_SIZE, false);
bad1:
	unmap_pages(page_table, TRAMPOLINE, PAGE_SIZE, false);
bad0:
	pm_free(page_table);
	return 0;
}

void free_user_page_table(pte_t *page_table, size_t size)
{
	unmap_pages(page_table, USER_STACK_BASE, PAGE_SIZE, true);
	unmap_pages(page_table, TRAP_FRAME, PAGE_SIZE, false);
	unmap_pages(page_table, TRAMPOLINE, PAGE_SIZE, false);
	uvm_free(page_table, size);
}

static void copy_user_stack(pte_t *dst, pte_t *src)
{
	uint64_t dst_stack = uvm_walk_addr(dst, USER_STACK_BASE);
	uint64_t src_stack = uvm_walk_addr(src, USER_STACK_BASE);
	memmove((void *)dst_stack, (void *)src_stack, PAGE_SIZE);
}

int copy_user_page_table(pte_t *dst, pte_t *src, size_t size)
{
	void *mem;
	pte_t *pte;
	uint64_t pa, va, flag;

	for (va = 0; va < size; va += PAGE_SIZE) {
		if (!(pte = walk(src, va, false)))
			panic("walk invalid PTE");
		if (!(*pte & PTE_V))
			panic("page not present");
		pa = PTE2PA(*pte);
		flag = PTE_FLAGS(*pte);
		if (!(mem = pm_alloc()))
			goto failed;
		memmove(mem, (const void *)pa, PAGE_SIZE);
		if ((map_pages(dst, va, (uint64_t)mem, PAGE_SIZE, flag))) {
			pm_free(mem);
			goto failed;
		}
	}

	copy_user_stack(dst, src);

	return 0;

failed:
	unmap_pages(dst, 0, va, true);
	return -1;
}

int copy_in(pte_t *page_table, void *dst, uint64_t src, size_t n)
{
	uint64_t va, pa;
	size_t len;

	while (n > 0) {
		va = PAGE_ROUND_DOWN(src);
		pa = uvm_walk_addr(page_table, va);
		if (!pa)
			return -1;
		len = PAGE_SIZE - (src - va);
		if (len > n)
			len = n;
		memmove(dst, (void *)(pa + (src - va)), len);
		n -= len;
		dst += len;
		src += len;
	}
	return 0;
}

int copy_out(pte_t *page_table, uint64_t dst, void *src, size_t n)
{
	uint64_t va, pa;
	size_t len;
	pte_t *pte;

	while (n > 0) {
		va = PAGE_ROUND_DOWN(dst);
		if (va >= MAX_VADDR)
			return -1;
		pte = walk(page_table, va, false);
		if (!pte || !(*pte & PTE_V) || !(*pte & PTE_U) ||
		    !(*pte & PTE_W))
			return -1;
		pa = PTE2PA(*pte);
		len = PAGE_SIZE - (dst - va);
		if (len > n)
			len = n;
		memmove((void *)(pa + (dst - va)), src, len);
		n -= len;
		src += len;
		dst += len;
	}
	return 0;
}

int copy_str_in(pte_t *page_table, char *dst, uint64_t src, size_t max_len)
{
	uint64_t va, pa;
	size_t len;
	char *s;

	while (max_len > 0) {
		va = PAGE_ROUND_DOWN(src);
		pa = uvm_walk_addr(page_table, va);
		if (pa == 0)
			return -1;
		len = PAGE_SIZE - (src - va);
		if (len > max_len)
			len = max_len;
		s = (char *)(pa + (src - va));
		for (; len > 0; len--) {
			*dst = *s;
			if (*s == 0)
				return 0;
			dst++;
			s++;
		}
		max_len -= len;
	}
	return 0;
}

int copy_str_out(pte_t *page_table, uint64_t dst, char *src, size_t max_len)
{
	uint64_t va, pa;
	size_t len;
	char *s;
	pte_t *pte;

	while (max_len > 0) {
		va = PAGE_ROUND_DOWN(dst);
		pte = walk(page_table, va, false);
		if (!pte || !(*pte & PTE_V) || !(*pte & PTE_U) ||
		    !(*pte & PTE_W))
			return -1;
		pa = PTE2PA(*pte);
		len = PAGE_SIZE - (dst - va);
		if (len > max_len)
			len = max_len;
		s = (char *)(pa + (dst - va));
		for (; len > 0; len--) {
			*s = *src;
			if (*src == 0)
				return 0;
			s++;
			src++;
		}
		max_len -= len;
	}
	return 0;
}

int either_copy_out(bool to_user, uint64_t dst, void *src, size_t n)
{
	if (to_user) {
		return copy_out(running_proc()->page_table, dst, src, n);
	} else {
		memmove((void *)dst, src, n);
		return 0;
	}
}

int either_copy_in(bool from_user, void *dst, uint64_t src, size_t n)
{
	if (from_user) {
		return copy_in(running_proc()->page_table, dst, src, n);
	} else {
		memmove(dst, (void *)src, n);
		return 0;
	}
}

static void map_kernel_stacks(pte_t *page_table)
{
	uint32_t i;
	uint64_t va, pa;
	for (i = 0; i < N_PROC; i++) {
		pa = (uint64_t)pm_alloc();
		if (!pa)
			panic("no free page");
		va = KERNEL_STACK(i);
		if (map_pages(page_table, va, pa, PAGE_SIZE, PTE_R | PTE_W))
			panic("mapping failed");
	}
}

static void kvm_map(pte_t *page_table, uint64_t va, uint64_t pa, size_t size,
		    uint64_t perm)
{
	if (map_pages(page_table, va, pa, size, perm))
		panic("mapping failed");
}

static pte_t *kvm_make(void)
{
	pte_t *page_table = pm_zalloc();
	if (!page_table)
		panic("no free page");

	/* UART */
	kvm_map(page_table, UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);
	/* PLIC */
	kvm_map(page_table, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);
	/* virtio */
	kvm_map(page_table, VIRTIO0, VIRTIO0, PAGE_SIZE, PTE_R | PTE_W);
	/* kernel text */
	kvm_map(page_table, TEXT_START, TEXT_START, TEXT_END - TEXT_START,
		PTE_R | PTE_X);
	/* memory */
	kvm_map(page_table, TEXT_END, TEXT_END, MAX_PADDR - TEXT_END,
		PTE_R | PTE_W);
	/* trampoline */
	kvm_map(page_table, TRAMPOLINE, (uint64_t)trampoline, PAGE_SIZE,
		PTE_R | PTE_X);
	/* kernel stacks */
	map_kernel_stacks(page_table);

	return page_table;
}

void kvm_init(void)
{
	kernel_page_table = kvm_make();
}

void kvm_init_hart(void)
{
	sfence_vma();
	write_satp(MAKE_SATP(kernel_page_table));
	sfence_vma();
}

uint64_t uvm_alloc(pte_t *page_table, uint64_t old_sz, uint64_t new_sz,
		   uint64_t xperm)
{
	uint64_t a;
	void *mem;

	if (new_sz <= old_sz)
		return old_sz;
	old_sz = PAGE_ROUND_UP(old_sz);
	for (a = old_sz; a < new_sz; a += PAGE_SIZE) {
		mem = pm_zalloc();
		if (!mem) {
			uvm_dealloc(page_table, a, old_sz);
			return 0;
		}
		if (map_pages(page_table, a, (uint64_t)mem, PAGE_SIZE,
			      PTE_R | PTE_U | xperm)) {
			pm_free(mem);
			uvm_dealloc(page_table, a, old_sz);
			return 0;
		}
	}
	return new_sz;
}

uint64_t uvm_dealloc(pte_t *page_table, uint64_t old_sz, uint64_t new_sz)
{
	if (new_sz >= old_sz)
		return old_sz;
	old_sz = PAGE_ROUND_UP(old_sz);
	new_sz = PAGE_ROUND_UP(new_sz);
	unmap_pages(page_table, new_sz, old_sz - new_sz, true);
	return new_sz;
}

void uvm_free(pte_t *page_table, size_t size)
{
	if (size > 0)
		unmap_pages(page_table, 0, PAGE_ROUND_UP(size), true);
	free_page_table(page_table);
}

uint64_t uvm_walk_addr(pte_t *page_table, uint64_t va)
{
	pte_t *pte;
	uint64_t pa;

	if (va >= MAX_VADDR)
		panic("invalid virtual address");

	pte = walk(page_table, va, false);
	if (!pte)
		return 0;
	if (!(*pte & PTE_V))
		return 0;
	if (!(*pte & PTE_U))
		return 0;
	pa = PTE2PA(*pte);
	return pa;
}
