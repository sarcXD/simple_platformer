#include <SDL2/SDL.h>
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <ft2build.h>
#include FT_FREETYPE_H

/*
* Project Estimation/Target ~ 1 - 2 months (was so off on this) 
* well, to be fair, if I account for actual time worked, then I was not 
* so off on my estimate, I think I am still in the estimation range
* Tasks:
* DONE:
* - gravity - very barebones version done
* - horizontal motion on ground
*   - accelerate
*   - constant speed
*   - decelerate to give impression of sliding
* - horizontal motion when falling
*   - inertia-like movement when accelerating prior to falling
*   - loss of inertia when player was at high speed before 
*     sliding off and other small movement when in free fall
*   - movement and deceleration when not moving in free fall
* - Jumping
* - Fixed framerate to prevent weird quirks with movement
* - Basic camera follower
* - move from row-major to column major setup for math library
* TODO:
* - Efficient Quad Renderer
* - Some way to make and define levels
* - Level Creation
* - Update camera follower for centering player in view (with limits) after
*   a few seconds (maybe like 2 seconds)
* - Level completion Object
* - Implement Broad Phase Collision for efficient collision handling 
* - Audio
*/

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef float    r32;
typedef double   r64;

typedef u8       b8;

#include "memory/arena.h"
#include "math.h"

enum PMoveState {
  NO_MOVE       = 0,
  MOVE          = 1,
  FALL_MOVE     = 2,
};

enum PlatformKey {
  PK_NIL = 0,
  PK_W = 1,
  PK_A = 2,
  PK_S = 3,
  PK_D = 4,
};

struct TextChar {
  s64 advance;
  Vec2 size;
  Vec2 bearing;
};

struct TextState {
  u32 pixel_size;
  u32 texture_atlas_id;
  u32 sp;
  u32 vao;
  u32 vbo;
  u32 chunk_size;
  s32* char_indexes;
  Mat4* transforms;
  TextChar* char_map;
};

#define BATCH_SIZE 2000

struct r32_array {
  r32* buffer;
  u32 size;
  u32 capacity;
};

void array_init(Arena* a, r32_array* arr, u32 capacity) {
  arr->buffer = (r32*)arena_alloc(a, capacity*sizeof(r32));

  assert(arr->buffer != NULL);

  arr->size = 0;
  arr->capacity = capacity;
}

void array_insert(r32_array* arr, r32* ele, u32 ele_size) {
  b8 assert_cond = arr->size + ele_size <= arr->capacity;
  if (!assert_cond) {
    SDL_Log("arr->size: %d, arr->capacity: %d", arr->size, arr->capacity);
  }
  assert(assert_cond);

  void* ptr = &arr->buffer[arr->size];
  memcpy(ptr, ele, sizeof(r32)*ele_size);
  arr->size += ele_size;
}

void array_clear(r32_array* arr) {
  memset(arr->buffer, 0, sizeof(r32)*arr->capacity);
  arr->size = 0;
}

struct GLRenderer {
  // colored quad
  b8  cq_init;
  u32 cq_sp;
  u32 cq_vao;
  // camera
  b8   cam_update;
  Vec3 preset_up_dir;
  Vec3 cam_pos;
  Vec3 cam_look;
  Mat4 cam_view;
  Mat4 cam_proj;
  // Batched cq
  // batching buffer
  u32 cq_batch_sp;
  u32 cq_batch_vao;
  u32 cq_batch_vbo;
  u32 cq_batch_count;
  r32_array cq_pos_batch;
  r32_array cq_mvp_batch;
  r32_array cq_color_batch;

  // ui text 
  TextState ui_text;
}; 

struct Controller {
  b8 move_up;
  b8 move_down;
  b8 move_left;
  b8 move_right;
  b8 jump;
  b8 toggle_gravity;
};

struct Rect {
  Vec2 tl;
  Vec2 br;
  Vec2 size;
  Vec3 position;
};

struct FrameTimer {
  u64 tCurr;
  u64 tPrev;
  r64 tDeltaMS;
  r64 tDelta;
  u64 tFreq;
};

FrameTimer frametimer() {
  FrameTimer res = {};
  res.tFreq = SDL_GetPerformanceFrequency();
  res.tCurr = SDL_GetPerformanceCounter();
  res.tPrev = res.tCurr;
  res.tDelta = (r64)(res.tCurr - res.tPrev) / (r64)res.tFreq;
  res.tDeltaMS = res.tDelta * 1000.0f;

  return res;
}

void update_frame_timer(FrameTimer *ft) {
  ft->tPrev = ft->tCurr;
  ft->tCurr = SDL_GetPerformanceCounter();
  ft->tDelta = (r64)(ft->tCurr - ft->tPrev) / (r64)ft->tFreq;
  ft->tDeltaMS = ft->tDelta * 1000.0f;
}

void enforce_frame_rate(FrameTimer *ft, u32 target) {
  r64 target_frametime = SDL_floor(
    1000.0f/(r64)target
  );
  while(ft->tDeltaMS < target_frametime) {
    ft->tCurr = SDL_GetPerformanceCounter();
    ft->tDelta = (r64)(ft->tCurr - ft->tPrev) / ft->tFreq;
    ft->tDeltaMS = ft->tDelta * 1000.0f;

    // pass time
    continue;
  }
}

