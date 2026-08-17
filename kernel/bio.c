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

#define NBUCKET 13

struct {
  struct spinlock evict_lock;
  struct spinlock bucket_lock[NBUCKET];
  struct buf *bucket[NBUCKET];
  struct buf buf[NBUF];
} bcache;

static uint
bhash(uint dev, uint blockno)
{
  return (dev + blockno) % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.evict_lock, "bcache.evict");
  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache.bucket_lock[i], "bcache.bucket");
    bcache.bucket[i] = 0;
  }

  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->dev = 0;
    b->blockno = i;
    b->timestamp = 0;
    uint h = bhash(b->dev, b->blockno);
    b->next = bcache.bucket[h];
    bcache.bucket[h] = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint h = bhash(dev, blockno);
  acquire(&bcache.bucket_lock[h]);

  // Is the block already cached?
  for(b = bcache.bucket[h]; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      __sync_add_and_fetch(&b->refcnt, 1);
      release(&bcache.bucket_lock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[h]);

  // Serialize misses and lock every bucket so lookup plus insertion is atomic.
  acquire(&bcache.evict_lock);
  for(int i = 0; i < NBUCKET; i++)
    acquire(&bcache.bucket_lock[i]);

  // A concurrent miss may have inserted this block while we waited.
  for(b = bcache.bucket[h]; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      __sync_add_and_fetch(&b->refcnt, 1);
      for(int i = NBUCKET - 1; i >= 0; i--)
        release(&bcache.bucket_lock[i]);
      release(&bcache.evict_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf *victim = 0;
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    if(b->refcnt == 0 &&
       (victim == 0 || b->timestamp < victim->timestamp))
      victim = b;
  }
  if(victim == 0){
    for(int i = NBUCKET - 1; i >= 0; i--)
      release(&bcache.bucket_lock[i]);
    release(&bcache.evict_lock);
    panic("bget: no buffers");
  }

  uint oldh = bhash(victim->dev, victim->blockno);
  struct buf **link = &bcache.bucket[oldh];
  while(*link && *link != victim)
    link = &(*link)->next;
  if(*link == 0)
    panic("bget: victim");
  *link = victim->next;

  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;
  victim->next = bcache.bucket[h];
  bcache.bucket[h] = victim;

  for(int i = NBUCKET - 1; i >= 0; i--)
    release(&bcache.bucket_lock[i]);
  release(&bcache.evict_lock);
  acquiresleep(&victim->lock);
  return victim;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
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

  releasesleep(&b->lock);

  if(__sync_sub_and_fetch(&b->refcnt, 1) == 0)
    b->timestamp = ticks;
}

void
bpin(struct buf *b) {
  __sync_add_and_fetch(&b->refcnt, 1);
}

void
bunpin(struct buf *b) {
  if(__sync_sub_and_fetch(&b->refcnt, 1) == 0)
    b->timestamp = ticks;
}
