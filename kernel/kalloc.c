// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem_cpu {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem_cpu kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  // If the local list is empty, steal roughly half of a donor's pages.
  if(r == 0){
    for(int donor = 0; donor < NCPU; donor++){
      if(donor == id)
        continue;
      acquire(&kmem[donor].lock);
      int count = 0;
      for(struct run *p = kmem[donor].freelist; p; p = p->next)
        count++;
      int take = (count + 1) / 2;
      struct run *stolen = kmem[donor].freelist;
      struct run *last = stolen;
      for(int n = 1; n < take && last; n++)
        last = last->next;
      if(last){
        kmem[donor].freelist = last->next;
        last->next = 0;
      }
      release(&kmem[donor].lock);

      if(stolen){
        r = stolen;
        struct run *rest = r->next;
        r->next = 0;
        if(rest){
          acquire(&kmem[id].lock);
          last = rest;
          while(last->next)
            last = last->next;
          last->next = kmem[id].freelist;
          kmem[id].freelist = rest;
          release(&kmem[id].lock);
        }
        break;
      }
    }
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