struct GameState {
  // the default size the game is designed around
  Vec2 world_size;
  Vec2 screen_size;
  // the scaling factor to increase/decrease size of game assets
  Vec2 render_scale;
  // player
  Rect player;
  Rect floor;
  Rect wall;
};

u32 gl_shader_program(char* vs, char* fs)
{
  int status;
  char info_log[512];
  
  
  // =============
  // vertex shader
  u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vs, NULL);
  glCompileShader(vertex_shader);
  
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
  if (status == 0)
  {
    glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
    printf("== ERROR: Vertex Shader Compilation Failed ==\n");
    printf("%s\n", info_log);
  }
  
  
  // ===============
  // fragment shader
  u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fs, NULL);
  glCompileShader(fragment_shader);
  
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
  if (status == 0)
  {
    glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
    printf("== ERROR: Fragment Shader Compilation Failed ==\n");
    printf("%s\n", info_log);
  }
  
  
  // ==============
  // shader program
  u32 shader_program = glCreateProgram();
  
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  
  glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
  if(status == 0)
  {
    glGetProgramInfoLog(shader_program, 512, NULL, info_log);
    printf("== ERROR: Shader Program Linking Failed\n");
    printf("%s\n", info_log);
  }
  
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  return shader_program;
}

u32 gl_shader_program_from_path(const char* vspath, const char* fspath)
{
  size_t read_count;
  char* vs = (char*)SDL_LoadFile(vspath, &read_count);
  if (read_count == 0)
  {
    printf("Error! Failed to read vertex shader file at path %s\n", vspath);
    return 0;
  }
  
  char* fs = (char*)SDL_LoadFile(fspath, &read_count);
  if (read_count == 0)
  {
    printf("Error! Failed to read fragment shader file at path %s\n", vspath);
    return 0;
  }
  
  u32 shader_program = gl_shader_program(vs, fs);
  return shader_program;
}

u32 gl_setup_colored_quad(u32 sp)
{
  // @todo: make this use index buffer maybe?
  r32 vertices[] = {
    -1.0f, -1.0f,  0.0f,  // bottom-left
    1.0f, -1.0f,  0.0f,  // bottom-right
    1.0f,  1.0f,  0.0f,  // top-right
    1.0f,  1.0f,  0.0f,  // top-right
    -1.0f,  1.0f,  0.0f,  // top-left
    -1.0f, -1.0f,  0.0f,  // bottom-left
  };
  u32 vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(r32), (void*)0);
  
  glBindVertexArray(0);
  
  // now return or store the vao, vbo state somewhere
  return vao;
}

void gl_setup_colored_quad_optimized(
  GLRenderer* renderer,
  u32 sp
) {
  // @todo: make this use index buffer maybe?
  glGenVertexArrays(1, &renderer->cq_batch_vao);
  glGenBuffers(1, &renderer->cq_batch_vbo);
  
  glBindVertexArray(renderer->cq_batch_vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->cq_batch_vbo);
  glBufferData(
    GL_ARRAY_BUFFER, (
      renderer->cq_pos_batch.capacity + renderer->cq_color_batch.capacity
    ) * sizeof(r32), NULL, GL_DYNAMIC_DRAW
  );

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(r32), (void*)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
    1, 3, GL_FLOAT, GL_FALSE, 
    3 * sizeof(r32), (void*)(renderer->cq_pos_batch.capacity*sizeof(r32))
  );
  
  glBindVertexArray(0);
}

void gl_cq_flush(GLRenderer* renderer) {
  glUseProgram(renderer->cq_batch_sp);
  glEnable(GL_DEPTH_TEST);

  glUniformMatrix4fv(
    glGetUniformLocation(renderer->cq_batch_sp, "View"),
    1, GL_FALSE, (renderer->cam_view).buffer
  );

  glUniformMatrix4fv(
    glGetUniformLocation(renderer->cq_batch_sp, "Projection"), 
    1, GL_FALSE, (renderer->cam_proj).buffer
  );

  glBindBuffer(GL_ARRAY_BUFFER, renderer->cq_batch_vbo);

  // fill batch data
  // position batch
  glBufferSubData(
    GL_ARRAY_BUFFER, 
    0, 
    renderer->cq_pos_batch.capacity*sizeof(r32), 
    renderer->cq_pos_batch.buffer
  );

  // color batch
  glBufferSubData(
    GL_ARRAY_BUFFER, 
    renderer->cq_pos_batch.capacity*sizeof(r32), 
    renderer->cq_color_batch.capacity*sizeof(r32), 
    (void*)renderer->cq_color_batch.buffer
  );

  glBindVertexArray(renderer->cq_batch_vao);
  glDrawArrays(GL_TRIANGLES, 0, renderer->cq_batch_count*6);

  array_clear(&renderer->cq_pos_batch);
  array_clear(&renderer->cq_color_batch);
  renderer->cq_batch_count = 0;
}

