#pragma once

#include "../core.h"
#include "../memory/arena.h"

struct r32_array {
  size_t size;
  size_t capacity;
  r32 *buffer;
};

struct u32_array {
    size_t size;
    size_t capacity;
    u32 *buffer;
};

// @r32_array
void array_init(Arena* a, r32_array* arr, size_t capacity);
void array_insert(r32_array* arr, r32* ele, size_t ele_size);
void array_clear(r32_array* arr);

// @u32_array
void array_init(Arena* a, u32_array* arr, size_t capacity);
void array_insert(u32_array* arr, u32* ele, size_t ele_size);
void array_clear(u32_array* arr);
