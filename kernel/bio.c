// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13     // edit: lab zamky 2 uloha

struct {
  struct spinlock bigLock; // edit: lab zamky
  struct spinlock lock[NBUCKETS];  // edit: lab zamky : pole zamkov na kazdy bucket
  struct buf buf[NBUF]; // edit: 30 bufferov, ktore prerozdelime do bucketov

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;      // edit: lab zamky
  
  // edit: pole NBUCKETS zretazenych zoznamov = hasovacia tabulka NBUCKETS bucketov
  struct buf head[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  //initlock(&bcache.lock, "bcache"); //edit
  initlock(&bcache.bigLock, "globalLock");

  // edit: inicializacia pola zamkov bucketov
  for(int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache.bucket");

    // inicializacia zretazenych zoznamov resp len ich "head" - prva bunka pre kazdy bucket
    // jeho prev aj next ukazuje sam na seba (za normalnych okolnosti by ukazovali na NULL)
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  // Create linked list of buffers
  //bcache.head.prev = &bcache.head; //edit
  //bcache.head.next = &bcache.head; //edit

  // edit: samotne pridanie bufferov do zoznamu resp. 0.-eho bucketu(neskor sa roztriedia)
  // edit: vsade sa pridalo to [0]
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){      
    b->blockno = 0;
    // kuk nakres v pozn
    // 1) next noveho node sa napoji na next headu, a teda strci sa medzi head a prvy buff
    b->next = bcache.head[0].next;
    // 2) prev noveho node bude ukazovat na head
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    // 3) prev povodne prveho node (teraz druhy) bude ukazovat na novy node
    bcache.head[0].next->prev = b;
    // 4) next headu bude ukazovat na novy node
    bcache.head[0].next = b;
  }
}


// premiestni buffer z jedneho bucketu do druheho "int bucket"
// oba zamky musia byt uzamknute
// yanked z bresle()
void
move_bucket(struct buf *b, int bucket){ //edit
    // edit: vytiahnutie buff zo stareho bucketu
    // node co ide po bcku bude teraz ukazovat s prev na ten co bol pred bckom
    b->next->prev = b->prev;
    // node pred bckom bude s next ukazovat na node co ide po bcku
    b->prev->next = b->next;
    // edit: napojenie na prvy node/zaznam v buckete next bcka nan bude ukazovat
    b->next = bcache.head[bucket].next;
    // prev noveho node ukazuje na head
    b->prev = &bcache.head[bucket];
    // edit: povodne prvy node bude ukazovat s prev na novy node (novy node sa strci pred prvy)
    bcache.head[bucket].next->prev = b;
    // edit: ukazovatel head bude teraz ukazovat na novy node
    bcache.head[bucket].next = b;
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.


/*
static struct buf*
bget(uint dev, uint blockno) {

  struct buf *b;
  struct buf *LRU = bcache.buf;
  int current_bucket;
  int minBuffBucket = -1;

  int bucket = blockno % NBUCKETS;

  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  
  for(b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
     b->refcnt++;
     release(&bcache.lock[bucket]);
     acquiresleep(&b->lock); 
     return b;
    }
  }
  
  acquire(&bcache.bigLock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  for (b = bcache.buf; b != bcache.buf + NBUF; b = b + 1) {
    current_bucket = b->blockno & NBUCKETS;

    // 1)zamknem si current_bucket
    if (bucket != current_bucket && current_bucket != minBuffBucket)
      acquire(&bcache.lock[current_bucket]);


    if (b->refcnt == 0 && b->ticks < LRU->ticks) {
      // ak sme nasli nove min, stare min treba odomknut
      if (minBuffBucket != -1  && minBuffBucket != bucket && minBuffBucket != current_bucket)
        release(&bcache.lock[minBuffBucket]);
      LRU = b;
      minBuffBucket = current_bucket;
    }
    else {
      // 2)ak v current_buckete neni LRU buff tak ho mozem odomknut, ak je, tak ostava zamknuty
      // mozem ho odomknut len ak sa nerovna bucketu a minBuffBucketu
      if (bucket != current_bucket && current_bucket != minBuffBucket)
        release(&bcache.lock[current_bucket]);
    }
  }

    if (LRU == bcache.buf && LRU->refcnt != 0) {
      panic("bget: no buffers");
  }
  // ak sa nasiel nejaky LRU, tak uz ho mame zamknuty vid 1), 2)
  if (LRU) {
    LRU->dev = dev;
    LRU->blockno = blockno;
    LRU->valid = 0;
    LRU->refcnt = 1;

    if (bucket != minBuffBucket) {
      move_bucket(LRU, bucket);
      release(&bcache.lock[minBuffBucket]);
    }

    // odomknem bucket
    release(&bcache.lock[bucket]);

    // odomnkenm global lock
    release(&bcache.bigLock);

    acquiresleep(&LRU->lock);
    return LRU;
  }
  panic("bget: no buffers");

}
*/


static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket = blockno % NBUCKETS;

  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  for(b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //acquire(&bcache.bigLock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
   for(b = bcache.buf; b != bcache.buf + NBUF; b = b + 1) {
     int current_bucket = b->blockno % NBUCKETS;

      if(bucket != current_bucket)
         acquire(&bcache.lock[current_bucket]);

      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        if (bucket != current_bucket) {
          move_bucket(b, bucket);
          release(&bcache.lock[current_bucket]);
        }

        //release(&bcache.bigLock);
        release(&bcache.lock[bucket]);

        acquiresleep(&b->lock);
        return b;
      }

      if(bucket != current_bucket)
        release(&bcache.lock[current_bucket]);
  }

  panic("bget: no buffers");
}

//
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf* b;

  b = bget(dev, blockno);
  if (!b->valid){
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}


// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bucket = b->blockno % NBUCKETS;

  releasesleep(&b->lock);

  //b->ticks = ticks; //edit na sledovanie LRU

  //acquire(&bcache.lock); //edit
  acquire(&bcache.lock[bucket]);
  
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // edit: lab zamky : pouzity buffer vraciame na zaciatok zretazeneho zoznamu bufferov
    // vystrihnutie bcka zo zretaz zoznamu
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // edit: lab zmaky : prilepenie na zaciatok MRU listu
    // edit: [bucket]
    //  b ukazuje na head a nasledovnika head (strci sa medzi ne)
    b->next = bcache.head[bucket].next;
    b->prev = &bcache.head[bucket];
    // stary node ukazuje prev na nove bcko
    bcache.head[bucket].next->prev = b;
    // head ukazuje na b
    bcache.head[bucket].next = b; 
  }
  
  //release(&bcache.lock); //edit
  release(&bcache.lock[bucket]);
}

void
bpin(struct buf *b) {

  int bucket = b->blockno % NBUCKETS; //edit

  //acquire(&bcache.lock);  //edit
  acquire(&bcache.lock[bucket]);
  b->refcnt++;
  //release(&bcache.lock); // edit
  release(&bcache.lock[bucket]);
}

void
bunpin(struct buf *b) {

  int bucket = b->blockno % NBUCKETS; //edit

  //acquire(&bcache.lock); //edit
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
  //release(&bcache.lock); //edit
  release(&bcache.lock[bucket]);
}


