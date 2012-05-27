//
//  NanoVM, a tiny java VM for the Atmel AVR family
//  Copyright (C) 2005 by Till Harbaum <Till@Harbaum.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// 

//
//  This file contains the heap. It can be requested to
//  create/delete objects on the heap and does some
//  simple garbage collection.
//
//  The heap is being used top-to-bottom allowing the
//  virtual machines stack to grow inside the heap from bottom to
//  top
//

#include "types.h"
#include "config.h"
#include "debug.h"
#include "error.h"

#include "utils.h"
#include "heap.h"
#include "stack.h"
#include "vm.h"

u08_t heap[HEAPSIZE];
u16_t heap_base = 0;

#define HEAP_ID_FREE 0

typedef struct {
  heap_id_t id;
  unsigned int fieldref:1;
  unsigned int len:15;
} __attribute__((packed)) heap_t;

// return the real heap base (where memory can be "stolen"
// from
u08_t *heap_get_base(void) {
  return heap;
}

// a version of memcpy that can only copy overlapping chunks
// if the target address is higher
void heap_memcpy_up(u08_t *dst, u08_t *src, u16_t len) {
  dst += len;  src += len;
  while(len--) *--dst = *--src;
}

#ifdef DEBUG_JVM
// make some sanity checks on the heap in order to detect 
// heap curruption as early as possible
void heap_check(void) {
  u16_t current = heap_base;
  heap_t *h = (heap_t*)&heap[current];

  if(h->id != HEAP_ID_FREE) {
    DEBUGF("heap_check(): start element not free element\n");
    error(ERROR_HEAP_CORRUPTED);
  }  
  
  current += h->len + sizeof(heap_t);

  while(current < sizeof(heap)) {
    h = (heap_t*)&heap[current];
    if(h->id != HEAP_ID_FREE) {
      if(h->len > sizeof(heap)) {
	DEBUGF("heap_check(): single chunk too big\n");
	heap_show();
	error(ERROR_HEAP_ILLEGAL_CHUNK_SIZE);
      }
    } else {
      DEBUGF("heap_check(): unexpected free chunk\n");
      heap_show();
      error(ERROR_HEAP_CORRUPTED);
    }
    
    if(h->len+sizeof(heap_t) > sizeof(heap) - current) {
      DEBUGF("heap_check(): total size error\n");
      heap_show();
      error(ERROR_HEAP_CORRUPTED);
    }

    current += h->len + sizeof(heap_t);
  }

  if(current != sizeof(heap)) {
    DEBUGF("heap_check(): heap sum mismatch\n");
    heap_show();
    error(ERROR_HEAP_CORRUPTED);
  }
}
#endif

void heap_show(void) {
  u16_t current = heap_base;

  DEBUGF("Heap:\n");
  while(current < sizeof(heap)) {
    heap_t *h = (heap_t*)&heap[current];
    if(h->id == HEAP_ID_FREE) {
      DEBUGF("- %d free bytes\n", h->len);
    } else {
      DEBUGF("- chunk id %x with %d bytes:\n", h->id, h->len);

      if(h->len > sizeof(heap))
	error(ERROR_HEAP_ILLEGAL_CHUNK_SIZE);

      DEBUG_HEXDUMP(h+1, h->len);
    }

    if(h->len+sizeof(heap_t) > sizeof(heap) - current) {
      DEBUGF("heap_show(): total size error\n");
      error(ERROR_HEAP_CORRUPTED);
    }

    current += h->len + sizeof(heap_t);
  }

  DEBUGF("- %d bytes stolen\n", heap_base);
}

// search for chunk with id in heap and return chunk header
// address
heap_t *heap_search(heap_id_t id) {
  u16_t current = heap_base;

  while(current < sizeof(heap)) {
    heap_t *h = (heap_t*)&heap[current];
    if(h->id == id) return h;
    current += h->len + sizeof(heap_t);
  }
  return NULL;
}

heap_id_t heap_new_id(void) {
  heap_id_t id;

  for(id=1;id;id++) 
    if(heap_search(id) == NULL) 
      return id;

  return 0;
}

bool_t heap_alloc_internal(heap_id_t id, bool_t fieldref, u16_t size) {
  u16_t req = size + sizeof(heap_t);  // total mem required

  // search for free block
  heap_t *h = (heap_t*)&heap[heap_base];

  if(h->len >= req) {
    // reduce the size of the free chunk
    h->len -= req;

    // and create the new chunk behind this one
    h = (heap_t*)&heap[heap_base + sizeof(heap_t) + h->len];
    h->id = id;
    h->fieldref = fieldref;
    h->len = size;
#ifdef NVM_INITIALIZE_ALLOCATED
    // fill memory with zero
    u08_t * ptr = (void*)(h+1);
    while(size--)
      *ptr++=0;
#endif
    return TRUE;
  }

  DEBUGF("heap_alloc_internal(%d): out of memory\n", size);
  return FALSE;
}

heap_id_t heap_alloc(bool_t fieldref, u16_t size) {
  heap_id_t id = heap_new_id();

  DEBUGF("heap_alloc(size=%d)", size);
  DEBUGF(" -> id=0x%04x\n", id);
  if(!heap_alloc_internal(id, fieldref, size)) {
    heap_garbage_collect();
    // we need to reallocate heap id, gc. threw away the old one..
    if(!heap_alloc_internal(id, fieldref, size))
      error(ERROR_HEAP_OUT_OF_MEMORY);
    DEBUGF("heap_alloc(size=%d)", size);
    DEBUGF(" -> id=0x%04x successfull after gc\n", id);
  }

  return id;
}