void gl_draw_colored_quad_optimized(
  GLRenderer* renderer,
  Vec3 position,
  Vec2 size,
  Vec3 color
) {
  Vec4 vertices[6] = {
    Vec4{-1.0f, -1.0f,  0.0f, 1.0f},// bottom-left
    Vec4{ 1.0f, -1.0f,  0.0f, 1.0f},// bottom-right
    Vec4{ 1.0f,  1.0f,  0.0f, 1.0f},// top-right
    Vec4{ 1.0f,  1.0f,  0.0f, 1.0f},// top-right
    Vec4{-1.0f,  1.0f,  0.0f, 1.0f},// top-left
    Vec4{-1.0f, -1.0f,  0.0f, 1.0f} // bottom-left
  };

  // setting quad size
  Mat4 model = diag4m(1.0);
  Mat4 scale = scaling_matrix4m(size.x, size.y, 0.0f);
  model = multiply4m(scale, model);
  // setting quad position
  Mat4 translation = translation_matrix4m(position.x, position.y, position.z);
  model = multiply4m(translation, model);

  Vec4 model_pos;
  model_pos = multiply4mv(model, vertices[0]);
  vertices[0] = model_pos;
  model_pos = multiply4mv(model, vertices[1]);
  vertices[1] = model_pos;
  model_pos = multiply4mv(model, vertices[2]);
  vertices[2] = model_pos;
  model_pos = multiply4mv(model, vertices[3]);
  vertices[3] = model_pos;
  model_pos = multiply4mv(model, vertices[4]);
  vertices[4] = model_pos;
  model_pos = multiply4mv(model, vertices[5]);
  vertices[5] = model_pos;
  
  array_insert(&renderer->cq_pos_batch, vertices[0].data, 4);
  array_insert(&renderer->cq_pos_batch, vertices[1].data, 4);
  array_insert(&renderer->cq_pos_batch, vertices[2].data, 4);
  array_insert(&renderer->cq_pos_batch, vertices[3].data, 4);
  array_insert(&renderer->cq_pos_batch, vertices[4].data, 4);
  array_insert(&renderer->cq_pos_batch, vertices[5].data, 4);

  // initialise color to be per vertex to allow batching
  array_insert(&renderer->cq_color_batch, color.data, 3);
  array_insert(&renderer->cq_color_batch, color.data, 3);
  array_insert(&renderer->cq_color_batch, color.data, 3);
  array_insert(&renderer->cq_color_batch, color.data, 3);
  array_insert(&renderer->cq_color_batch, color.data, 3);
  array_insert(&renderer->cq_color_batch, color.data, 3);

  renderer->cq_batch_count++;

  if(renderer->cq_batch_count == BATCH_SIZE) {
    gl_cq_flush(renderer);
  }
}

