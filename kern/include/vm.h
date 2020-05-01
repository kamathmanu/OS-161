#ifndef _VM_H_
#define _VM_H_

#include <machine/vm.h>

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */

 /* Initialization function */
 void vm_bootstrap(void);

/*RANDOM DEFINITIONS*/

#define DUMBVM_STACKPAGES 12
#define JAFFVM_STACKPAGES 500
#define MIPS_KSEG0  0x80000000
#define KVADDR_TO_PADDR(vaddr) (vaddr - MIPS_KSEG0)

 /*******************COREMAP***************************/

 struct coremap_entry {
   paddr_t page_frame;
   int is_allocated;
   int chunk_size;
   struct addrspace * as;

 };

typedef struct coremap_entry coremap_entry;

extern struct coremap_entry * coremap;

// Number of pages taken up by coremap
extern u_int32_t coremap_base_page_offset;
extern u_int32_t total_coremap_entries;
extern u_int32_t num_free_coremap_entries;

//Flag to signal that thread_bootstrap is complete.
extern int booted;

/* Initialization function */
void coremap_bootstrap();

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);
void free_kpages(vaddr_t addr);

/*****************PAGE TABLE*************************/

#define NUMBER_OF_PAGE_TABLE_ENTRIES 1024

//2 level table which acts as a set-associative
//cache that

//Level 2 of the Page Table - an array of size total_coremap_entries

struct page_table_entry_array {
struct page_table_entry *second_level_page_table[NUMBER_OF_PAGE_TABLE_ENTRIES];
};

//structure for a single page table entry/PTE

struct page_table_entry {
 // vaddr_t vaddr;
 paddr_t pfn:20;
 unsigned int permissions:3; //READ: 001, WRITE: 010, XEC: 100
};

typedef struct page_table_entry page_table_entry;
typedef struct page_table_entry_array page_table_entry_array;

/*****************MEMORY SEGMENT STRUCTURE*************************/

//hold info for a segment in memory.
//Basic base and bounds method.

struct segment{
  vaddr_t v_base;
  size_t npages;
  unsigned int permissions;
};

typedef struct segment segment;
typedef struct page_table_entry *stack;
typedef struct page_table_entry *heap;

/***************TLB & Page Fault Handler*********************/

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

/*Fault codes*/

#define SIGSEGV 2

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

#endif /* _VM_H_ */
