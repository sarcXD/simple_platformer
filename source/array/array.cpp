#include "array.h"

// :r32_array_functions
void array_init(Arena* a, r32_array* arr, size_t capacity) {
  arr->buffer = (r32*)arena_alloc(a, capacity*sizeof(r32));

  assert(arr->buffer != NULL);

  arr->size = 0;
  arr->capacity = capacity;
}

void array_insert(r32_array* arr, r32* ele, size_t ele_size) {
  b8 assert_cond = arr->size + ele_size <= arr->capacity;
  assert(assert_cond);

  void* ptr = &arr->buffer[arr->size];
  memcpy(ptr, ele, sizeof(r32)*ele_size);
  arr->size += ele_size;
}

void array_clear(r32_array* arr) {
  memset(arr->buffer, 0, sizeof(r32)*arr->capacity);
  arr->size = 0;
}

// :u32_array_functions
void array_init(Arena* a, u32_array* arr, size_t capacity) {
  arr->buffer = (u32*)arena_alloc(a, capacity*sizeof(r32));

  assert(arr->buffer != NULL);

  arr->size = 0;
  arr->capacity = capacity;
}

void array_insert(u32_array* arr, u32* ele, size_t ele_size) {
  b8 assert_cond = arr->size + ele_size <= arr->capacity;
  assert(assert_cond);

  void* ptr = &arr->buffer[arr->size];
  memcpy(ptr, ele, sizeof(u32)*ele_size);
  arr->size += ele_size;
}

void array_clear(u32_array* arr) {
  memset(arr->buffer, 0, sizeof(u32)*arr->capacity);
  arr->size = 0;
}

