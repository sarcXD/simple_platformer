#pragma once

struct r32_array {
  r32 *buffer;
  size_t size;
  size_t capacity;
};

struct u32_array {
    u32 *buffer;
    size_t size;
    size_t capacity;
};

// @r32_array
void array_init(Arena* a, r32_array* arr, size_t capacity);
void array_insert(r32_array* arr, r32* ele, size_t ele_size);
void array_clear(r32_array* arr);

// @u32_array
void array_init(Arena* a, u32_array* arr, size_t capacity);
void array_insert(u32_array* arr, u32* ele, size_t ele_size);
void array_clear(u32_array* arr);
