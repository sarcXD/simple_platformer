#pragma once

#include <assert.h>
#include <string.h>
#include "../core.h"

#ifndef ALIGNMENT
#define ALIGNMENT (2*sizeof(void*))
#endif

struct Arena {
  unsigned char* buffer;
  size_t prev_offset;
  size_t curr_offset;
  size_t capacity;
};

// definition
b8 is_power_of_two(uintptr_t x);
uintptr_t fast_modulo(uintptr_t p, uintptr_t a);
uintptr_t align_forward(uintptr_t ptr, uintptr_t alignment);
void arena_init(Arena* a, unsigned char *memory, size_t capacity);
void* arena_alloc(Arena* a, size_t size);
void arena_clear(Arena* a);


// implementation
b8 is_power_of_two(uintptr_t x) {
  return (x & (x-1)) == 0;
}

uintptr_t fast_modulo(uintptr_t p, uintptr_t a) { 
  return (p & (a - 1)); 
}

uintptr_t align_forward(uintptr_t ptr, size_t alignment) {
  uintptr_t p, a, modulo;

  assert(is_power_of_two(alignment));

  p = ptr;
  a = (uintptr_t)alignment;
  modulo = fast_modulo(p, a);

  if (modulo != 0) {
    p += (a - modulo);
  }

  return p;
}

void arena_init(Arena* a, unsigned char *memory, size_t capacity) {
  a->buffer = memory;
  a->prev_offset = 0;
  a->curr_offset = 0;
  a->capacity = capacity;
}

void* arena_alloc(Arena* a, size_t size) {
  void *ptr = NULL;

  assert(is_power_of_two(ALIGNMENT));

  uintptr_t curr_ptr = (uintptr_t)a->buffer + a->curr_offset;
  uintptr_t offset = align_forward(curr_ptr, ALIGNMENT);
  offset = offset - (uintptr_t)a->buffer;
  size_t next_offset = offset + size;

  assert(next_offset <= a->capacity);

  ptr = &a->buffer[offset];
  a->prev_offset = a->curr_offset;
  a->curr_offset = next_offset;
  memset(ptr, 0, size);

  return ptr;
}

void arena_clear(Arena* a) {
  a->curr_offset = 0;
  a->prev_offset = 0;
}
