#include "memory.h"

b8 is_power_of_two(uintptr_t x) { return (x & (x - 1)) == 0; }

uintptr_t fast_modulo(uintptr_t p, uintptr_t a) { return (p & (a - 1)); }

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

//===========================================================================================
// ---------------------------------- Arena
// -------------------------------------------------
//===========================================================================================

/*
  A cases where arena allocation WILL fail:
  | size = size_t + ${some_number_that_comes_up_higher_than_offset}

  This is because there is no check being made
*/
void arena_init(struct Arena *a, unsigned char *backing_store,
                size_t capacity) {
  a->buffer = backing_store;
  a->curr_offset = 0;
  a->prev_offset = 0;
  a->capacity = capacity;
}

void *arena_alloc_aligned(struct Arena *a, size_t size, size_t alignment) {
  void *ptr = NULL;

  assert(is_power_of_two(alignment));

  uintptr_t curr_ptr = (uintptr_t)a->buffer + a->curr_offset;
  uintptr_t offset = align_forward(curr_ptr, alignment);
  offset = offset - (uintptr_t)a->buffer;

  if (size <= a->capacity - offset) {
    ptr = &a->buffer[offset];
    a->prev_offset = a->curr_offset;
    a->curr_offset = offset + size;
    memset(ptr, 0, size);
  }

  return ptr;
}

void *arena_alloc(struct Arena *a, size_t size) {
  return arena_alloc_aligned(a, size, DEFAULT_ALIGNMENT);
}

void *arena_resize_aligned(struct Arena *a, void *old_memory, size_t old_size,
                           size_t new_size, size_t alignment) {
  unsigned char *old = (unsigned char *)old_memory;
  void *ptr = NULL;

  assert(is_power_of_two(alignment));

  if (old >= a->buffer && old < a->buffer + a->capacity) {
    if (a->buffer + a->prev_offset == old) {
      // extend_last_element
      if (new_size > old_size) {
        size_t size_increase = new_size - old_size;
        if (size_increase > (a->capacity - a->curr_offset)) {
          new_size = old_size;
          size_increase = 0;
        }
        memset(&a->buffer[a->curr_offset], 0, size_increase);
      }
      a->curr_offset = a->prev_offset + new_size;
      ptr = old_memory;
    } else {
      ptr = arena_alloc_aligned(a, new_size, alignment);
      if (ptr != NULL) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        memmove(ptr, old_memory, copy_size);
      }
    }
  }

  return ptr;
}

void *arena_resize(struct Arena *a, void *old_mem, size_t old_size,
                   size_t new_size) {
  return arena_resize_aligned(a, old_mem, old_size, new_size,
                              DEFAULT_ALIGNMENT);
}

void arena_clear(struct Arena *a) {
  a->curr_offset = 0;
  a->prev_offset = 0;
}

//===========================================================================================
// ---------------------------------- STACK
// -------------------------------------------------
//===========================================================================================

void stack_init(struct stack *s, void *backing_store, size_t capacity) {
  s->buffer = (unsigned char *)backing_store;
  s->prev_offset = 0;
  s->curr_offset = 0;
  s->capacity = capacity;
}

size_t calc_padding_with_header(uintptr_t ptr, uintptr_t alignment,
                                size_t hdr_sz) {
  uintptr_t p, a, modulo, padding, space_needed;

  assert(is_power_of_two(alignment));

  padding = space_needed = 0;

  p = ptr;
  a = alignment;
  modulo = fast_modulo(p, a);

  if (modulo != 0) {
    padding = a - modulo;
  }

  space_needed = (uintptr_t)hdr_sz;
  if (padding < space_needed) {
    space_needed -= padding;
    if (fast_modulo(space_needed, a) != 0) {
      padding = padding + space_needed + a;
    } else {
      padding = padding + space_needed;
    }
  }

  return (size_t)padding;
}

struct ResVoid stack_alloc_aligned(struct stack *s, size_t size,
                                   size_t alignment) {
  uintptr_t curr_addr, next_addr;
  size_t padding;
  struct stack_hdr *header;

  assert(is_power_of_two(alignment));
  if (alignment > 128) {
    alignment = 128;
  }

  struct ResVoid result = {.status = MEM_OK, .bytes_count = 0, .memory = 0};

  curr_addr = (uintptr_t)s->buffer + (uintptr_t)s->curr_offset;
  padding = calc_padding_with_header(curr_addr, (uintptr_t)alignment,
                                     sizeof(struct stack_hdr));

  if (size > s->capacity - (s->curr_offset + padding)) {
    result.status = MEM_FULL;
    return result;
  }

  next_addr = curr_addr + (uintptr_t)padding;
  header = (struct stack_hdr *)(next_addr - sizeof(struct stack_hdr));
  header->prev_offset = s->prev_offset;
  header->padding = padding;

  s->prev_offset = s->curr_offset + padding;
  s->curr_offset = s->prev_offset + size;

  result.memory = memset((void *)next_addr, 0, size);
  result.bytes_count = size;

  return result;
}

struct ResVoid stack_alloc(struct stack *s, size_t size) {
  return stack_alloc_aligned(s, size, DEFAULT_ALIGNMENT);
}

enum MemStatus stack_free(struct stack *s) {
  uintptr_t last_ele = (uintptr_t)s->buffer + (uintptr_t)s->prev_offset;
  struct stack_hdr *header =
      (struct stack_hdr *)(last_ele - sizeof(struct stack_hdr));

  uintptr_t prev_ele = (uintptr_t)s->buffer + (uintptr_t)header->prev_offset;
  s->curr_offset =
      (size_t)((last_ele - (uintptr_t)header->padding) - (uintptr_t)s->buffer);
  s->prev_offset = (size_t)(prev_ele - (uintptr_t)s->buffer);

  return MEM_OK;
}

struct ResVoid stack_resize_aligned(struct stack *s, void *old_memory,
                                    size_t old_size, size_t new_size,
                                    size_t alignment) {
  struct ResVoid result = {.status = MEM_OK, .bytes_count = 0, .memory = 0};

  if (old_memory < s->buffer || old_memory > s->buffer + s->capacity) {
    result.status = MEM_OUT_OF_BOUNDS;
    return result;
  }

  // is_last_element()
  if (s->buffer + s->prev_offset == old_memory) {
    if (new_size > old_size) {
      size_t size_difference = new_size - old_size;
      if (size_difference > s->capacity - s->curr_offset) {
        result.status = MEM_FULL;
        return result;
      }

      memset(&s->buffer[s->curr_offset], 0, size_difference);
    }
    s->curr_offset = s->prev_offset + new_size;

    result.memory = old_memory;
    return result;
  }

  result = stack_alloc_aligned(s, new_size, alignment);
  size_t min_size =
      old_size < result.bytes_count ? old_size : result.bytes_count;
  memmove(result.memory, old_memory, min_size);

  return result;
}

struct ResVoid stack_resize(struct stack *s, void *old_memory, size_t old_size,
                            size_t new_size) {
  return stack_resize_aligned(s, old_memory, old_size, new_size,
                              DEFAULT_ALIGNMENT);
}

void stack_clear(struct stack *s) {
  s->prev_offset = 0;
  s->curr_offset = 0;
}
