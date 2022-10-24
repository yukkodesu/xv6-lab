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

#define NBUCKET 7
struct bcache {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void binit(void) {
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.lock[i], "bcache");
  }

  // Create linked list of buffers
  for (int i = 0; i < NBUCKET; i++) {
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  int i = 0;
  for (b = bcache.buf, i = 0; b < bcache.buf + NBUF; b++, i++) {
    b->next = bcache.head[i % NBUCKET].next;
    b->prev = &bcache.head[i % NBUCKET];
    initsleeplock(&b->lock, "buffer");
    bcache.head[i % NBUCKET].next->prev = b;
    bcache.head[i % NBUCKET].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;
  int this_bucket = blockno % NBUCKET;
  acquire(&bcache.lock[this_bucket]);

  // Is the block already cached?
  for (b = bcache.head[this_bucket].next; b != &bcache.head[this_bucket];
       b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[this_bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.head[this_bucket].prev; b != &bcache.head[this_bucket];
       b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[this_bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (int i = (this_bucket + 1) % NBUCKET; i != this_bucket;
       i = (i + 1) % this_bucket) {
    acquire(&bcache.lock[i]);
    for (b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev) {
      if (b->refcnt == 0) {
        // delete from bucket i
        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache.lock[i]);
        // add to this_bucket
        b->next = bcache.head[this_bucket].next;
        b->prev = &bcache.head[this_bucket];
        bcache.head[this_bucket].next->prev = b;
        bcache.head[this_bucket].next = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[this_bucket]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int this_bucket = b->blockno % NBUCKET;

  acquire(&bcache.lock[this_bucket]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[this_bucket].next;
    b->prev = &bcache.head[this_bucket];
    bcache.head[this_bucket].next->prev = b;
    bcache.head[this_bucket].next = b;
  }

  release(&bcache.lock[this_bucket]);
}

void bpin(struct buf *b) {
  int this_bucket = b->blockno % NBUCKET;
  acquire(&bcache.lock[this_bucket]);
  b->refcnt++;
  release(&bcache.lock[this_bucket]);
}

void bunpin(struct buf *b) {
  int this_bucket = b->blockno % NBUCKET;
  acquire(&bcache.lock[this_bucket]);
  b->refcnt--;
  release(&bcache.lock[this_bucket]);
}