void heap_realloc(heap_id_t id, u16_t size) {
  DEBUGF("heap_realloc(id=0x%04x, size=%d)\n", id, size);

  // check free mem and call garbage collection if required
  heap_t *h = (heap_t*)&heap[heap_base];
  if(h->len >= size + sizeof(heap_t))
    heap_garbage_collect();

  // get info on old chunk
  h = heap_search(id);

  // allocate space for bigger one
  if(!heap_alloc_internal(id, h->fieldref, size))
    error(ERROR_HEAP_OUT_OF_MEMORY);

  heap_t *h_new = heap_search(id);

  utils_memcpy(h_new+1, h+1, h->len);

  h->id = 0xff;  // unused id to make garbage collection delete
                 // this chunk next time
}

u16_t heap_get_len(heap_id_t id) {
  heap_t *h = heap_search(id);
  if(!h) error(ERROR_HEAP_CHUNK_DOES_NOT_EXIST);
  return h->len;
}

void *heap_get_addr(heap_id_t id) {
  heap_t *h = heap_search(id);
  if(!h) error(ERROR_HEAP_CHUNK_DOES_NOT_EXIST);
  return h+1;
}

void heap_init(void) {
  DEBUGF("heap_init()\n");

  // just one big free block
  heap_t *h = (heap_t*)&heap[0];
  h->id  = HEAP_ID_FREE;
  h->len = sizeof(heap) - sizeof(heap_t);
}

// in some cases, references to heap objects may be inside
// other heap objects. This currently happens only when
// a class is instanciated and this class contains fields.
// the heap element created by the constructor is marked with
// the fieldref bit and it is searched for references during
// garbage collections
bool_t heap_fieldref(heap_id_t id) {
  nvm_ref_t id16 = id | NVM_TYPE_HEAP;
  u16_t current = heap_base;

  // walk through the entire heap
  while(current < sizeof(heap)) {
    heap_t *h = (heap_t*)&heap[current];

    // check for entries with the fieldref flag
    if(h->fieldref) {
      u08_t j;

      // check all entries in the heap element for
      // the reference we are searching for
      for(j=0;j<h->len/sizeof(nvm_ref_t);j++) {
	if(((nvm_ref_t*)(h+1))[j] == id16)
	  return TRUE;
      }
    }

    current += h->len + sizeof(heap_t);
  }
  
  return FALSE;
}

// walk through the heap, check for every object
// if it's still being used and remove it if not
void heap_garbage_collect(void) {
  u16_t current = heap_base;
  heap_t *h;
  DEBUGF("heap_garbage_collect() free space before: %d\n", ((heap_t*)&heap[heap_base])->len);
  // set current to stack-top
  // walk through the entire heap
  while(current < sizeof(heap)) {
    h = (heap_t*)&heap[current];
    u16_t len = h->len + sizeof(heap_t);

    // found an entry
    if(h->id != HEAP_ID_FREE) {
      // check if it's still used
      if((!stack_heap_id_in_use(h->id))&&(!heap_fieldref(h->id))) {
	// it is not used, remove it
	DEBUGF("HEAP: removing unused object with id 0x%04x (len %d)\n",
	       h->id, len);
      
	// move everything before to the top
	heap_memcpy_up(heap+heap_base+len, heap+heap_base, current-heap_base);

	// add freed mem to free-chunk
	h = (heap_t*)&heap[heap_base];
	h->len += len;
      }
    }
    current += len;
  }

  if(current != sizeof(heap)) {
    DEBUGF("heap_garbage_collect(): total size error\n");
    error(ERROR_HEAP_CORRUPTED);
  }
  DEBUGF("heap_garbage_collect() free space after: %d\n", ((heap_t*)&heap[heap_base])->len);
}

// "steal" some bytes from the bottom of the heap (where
// the free-chunk is)
void heap_steal(u16_t bytes) {
  heap_t *h = (heap_t*)&heap[heap_base];
  u16_t len;

  DEBUGF("HEAP: request to steal %d bytes\n", bytes);

  if(h->id != HEAP_ID_FREE) {
    DEBUGF("heap_steal(%d): start element not free element\n", bytes);
    error(ERROR_HEAP_CORRUPTED);
  }

  // try to make space if necessary
  len = h->len;
  if(len < bytes) 
    heap_garbage_collect();

  len = h->len;
  if(len < bytes) 
    error(ERROR_HEAP_OUT_OF_STACK_MEMORY);

   // finally steal ...
  heap_base += bytes;
  h = (heap_t*)&heap[heap_base];
  h->id = HEAP_ID_FREE;
  h->len = len - bytes;
}

// someone wants us to give some bytes back :-)
void heap_unsteal(u16_t bytes) {
  heap_t *h = (heap_t*)&heap[heap_base];
  u16_t len;

  if(h->id != HEAP_ID_FREE) {
    DEBUGF("heap_unsteal(%d): start element not free element\n", bytes);
    error(ERROR_HEAP_CORRUPTED);
  }

  DEBUGF("HEAP: request to unsteal %d bytes\n", bytes);

  if(heap_base < bytes) {
    DEBUGF("stack underrun by %d bytes\n", bytes-heap_base);
    error(ERROR_HEAP_STACK_UNDERRUN);
  }

  // finally unsteal ...
  len = h->len;
  heap_base -= bytes;
  h = (heap_t*)&heap[heap_base];
  h->id = HEAP_ID_FREE;
  h->len = len + bytes;
}

