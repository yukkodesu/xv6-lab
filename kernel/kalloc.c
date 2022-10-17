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

struct run
{
  struct run *next;
};

struct kmem
{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void kinit()
{
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  int now_cpuid;
  push_off();
  now_cpuid = cpuid();
  pop_off();
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmems[now_cpuid].lock);
  r->next = kmems[now_cpuid].freelist;
  kmems[now_cpuid].freelist = r;
  release(&kmems[now_cpuid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int now_cpuid;
  push_off();
  now_cpuid = cpuid();
  pop_off();
  acquire(&kmems[now_cpuid].lock);
  r = kmems[now_cpuid].freelist;
  if (r)
  {
    kmems[now_cpuid].freelist = r->next;
    release(&kmems[now_cpuid].lock);
  }
  else
  {
    release(&kmems[now_cpuid].lock);
    for (int i = 0; i < NCPU; i++)
    {
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if (!r)
      {
        release(&kmems[i].lock);
        continue;
      }
      kmems[i].freelist = r->next;
      release(&kmems[i].lock);
      break;
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
