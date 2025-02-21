#ifndef _MM_H
#define _MM_H

#include "types.h"

struct process;

typedef uint64_t pte_t;

void pm_init(void);
void *pm_alloc(void);
void *pm_zalloc(void);
void pm_free(void *ptr);

void kvm_init(void);
void kvm_init_hart(void);

pte_t *walk(pte_t *page_table, uint64_t va, bool alloc);
int map_pages(pte_t *page_table, uint64_t va, uint64_t pa, size_t size,
	      uint64_t perm);
void unmap_pages(pte_t *page_table, uint64_t va, size_t size, bool free);

uint64_t uvm_alloc(pte_t *page_table, uint64_t old_sz, uint64_t new_sz,
		   uint64_t xperm);
uint64_t uvm_dealloc(pte_t *page_table, uint64_t old_sz, uint64_t new_sz);
void uvm_free(pte_t *page_table, size_t size);
uint64_t uvm_walk_addr(pte_t *page_table, uint64_t va);

pte_t *get_user_page_table(struct process *p);
void free_user_page_table(pte_t *page_table, size_t size);
int copy_user_page_table(pte_t *dst, pte_t *src, size_t size);

int copy_in(pte_t *page_table, void *dst, uint64_t src, size_t n);
int copy_out(pte_t *page_table, uint64_t dst, void *src, size_t n);
int copy_str_in(pte_t *page_table, char *dst, uint64_t src, size_t max_len);
int copy_str_out(pte_t *page_table, uint64_t dst, char *src, size_t max_len);
int either_copy_out(bool to_user, uint64_t dst, void *src, size_t n);
int either_copy_in(bool from_user, void *dst, uint64_t src, size_t n);

#endif