void gl_draw_colored_quad(
  GLRenderer* renderer,
  Vec3 position,
  Vec2 size,
  Vec3 color
) {
  glEnable(GL_DEPTH_TEST);
  glUseProgram(renderer->cq_sp);
  if (renderer->cq_init == 0)
  {
    glUniformMatrix4fv(
      glGetUniformLocation(renderer->cq_sp, "Projection"), 
      1, GL_FALSE, (renderer->cam_proj).buffer
    );
    renderer->cq_init = 1;
  }
  // setting quad size
  Mat4 model = diag4m(1.0);
  Mat4 scale = scaling_matrix4m(size.x, size.y, 0.0f);
  model = multiply4m(scale, model);
  // setting quad position
  Mat4 translation = translation_matrix4m(position.x, position.y, position.z);
  model = multiply4m(translation, model);
  // setting color
  glUniform3fv(glGetUniformLocation(renderer->cq_sp, "Color"), 1, color.data);
  
  glUniformMatrix4fv(
    glGetUniformLocation(renderer->cq_sp, "Model"), 
    1, GL_FALSE, model.buffer
  );

  glUniformMatrix4fv(
    glGetUniformLocation(renderer->cq_sp, "View"), 
    1, GL_FALSE, (renderer->cam_view).buffer
  );
  
  glBindVertexArray(renderer->cq_vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void gl_setup_text(TextState* state, FT_Face font_face)
{
  FT_Set_Pixel_Sizes(font_face, state->pixel_size, state->pixel_size);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  
  glGenTextures(1, &(state->texture_atlas_id));
  glBindTexture(GL_TEXTURE_2D_ARRAY, state->texture_atlas_id);
  
  // generate texture
  glTexImage3D(
    GL_TEXTURE_2D_ARRAY,
    0,
    GL_R8,
    state->pixel_size,
    state->pixel_size,
    128,
    0,
    GL_RED,
    GL_UNSIGNED_BYTE,
    0
  );
  
  // generate characters
  for (u32 c = 0; c < 128; c++)
  {
    if (FT_Load_Char(font_face, c, FT_LOAD_RENDER))
    {
      printf("ERROR :: Freetype failed to load glyph: %c", c);
    } 
    else 
    {
      glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        0, 0, // x, y offset
        int(c),
        font_face->glyph->bitmap.width,
        font_face->glyph->bitmap.rows,
        1,
        GL_RED,
        GL_UNSIGNED_BYTE,
        font_face->glyph->bitmap.buffer
      );
      
      // set texture options
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      
      TextChar tc;
      tc.size = Vec2{
        (r32)font_face->glyph->bitmap.width, 
        (r32)font_face->glyph->bitmap.rows
      };
      tc.bearing = Vec2{
        (r32)font_face->glyph->bitmap_left, 
        (r32)font_face->glyph->bitmap_top
      };
      tc.advance = font_face->glyph->advance.x;
      
      state->char_map[c] = tc;
    }
  }
  
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  
  // @note: this data is used for GL_TRIANGLE_STRIP
  // as such the order for vertices for this is AntiCW -> CW -> AntiCW
  // that can be seen in this array as it goes from ACW -> CW
  r32 vertices[] = {
    0.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 1.0f,
    1.0f, 0.0f
  };
  
  glGenVertexArrays(1, &(state->vao));
  glGenBuffers(1, &(state->vbo));
  
  glBindVertexArray(state->vao);
  glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void gl_render_text(GLRenderer *renderer, char* text, Vec2 position, r32 size, Vec3 color)
{
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  glUseProgram(renderer->ui_text.sp);
  glUniformMatrix4fv(glGetUniformLocation(renderer->ui_text.sp, "View"), 
                     1, GL_FALSE, renderer->cam_view.buffer);
  glUniformMatrix4fv(glGetUniformLocation(renderer->ui_text.sp, "Projection"), 
                     1, GL_FALSE, renderer->cam_proj.buffer);
  glUniform3fv(glGetUniformLocation(renderer->ui_text.sp, "TextColor"), 1, color.data);
  glBindVertexArray(renderer->ui_text.vao);
  glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->ui_text.texture_atlas_id);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_text.vbo);
  glActiveTexture(GL_TEXTURE0);
  
  u32 running_index = 0;
  r32 startx = position.x;
  r32 starty = position.y;
  r32 linex = startx;
  r32 scale = size/renderer->ui_text.pixel_size;
  memset(renderer->ui_text.transforms, 0, renderer->ui_text.chunk_size);
  memset(renderer->ui_text.char_indexes, 0, renderer->ui_text.chunk_size);
  
  char *char_iter = text;
  while (*char_iter != '\0')
  {
    TextChar render_char = renderer->ui_text.char_map[*char_iter];
    if (*char_iter == '\n')
    {
      linex = startx;
      starty = starty - (render_char.size.y * 1.5 * scale);
    }
    else if (*char_iter == ' ')
    {
      linex += (render_char.advance >> 6) * scale;
    }
    else
    {
      r32 xpos = linex + (scale * render_char.bearing.x);
      r32 ypos = starty - (renderer->ui_text.pixel_size - render_char.bearing.y) * scale;
      
      r32 w = scale * renderer->ui_text.pixel_size;
      r32 h = scale * renderer->ui_text.pixel_size;
      
      Mat4 sm = scaling_matrix4m(w, h, 0);
      Mat4 tm = translation_matrix4m(xpos, ypos, 0);
      Mat4 model = multiply4m(tm, sm);
      renderer->ui_text.transforms[running_index] = model;
      renderer->ui_text.char_indexes[running_index] = int(*char_iter);
      
      linex += (render_char.advance >> 6) * scale;
      running_index++;
      if (running_index > renderer->ui_text.chunk_size - 1)
      {
        r32 transform_loc = glGetUniformLocation(renderer->ui_text.sp, "LetterTransforms");
        glUniformMatrix4fv(transform_loc, renderer->ui_text.chunk_size, 
                           GL_FALSE, &(renderer->ui_text.transforms[0].buffer[0]));
        r32 texture_map_loc = glGetUniformLocation(renderer->ui_text.sp, "TextureMap");
        glUniform1iv(texture_map_loc, renderer->ui_text.chunk_size, renderer->ui_text.char_indexes);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, renderer->ui_text.chunk_size);
        running_index = 0;
        memset(renderer->ui_text.transforms, 0, renderer->ui_text.chunk_size);
        memset(renderer->ui_text.char_indexes, 0, renderer->ui_text.chunk_size);
      }
    }
    char_iter++;
  }
  if (running_index > 0)
  {
    u32 render_count = running_index < renderer->ui_text.chunk_size ? running_index : renderer->ui_text.chunk_size;
    r32 transform_loc = glGetUniformLocation(renderer->ui_text.sp, "LetterTransforms");
    glUniformMatrix4fv(transform_loc, render_count, 
                       GL_FALSE, &(renderer->ui_text.transforms[0].buffer[0]));
    r32 texture_map_loc = glGetUniformLocation(renderer->ui_text.sp, "TextureMap");
    glUniform1iv(texture_map_loc, render_count, renderer->ui_text.char_indexes);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, render_count);
    running_index = 0;
    memset(renderer->ui_text.transforms, 0, render_count);
    memset(renderer->ui_text.char_indexes, 0, render_count);
  }
}

Rect rect(Vec3 position, Vec2 size) {
  Rect r = {0};
  r.position = position;
  r.size = size;

  r.tl.x = position.x - size.x;
  r.tl.y = position.y + size.y;

  Vec2 br;
  r.br.x = position.x + size.x;
  r.br.y = position.y - size.y;

  return r;
}

Vec2 get_move_dir(Controller c) {
  Vec2 dir = {};
  if (c.move_up) {
    dir.y = 1.0f;
  }
  if (c.move_down) {
    dir.y = -1.0f;
  }
  if (c.move_left) {
    dir.x = -1.0f;
  }
  if (c.move_right) {
    dir.x = 1.0f;
  }

  return dir;
}

void update_camera(GLRenderer *renderer) {
  if (renderer->cam_update == true) {
    renderer->cam_view = camera_create4m(
      renderer->cam_pos, 
      add3v(renderer->cam_pos, renderer->cam_look), 
      renderer->preset_up_dir
    );
    renderer->cam_update = false;
  }
}

