// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "defs.h"
#include "types.h"
#include "param.h"
#include "mmu.h"
#include "memlayout.h"
#include<stddef.h>
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *prev;
  struct run *next;
};

struct {
  int use_lock;	
  struct spinlock lock;
  struct run *freelist1, *freelist2;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  freerange(vstart, vend);
  kmem.use_lock = 0;
  kmem.freelist2 = NULL;	
  kmem.freelist1 = NULL;
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r, *tmp;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  
  r = (struct run*)v;

  if(kmem.freelist1 == NULL && kmem.freelist2 == NULL){
  	kmem.freelist1 = kmem.freelist2 = r;
  	r->next = r->prev = r;
  }
  
  else{
  	tmp = kmem.freelist2;
  	tmp->next->prev = r;
  	r->next = tmp->next;
  	tmp->next = r;
  	r->prev = tmp;
 	kmem.freelist1 = r;
  }

  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r, *tmp;

  if(kmem.use_lock)
    acquire(&kmem.lock);

  r = kmem.freelist1;
  if(kmem.freelist1 == kmem.freelist2){
	kmem.freelist1 = kmem.freelist2 = NULL;

  }
  
  else{
  	tmp = kmem.freelist2;
  	if(r){
    		kmem.freelist1 = r->next;
    		tmp->next = r->next;
    		r->next->prev = tmp;
  	}
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

