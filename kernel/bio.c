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

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  // Serializes cache misses and eviction. Lookups of cached blocks use only
  // their hash bucket's lock.
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKET];
} bcache;

static uint
bucketno(uint blockno)
{
  return blockno % NBUCKET;
}

static void
binsert(struct bucket *bucket, struct buf *b)
{
  b->next = bucket->head.next;
  b->prev = &bucket->head;
  bucket->head.next->prev = b;
  bucket->head.next = b;
}

static void
bremove(struct buf *b)
{
  b->next->prev = b->prev;
  b->prev->next = b->next;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Initially all free buffers can live in any bucket.
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bucket *bucket = &bcache.buckets[bucketno(blockno)];

  // Is the block already cached?
  acquire(&bucket->lock);
  for(b = bucket->head.next; b != &bucket->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  // Serialize misses so a block cannot be inserted twice and so an evicted
  // buffer can be moved between buckets safely.
  acquire(&bcache.lock);
  acquire(&bucket->lock);
  for(b = bucket->head.next; b != &bucket->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  // Not cached. Recycle an unused buffer from any bucket.
  for(int i = 0; i < NBUCKET; i++){
    struct bucket *oldbucket = &bcache.buckets[i];
    acquire(&oldbucket->lock);
    for(b = oldbucket->head.next; b != &oldbucket->head; b = b->next){
      if(b->refcnt == 0) {
        bremove(b);
        release(&oldbucket->lock);

        acquire(&bucket->lock);
        binsert(bucket, b);
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
        release(&bucket->lock);
        release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
      }
    }
    release(&oldbucket->lock);
  }
  release(&bcache.lock);
  panic("bget: no buffers");
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

  acquire(&bcache.buckets[bucketno(b->blockno)].lock);
  b->refcnt--;
  release(&bcache.buckets[bucketno(b->blockno)].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.buckets[bucketno(b->blockno)].lock);
  b->refcnt++;
  release(&bcache.buckets[bucketno(b->blockno)].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.buckets[bucketno(b->blockno)].lock);
  b->refcnt--;
  release(&bcache.buckets[bucketno(b->blockno)].lock);
}