Vec3 get_world_position_from_percent(GameState state, Vec3 v) {
  Vec3 world_pos = v;
  world_pos.x = state.render_scale.x*state.world_size.x*v.x/100.0f;
  world_pos.y = state.render_scale.y*state.world_size.y*v.y/100.0f;

  return world_pos;
}

Vec2 get_screen_position_from_percent(GameState state, Vec2 v) {
  Vec2 screen_pos = v;
  screen_pos.x = state.render_scale.x*state.screen_size.x*v.x/100.0f;
  screen_pos.y = state.render_scale.y*state.screen_size.y*v.y/100.0f;

  return screen_pos;
}

Vec3 get_screen_position_from_percent(GameState state, Vec3 v) {
  Vec3 screen_pos = v;
  screen_pos.x = state.render_scale.x*state.screen_size.x*v.x/100.0f;
  screen_pos.y = state.render_scale.y*state.screen_size.y*v.y/100.0f;

  return screen_pos;
}

int main(int argc, char* argv[])
{
  u32 scr_width = 1024;
  u32 scr_height = 768;
  
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    printf("Error initialising SDL2: %s\n", SDL_GetError());
    return -1;
  }
  
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  SDL_Window* window = SDL_CreateWindow("simple platformer",
                                        SDL_WINDOWPOS_UNDEFINED, 
                                        SDL_WINDOWPOS_UNDEFINED,
                                        scr_width, scr_height,
                                        SDL_WINDOW_OPENGL);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  if (!context)
  {
    printf("ERROR :: OpenGL context creation failed: %s\n", SDL_GetError());
    return -1;
  }
  
    // load glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
	    printf("ERROR :: Failed to initialize Glad\n");
	    return -1;
    }
  
  // vsync controls: 0 = OFF | 1 = ON (Default)
  SDL_GL_SetSwapInterval(0);
  
  GLRenderer *renderer = new GLRenderer();

  u32 pos_ele_count =  BATCH_SIZE * 4*6;
  u32 color_ele_count = BATCH_SIZE * 3*6;
  // 1GB <= (((1b*1024)kb*1024)mb*1024)mb
  u32 mem_size = (1024*1024*1024);
  void* batch_memory = calloc(mem_size, sizeof(r32));
  Arena batch_arena;
  arena_init(&batch_arena, (unsigned char*)batch_memory, mem_size*sizeof(r32));
  array_init(&batch_arena, &(renderer->cq_pos_batch), pos_ele_count);
  array_init(&batch_arena, &(renderer->cq_color_batch), color_ele_count);


  u32 quad_sp = gl_shader_program_from_path(
    "./source/shaders/colored_quad.vs.glsl", 
    "./source/shaders/colored_quad.fs.glsl"
  );
  u32 ui_text_sp = gl_shader_program_from_path(
    "./source/shaders/ui_text.vs.glsl",
    "./source/shaders/ui_text.fs.glsl"
  );
  u32 cq_batch_sp = gl_shader_program_from_path(
    "./source/shaders/cq_batched.vs.glsl",
    "./source/shaders/cq_batched.fs.glsl"
  );
  u32 quad_vao = gl_setup_colored_quad(quad_sp);
  renderer->cq_sp = quad_sp;
  renderer->cq_vao = quad_vao;

  renderer->cq_batch_sp = cq_batch_sp;
  gl_setup_colored_quad_optimized(renderer, cq_batch_sp);
  
  
  r32 render_scale = 2.0f;
  // ==========
  // setup text
  // 1. setup free type library stuff
  FT_Library ft_lib;
  FT_Face roboto_font_face;
  if (FT_Init_FreeType(&ft_lib))
  {
    printf("ERROR :: Could not init freetype library\n");
    return -1;
  }
  
  FT_Error error = FT_New_Face(
    ft_lib, 
    "assets/fonts/Roboto.ttf", 
    0, &roboto_font_face
  );
  if (error == FT_Err_Unknown_File_Format)
  {
    printf("ERROR :: Font Loading Failed. The font format is unsupported\n");
    return -1;
  }
  else if (error)
  {
    printf("ERROR :: Font Loading Failed. Unknown error code: %d\n", error);
    return -1;
  }
  // 2. setup gl text
  // @note: we only support 128 characters, which is the basic ascii set
  renderer->ui_text.chunk_size = 32;
  renderer->ui_text.pixel_size = 32*render_scale;
  renderer->ui_text.sp = ui_text_sp;
  renderer->ui_text.transforms = (Mat4*)malloc(
    renderer->ui_text.chunk_size*sizeof(Mat4)
  );
  renderer->ui_text.char_indexes = (s32*)malloc(
    renderer->ui_text.chunk_size*sizeof(s32)
  );
  renderer->ui_text.char_map = (TextChar*)malloc(
    128*sizeof(TextChar)
  );
  gl_setup_text(&(renderer->ui_text), roboto_font_face);
  
  
  // ============
  // setup camera
  Vec3 preset_up_dir = Vec3{0.0f, 1.0f, 0.0f};
  renderer->preset_up_dir = preset_up_dir;
  renderer->cam_update = false;
  renderer->cam_pos = Vec3{0.0f, 0.0f, 1.0f};
  renderer->cam_look = camera_look_around(TO_RAD(0.0f), -TO_RAD(90.0f));
  renderer->cam_view = camera_create4m(
    renderer->cam_pos, 
    add3v(renderer->cam_pos, renderer->cam_look), renderer->preset_up_dir
  );
  renderer->cam_proj = orthographic4m(
    0.0f, (r32)scr_width*render_scale, 
    0.0f, (r32)scr_height*render_scale, 
    0.1f, 10.0f
  );

  // @section: gameplay variables
  r32 motion_scale = 2.0f;
  r32 fall_accelx = 3.0f*motion_scale;
  r32 move_accelx = 6.0f*motion_scale;
  r32 freefall_accel = -11.8f*motion_scale;
  r32 jump_force = 6.5f*motion_scale;
  r32 effective_force = 0.0f;
  Vec2 player_velocity = Vec2{0.0f, 0.0f};
  Vec2 p_move_dir = Vec2{0.0f, 0.0f};
  // direction in which player is effectively travelling
  Vec2 p_motion_dir = Vec2{0.0f, 0.0f};

  GameState state = {0};
  state.world_size = Vec2{(r32)scr_width, (r32)scr_height};
  state.screen_size = Vec2{(r32)scr_width, (r32)scr_height};
  state.render_scale = vec2(render_scale);
  Vec3 player_position = Vec3{0.0f, 70.0f, -1.0f};
  Vec2 player_size = vec2(40.0f);
  state.player = rect(player_position, player_size);

  // @thinking: level object handling
  // there should be a most smallest supported unit
  // smallest_size: 16x16
  // object placement should be in pixels 
  // in order to scale to different resolutions it should be multiplied by
  // scaling factor
  Vec2 atom_size = {16.0f, 16.0f};

  Vec3 floor_position = Vec3{640.0f*render_scale, 400.0f*render_scale, -2.0f};
  Vec2 floor_size = atom_size*Vec2{40.0f, 1.5f};
  state.floor = rect(floor_position, floor_size);

  Vec3 wall_position = get_world_position_from_percent(
    state, Vec3{20.0f, 10.0f, -2.0f}
  );
  Vec2 wall_size = atom_size*Vec2{1.5f, 8.0f};
  state.wall = rect(wall_position, wall_size);

  // gameplay camera movement stuff
  Vec2 cam_lt_limit = {0};
  Vec2 cam_rb_limit = {0};
  cam_lt_limit = get_screen_position_from_percent(
    state, Vec2{20.0f, 80.0f}
  );
  cam_rb_limit = get_screen_position_from_percent(
    state, Vec2{80.0f, 20.0f}
  );

  Controller controller = {0};
  r32 key_down_time[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  b8 is_key_down_x = false;
  
  // gravity calculations
  b8 collidex = 0;
  b8 collidey = 0;
  b8 is_gravity = 0;
  
  b8 game_running = 1;

  FrameTimer timer = frametimer();

  while (game_running) 
  {
    update_frame_timer(&timer);
    enforce_frame_rate(&timer, 60);

    controller.jump = 0;
    controller.toggle_gravity = 0;

    SDL_Event ev;
    while(SDL_PollEvent(&ev))
    {
      switch(ev.type)
      {
        case (SDL_QUIT):
          {
            game_running = 0;
          } break;
        case (SDL_KEYDOWN):
          {
            if (ev.key.keysym.sym == SDLK_w)
            {
              controller.move_up = 1;
            }
            if (ev.key.keysym.sym == SDLK_a)
            {
              controller.move_left = 1;
              key_down_time[PK_A] = timer.tCurr;
            }
            if (ev.key.keysym.sym == SDLK_s)
            {
              controller.move_down = 1;
            }
            if (ev.key.keysym.sym == SDLK_d)
            {
              controller.move_right = 1;
              key_down_time[PK_D] = timer.tCurr;
            }
            if (ev.key.keysym.sym == SDLK_SPACE)
            {
              controller.jump = 1;
            }
            if (ev.key.keysym.sym == SDLK_g)
            {
              controller.toggle_gravity = 1;
            }
          } break;
        case (SDL_KEYUP):
          {
            if (ev.key.keysym.sym == SDLK_w)
            {
              controller.move_up = 0;
            }
            if (ev.key.keysym.sym == SDLK_a)
            {
              controller.move_left = 0;
              key_down_time[PK_A] = 0.0f;
            }
            if (ev.key.keysym.sym == SDLK_s)
            {
              controller.move_down = 0;
            }
            if (ev.key.keysym.sym == SDLK_d)
            {
              controller.move_right = 0;
              key_down_time[PK_D] = 0.0f;
            }
          } break;
        default:
          {
            break;
          }
      }
    }
    
    // @section: input processing
    if (controller.toggle_gravity)
    {
      is_gravity = !is_gravity;
      player_velocity = Vec2{0.0f, 0.0f};
      p_move_dir.x = 0.0f;
      effective_force = 0.0f;
      p_motion_dir = {0};
    }
    if (controller.move_up)
    {
      p_move_dir.y = 1.0f;
    }
    if (controller.move_down)
    {
      p_move_dir.y = -1.0f;
    }

    PlatformKey horizontal_move = PK_NIL;
    is_key_down_x = false;
    if (
      key_down_time[PK_A] != 0.0f || 
      key_down_time[PK_D] != 0.0f
    ) {
      horizontal_move = (
        key_down_time[PK_A] > key_down_time[PK_D] ? PK_A : PK_D
      );
    }

    if (horizontal_move == PK_A && controller.move_left) 
    {
      p_move_dir.x = -1.0f;
      is_key_down_x = true;
    } 
    if (horizontal_move == PK_D && controller.move_right) 
    {
      p_move_dir.x = 1.0f;
      is_key_down_x = true;
    }

    // @section: gravity
    Vec2 pd_1 = Vec2{0.0f, 0.0f};
    p_motion_dir = {0};
    r32 accel_computed = 0.0f;
    if (collidey)
    {
      player_velocity.y = 0.0f;
    }
    if (collidex)
    {
      player_velocity.x = 0.0f;
    }
    if (is_gravity)
    {
      // calculate force acting on player
      if (collidey) {
        // @note: can I reduce the states here like I did in the falling case
        // without separate checks
        if (is_key_down_x) {
          r32 updated_force = (
            effective_force + p_move_dir.x*move_accelx*timer.tDelta
          );
          updated_force = clampf(
            updated_force, -move_accelx, move_accelx
          );
          effective_force = updated_force;
        } else {
          r32 friction = 0.0f;
          if (effective_force > 0.0f) {
            friction = -move_accelx*timer.tDelta;
          } else if (effective_force < 0.0f) {
            friction = move_accelx*timer.tDelta;
          }
          r32 updated_force = effective_force + friction;
          effective_force = (
            ABS(updated_force) < 0.5f ? 
            0.0f : updated_force
          );
        }
      } else {
        r32 smoothing_force = effective_force;
        r32 net_force = 0.0f;
        r32 active_force = 0.0f;
        if (!collidex) { 
          net_force += effective_force;
          {
            // @note: air resistance 
            // (arbitrary force in opposite direction to reduce speed)
            // reason: seems that it would work well for the case where
            // player moves from platform move to free_fall
            // since the max speed in respective stages is different this can
            // function as a speed smoother, without too many checks and 
            // explicit checking
            b8 is_force_pos = effective_force > 0.0f;
            b8 is_force_neg = effective_force < 0.0f;
            r32 friction = 0.0f;
            if (is_force_pos) {
              friction = -fall_accelx*timer.tDelta;
            } else if (is_force_neg) {
              friction = fall_accelx*timer.tDelta;
            }
            net_force += friction;
          } 

          {
            // @note: player movement force
            active_force = p_move_dir.x*fall_accelx;
            r32 interm_total_force = net_force + active_force;
            if (ABS(interm_total_force) > fall_accelx) {
              r32 deficit = MIN(fall_accelx - ABS(net_force), 0.0f);
              active_force = p_move_dir.x*deficit;
            }
            net_force += active_force;
          }
        }
        effective_force = net_force;
      }
      
      {
        // horizontal motion setting
        r32 dx1 = effective_force;
        if ( dx1 == 0.0f ) {
          p_move_dir.x = 0.0f;
        }

        if (dx1 < 0.0f) {
          p_motion_dir.x = -1.0f;
        } else if (dx1 > 0.0f) {
          p_motion_dir.x = 1.0f;
        }
        accel_computed = (dx1 - player_velocity.x)/timer.tDelta;
        player_velocity.x = dx1;
        pd_1.x = dx1;
      }

      {
        // vertical motion when falling
        r32 dy1 = player_velocity.y;
        dy1 = dy1 + freefall_accel*timer.tDelta;
        if (controller.jump) {
          dy1 = jump_force;
        }
        if (dy1 < 0.0f) {
          p_motion_dir.y = -1.0f;
        } else if (dy1 > 0.0f) {
          p_motion_dir.y = 1.0f;
        }
        player_velocity.y = dy1;
        pd_1.y = dy1;
      }
    }
    else 
    {
	// @no_clip_movement
        Vec2 dir = get_move_dir(controller);
        pd_1 = dir * 5.0f * render_scale;
        if (pd_1.x < 0.0f) {
          p_motion_dir.x = -1.0f;
        } else if (pd_1.x > 0.0f) {
          p_motion_dir.x = 1.0f;
        }
        if (pd_1.y < 0.0f) {
          p_motion_dir.y = -1.0f;
        } else if (pd_1.y > 0.0f) {
          p_motion_dir.y = 1.0f;
        }
    }

 
    // @section: collision
    Vec3 next_player_position;
    next_player_position.x = state.player.position.x + pd_1.x;
    next_player_position.y = state.player.position.y + pd_1.y;

    Rect player_next = rect(next_player_position, state.player.size);
    
    Rect collision_targets[2] = {state.wall, state.floor};
    b8 is_collide_x = 0;
    b8 is_collide_y = 0;

    for (u32 i = 0; i < 2; i++) {
      Rect target = collision_targets[i];
      
      // @func: check_if_player_colliding_with_target

      b8 t_collide_x = 0;
      // need to adjust player position in case of vertical collisions
      // so need to check which player side collides
      b8 t_collide_bottom = 0;
      b8 t_collide_top = 0;

      r32 prev_top    = state.player.tl.y;
      r32 prev_left   = state.player.tl.x;
      r32 prev_bottom = state.player.br.y;
      r32 prev_right  = state.player.br.x;

      r32 p_top     = player_next.tl.y;
      r32 p_left    = player_next.tl.x;
      r32 p_bottom  = player_next.br.y;
      r32 p_right   = player_next.br.x;

      r32 t_left    = target.tl.x;
      r32 t_top     = target.tl.y;
      r32 t_right   = target.br.x;
      r32 t_bottom  = target.br.y;

      b8 prev_collide_x = !(prev_left > t_right || prev_right < t_left);
      b8 new_collide_yb = (p_bottom < t_top && p_top > t_top);
      b8 new_collide_yt = (p_top > t_bottom && p_bottom < t_bottom);
      if (prev_collide_x && new_collide_yb) {
        t_collide_top = 1;
      }
      if (prev_collide_x && new_collide_yt) {
        t_collide_bottom = 1;
      }

      b8 prev_collide_y = !(prev_top < t_bottom || prev_bottom > t_top);
      b8 new_collide_x = !(p_right < t_left || p_left > t_right);
      if (prev_collide_y && new_collide_x) {
        t_collide_x = 1;
      }

      // @func: update_player_positions_if_sides_colliding
      if (t_collide_top) {
        state.player.position.y -= (prev_bottom - t_top - 0.1f);
      } else if (t_collide_bottom) {
        state.player.position.y += (t_bottom - prev_top - 0.1f);
      }

      is_collide_x = is_collide_x || t_collide_x;
      is_collide_y = is_collide_y || t_collide_top || t_collide_bottom;

    }

    if (!is_collide_x) {
      if (p_motion_dir.x != 0.0f) {
        renderer->cam_update = true;
      }
      state.player.position.x = next_player_position.x;
    }
    if (!is_collide_y) {
      if (p_motion_dir.y != 0.0f) {
        renderer->cam_update = true;
      }
      state.player.position.y = next_player_position.y;
    }

    state.player = rect(state.player.position, state.player.size);
    collidex = is_collide_x;
    collidey = is_collide_y;

    // @func: update_camera
    if (renderer->cam_update == true) {
      renderer->cam_update = false;
      Vec2 player_screen = state.player.position.v2() - renderer->cam_pos.v2();

      if (player_screen.x <= cam_lt_limit.x && p_motion_dir.x == -1) {
        renderer->cam_pos.x += pd_1.x;
        renderer->cam_update = true;
      }
      if (player_screen.y >= cam_lt_limit.y && p_motion_dir.y == 1) {
        renderer->cam_pos.y += pd_1.y;
        renderer->cam_update = true;
      }
      if (player_screen.x >= cam_rb_limit.x && p_motion_dir.x == 1) {
        renderer->cam_pos.x += pd_1.x;
        renderer->cam_update = true;
      }
      if (player_screen.y <= cam_rb_limit.y && p_motion_dir.y == -1) {
        renderer->cam_pos.y += pd_1.y;
        renderer->cam_update = true;
      }
      if (renderer->cam_update == true) {
        renderer->cam_view = camera_create4m(
          renderer->cam_pos, 
          add3v(renderer->cam_pos, renderer->cam_look), 
          renderer->preset_up_dir
        );
        renderer->cam_update = false;
      }
    }
    
    // output
    glClearColor(0.8f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // player
    gl_draw_colored_quad_optimized(renderer, 
                         state.player.position,            // position
                         state.player.size,         // size
                         Vec3{0.45f, 0.8f, 0.2f});
    // floor
    gl_draw_colored_quad_optimized(renderer,
                         state.floor.position,
                         state.floor.size,
                         Vec3{1.0f, 1.0f, 1.0f});
    // wall
    gl_draw_colored_quad_optimized(renderer, 
                         state.wall.position,
                         state.wall.size,
                         Vec3{1.0f, 0.0f, 0.0f});

    gl_cq_flush(renderer);

    array_clear(r32, renderer->cq_pos_batch);
    array_clear(r32, renderer->cq_color_batch);
    renderer->cq_batch_count = 0;
    
    // render ui text
    
    if (is_collide_x || is_collide_y)
    {
      gl_render_text(renderer,
                     "is colliding",
                     Vec2{500.0f, 700.0f},      // position
                     28.0f,                     // size
                     Vec3{0.0f, 0.0f, 0.0f});   // color
      
      char movedir_output[50];
      sprintf(movedir_output, "move_dir = %f", p_move_dir.x);
      gl_render_text(renderer,
                     movedir_output,
                     Vec2{500.0f, 60.0f},      // position
                     28.0f,                     // size
                     Vec3{0.0f, 0.0f, 0.0f});   // color

      char speed_output[50];
      sprintf(speed_output, "%f pps", player_velocity.x);
      gl_render_text(renderer,
                     speed_output,
                     Vec2{500.0f, 100.0f},      // position
                     28.0f,                     // size
                     Vec3{0.0f, 0.0f, 0.0f});   // color
      
    }
    
    char fmt_buffer[50];

    sprintf(fmt_buffer, "frametime: %f", timer.tDelta);
    gl_render_text(renderer,
                   fmt_buffer,
                   Vec2{900.0f, 90.0f},      // position
                   28.0f*render_scale,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color
    
    SDL_GL_SwapWindow(window);

  }
  
  arena_clear(&batch_arena);
  free(renderer->ui_text.transforms);
  free(renderer->ui_text.char_indexes);
  free(renderer->ui_text.char_map);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
