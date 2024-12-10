#ifndef AMR_MEMORY_H
#define AMR_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifndef AMR_TYPES_H
#define AMR_TYPES_H

typedef int8_t   s8;
typedef int16_t	 s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8 b8;

#endif // AMR_TYPES_H


#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

// @todo: build a logging mechanism for handling errors
// maybe read about that

enum MemStatus { MEM_OK=0, MEM_OUT_OF_BOUNDS, MEM_FULL };

struct ResVoid {
  enum MemStatus status;
  size_t bytes_count;
  void* memory;
};

b8 is_power_of_two(uintptr_t x);
uintptr_t fast_modulo(uintptr_t p, uintptr_t a);
uintptr_t align_forward(uintptr_t ptr, size_t align);

//===========================================================================================
// ---------------------------------- ARENA -------------------------------------------------
//===========================================================================================

struct Arena {
  unsigned char* buffer;
  size_t prev_offset;
  size_t curr_offset;
  size_t capacity;
};

void  arena_init(struct Arena *a, unsigned char *backing_store, size_t capacity);
void* arena_alloc_aligned(struct Arena* a, size_t size, size_t align);
void* arena_alloc(struct Arena* a, size_t size);
void* arena_resize_aligned(struct Arena* a, void* old_memory, size_t old_size,
                                    size_t new_size, size_t align); 
void* arena_resize(struct Arena* a, void* old_mem, size_t old_size,
                            size_t new_size);
void arena_clear(struct Arena *a);

//===========================================================================================
// ---------------------------------- STACK -------------------------------------------------
//===========================================================================================

/* 
* @todo: stack needs to be updated, it's really just a work in progress right now.
* The main thing is minimizing the use of compound types, since that is pretty annoying to deal with.
* I would rather write code that makes sure to collapse all possible cases and lets me just not worry about code.
* Would rather stick to worrying about data being data
*/

struct stack {
  unsigned char* buffer;
  size_t prev_offset;
  size_t curr_offset;
  size_t capacity;
};

struct stack_hdr {
  size_t prev_offset;
  size_t padding;
};

void           stack_init(struct stack* s, void *backing_store, size_t capacity); 
struct ResVoid stack_alloc_aligned(struct stack* s, size_t size, size_t alignment);
struct ResVoid stack_alloc(struct stack* s, size_t size); 
enum MemStatus stack_free(struct stack* s);
struct ResVoid stack_resize_aligned(struct stack* s, void* old_memory, size_t old_size,
                                    size_t new_size, size_t alignment);
struct ResVoid stack_resize(struct stack* s, void* old_memory, size_t old_size, size_t new_size);
void           stack_clear(struct stack* s);

#endif
