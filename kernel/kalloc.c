// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(int cpuId, void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};
/*
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
*/

// edit: kazde jadro/CPU bude mat vlastny zoznam volnych stranok + vlastny zamok k nim 
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// edit: add
struct spinlock allLock; ////////////////////////////////////////////////??????????????????????????????????

void
kinit()
{
  //initlock(&kmem.lock, "kmem");
  //freerange(end, (void*)PHYSTOP);
  
  initlock(&allLock, "kmem.all");
  // velkost sections na ktore sa prerozdelia volne stranky?
  uint64 partLen = (PHYSTOP - (uint64)end) >> 3;
  for (uint64 i = 0; i < NCPU; i++) {
    // inicializuje zamky zoznamov jednotlivych CPU
    initlock(&kmem[i].lock, "kmem");
    // prerozdeli volne stranky medzi CPU a zavola freerange, aby sa pripisali do prislusnych zoznamov CPUs
    void *st = (void*) PGROUNDUP((uint64) (end) + i*partLen);
    void *ed = (void*) PGROUNDUP((uint64) (end) + (i+1)*partLen);
    freerange(i, st, ed);
  
  }
}

// edit: add int cpuId as param
// pridava uvolnene stranky do zoznamu volnych stranok?
//  moze pridelit vsetku volnu pamat tomu CPU, ktore ju zavola
void
freerange(int cpuId, void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // stranku zmeni na node zoznamu
    struct run *r = (struct run*) p;
    // next noveho node napoji na zoznam volnych stranok
    r->next = kmem[cpuId].freelist;
    kmem[cpuId].freelist = r;   // freelist ukazuje na zaciatok zretaz zoznamu volnych stranok
  
  }
   // kfree(p);
}

// funkcia na kradnutie volnych stranok ineho CPU ak nejake ine CPU nema ziadne volne
void steal(int cpuId) {

  release(&kmem[cpuId].lock); // treba releasnut lock cpucka pre ktore hladame stranky, aby sa mohol zamknut global lock vsetkych cpu
  acquire(&allLock);

  // iterovanie cez CPU, skip CPU pre ktore hladame volne stranky
  for (int i = 0; i < NCPU; i++) {
    if (i == cpuId)
      continue;

    // steal
    acquire(&kmem[i].lock); // lock pre vybrane cpu na kradnutie

    // ak cpu ma nejake volne stranky, tak sa preiteruje na koniec zoznamu resp zaciatok pretoze nodes sa vkladaju na zaciatok, takze prvy node ja na konci (ala zasobnik)
    if (kmem[i].freelist) {
      struct run* s = kmem[i].freelist;
      while (s->next)
        s = s->next;
      // zoznam volnych stranok resp jeho prvy node sa napoji na ukazovatel zaciatku volnych stranok CPU, ktory kradne
      s->next = kmem[cpuId].freelist;
      // ukazovatel volnych stranok kradnuceho CPU teraz ukazuje na zoznam volnych stranok okradaneho CPU
      kmem[cpuId].freelist = kmem[i].freelist;
      // okradany CPU nebude mat ziaden zoznam (NULL)
      kmem[i].freelist = 0;
    }

    release(&kmem[i].lock);

  }

  acquire(&kmem[cpuId].lock);
  release(&allLock);
}


// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  // edit: add
  push_off(); // zastavi prerusenia
  int cpuId = cpuid(); // vrati cislo aktualne pouzivaneho jadra


  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  /*acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);*/

  acquire(&kmem[cpuId].lock);
  r->next = kmem[cpuId].freelist;
  kmem[cpuId].freelist = r;
  release(&kmem[cpuId].lock);
  pop_off(); // spustenie preruseni
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{

  //edit: add
  push_off();

  struct run *r;

  // edit: add
  int cpuId = cpuid();

  // edit: add [cpuId]
  acquire(&kmem[cpuId].lock);
  r = kmem[cpuId].freelist;

  // edit: add
  if (!r) {
    steal(cpuId);
    r = kmem[cpuId].freelist;
  }


  if(r)
    kmem[cpuId].freelist = r->next;
  release(&kmem[cpuId].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
