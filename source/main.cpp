#include <SDL2/SDL.h>
#include <glad/glad.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdint.h>

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

struct Str256 {
    char buffer[256];
    u32 size;
};

void str_init(Str256 *str) {
    memset(str->buffer, 0, 256*sizeof(unsigned char));
    str->size = 0;
}

Str256 str256(const char *cstr) {
    Str256 str;
    str_init(&str);
    u32 size = strlen(cstr); 
    memcpy((void*)str.buffer, (void*)cstr, size);
    str.size = size;

    return str;
}

void str_pushc(Str256 *str, char c) {
    if (str->size + 1 > 256) {
	return;
    }
    str->buffer[str->size] = c;
    str->size++;
}

void str_push256(Str256 *str, Str256 to_push) {
    u32 available_space = 256 - str->size;
    SDL_assert(available_space >= to_push.size);

    memcpy((void*)&str->buffer[str->size], (void*)to_push.buffer, to_push.size);
    str->size += to_push.size;
}

void str_clear(Str256 *str) {
    memset(str->buffer, 0, str->size * sizeof(unsigned char));
    str->size = 0;
}

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

#define KB(x) (1024 * (x))
#define MB(x) (1024 * KB((x)))
#define GB(x) (1024 * MB((x)))

#include "array/array.cpp"

struct Rect {
  Vec2 lb;
  Vec2 rt;
};

#define PLAYER_Z -1.0f
#define OBSTACLE_Z -2.0f
#define GOAL_Z -3.0f

enum ENTITY_TYPE {
    PLAYER = 0,
    OBSTACLE = 1,
    GOAL = 2,
    INVERT_GRAVITY = 3,
    TELEPORT = 4,
};

static r32 entity_z[10];
static Vec3 entity_colors[10];

struct Entity {
    // @todo: set a base resolution and design the game elements around it
    s32 id;
    ENTITY_TYPE type;
    // raw property values in pixels
    Vec3 raw_position;
    Vec2 raw_size;
    // these properties will have scaling applied
    Vec3 position;
    Vec2 size;
    Rect bounds;
    // teleporter
    u32 link_id;    // which portal this is linked to
};

struct EntityInfo {
    u32 id;
    u32 index; // index into Level->Entities array
};

struct EntityInfoArr {
    EntityInfo *buffer;
    u32 size;
    u32 capacity;
};

#define LEVEL_MAX_ENTITIES 100
const int level_count = 2;
static const char* base_level_path = "./levels/";
static const char *level_names[20] = {
    "level0.txt",
    "level1.txt",
};

struct Level0x1 {
    u32 version = 0x1;
    u32 entity_count;
    Entity *entities;
};

typedef struct Level0x1 Level;

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
  r32_array cq_color_batch;
  // Batched line
  u32 line_sp;
  u32 line_vao;
  u32 line_vbo;
  u32 line_batch_count;
  r32_array line_pos_batch;
  r32_array line_color_batch;

  // ui text 
  TextState ui_text;
}; 

#include "renderer/renderer.h"
#include "renderer/renderer.cpp"

struct Controller {
  b8 move_up;
  b8 move_down;
  b8 move_left;
  b8 move_right;
  b8 jump;
  b8 toggle_gravity;
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
  // the smallest size a unit can be. This scales with render_scale
  Vec2 atom_size;
  Rect camera_bounds;

  // level
  // 0: in progress, 1: complete
  b8 level_state;
  u32 level_index;
  Str256 level_name;
  Level game_level;
  EntityInfo player;
  EntityInfo goal;
  EntityInfoArr obstacles;
  // gameplay
  b8 flip_gravity;
  b8 inside_teleporter;
  b8 teleporting;
  r64 gravity_flip_timer;
};

Rect rect(Vec3 position, Vec2 size) {
  Rect r = {0};

  r.lb.x = position.x;
  r.lb.y = position.y;

  r.rt.x = position.x + size.x;
  r.rt.y = position.y + size.y;

  return r;
}

b8 aabb_collision_rect(Rect a, Rect b) {
    r32 a_left = a.lb.x;
    r32 a_bottom = a.lb.y;
    r32 a_right = a.rt.x;
    r32 a_top = a.rt.y;

    r32 b_left = b.lb.x;
    r32 b_bottom = b.lb.y;
    r32 b_right = b.rt.x;
    r32 b_top = b.rt.y;

    return !(
	    a_left > b_right || a_right < b_left ||
	    a_top < b_bottom || a_bottom > b_top);
}

Entity get_entity_by_id(GameState state, u32 id) {
    Entity res;
    res.id = -1;

    for (int i = 0; i < state.game_level.entity_count; i++) {
	Entity e = state.game_level.entities[i];
	if (e.id == id) {
	    res = e;
	    break;
	}
    }

    // We should always have the entity we are trying to look for
    SDL_assert(res.id != -1);

    return res;
}

void load_level(GameState *state, Arena *level_arena, Str256 level_path) {
    // @step: initialise level state variables
    arena_clear(level_arena);
    memset(&state->game_level, 0, sizeof(Level));
    state->level_state = 0;

    size_t fsize;
    char* level_data = (char*)SDL_LoadFile(level_path.buffer, &fsize);
    SDL_assert(fsize != 0);

    u32 feature_flag = 0;
    u32 entity_flag = 0;
    u32 entity_id_counter = 0;
    Str256 level_property;
    str_init(&level_property);

    Entity level_entity;
    b8 is_comment = 0;
    u32 prop_flag = 0;
    u32 sub_prop_flag = 0;


    // @note: just decide beforehand, that a level should allow (AT MAX) 100 elements
    // this should be way overkill
    state->game_level.entities = (Entity*)arena_alloc(level_arena, LEVEL_MAX_ENTITIES*sizeof(Entity));

    for (int i = 0; i < fsize; i++) {
	char ele = level_data[i];

	// handling comments in level file
	if (ele == '#') {
	    is_comment = true;
	    continue;
	}
	if (ele == '\n' && is_comment) {
	    is_comment = false;
	    continue;
	}
	if (is_comment) {
	    continue;
	}

	if (ele == '\t' || ele == ' ' || ele == '\n') {
	    if (level_property.size == 0) {
		// @note: this will help ignore ' ', '\t', '\n' characters
		// allowing us to type those in the file for better readability
		continue;
	    }
	    switch (prop_flag) {
		case 0: {
		    state->game_level.version = strtol(level_property.buffer, NULL, 16);
		    str_clear(&level_property);
		    prop_flag++;
		    continue;
		} break;
		case 1: {
		    switch (sub_prop_flag) {
			case 0: {
			    // auto-generated id
			    // @note: will be overwritten when id is explicitly defined
			    level_entity.id = entity_id_counter;

			    // type
			    level_entity.type = (ENTITY_TYPE)strtol(level_property.buffer, NULL, 10);

	  	            // set z index based off of entity type
			    level_entity.raw_position.z = entity_z[level_entity.type];
	  	        } break;
	  	        case 1: {
	  	            // posx
	  	            level_entity.raw_position.x = strtol(level_property.buffer, NULL, 10);
	  	        } break;
	  	        case 2: {
	  	            // posy
	  	            level_entity.raw_position.y = strtol(level_property.buffer, NULL, 10);
	  	        } break;
	  	        case 3: {
	  	            // sizex
	  	            level_entity.raw_size.x = strtol(level_property.buffer, NULL, 10);
	  	        } break;
	  	        case 4: {
	  	            // sizey
	  	            level_entity.raw_size.y = strtol(level_property.buffer, NULL, 10);
	  	        } break;
			case 5: {
			    // pre-defined id
			    level_entity.id = strtol(level_property.buffer, NULL, 10);
			} break;
			case 6: {
			    // linked id
			    level_entity.link_id = strtol(level_property.buffer, NULL, 10);
			}
	  	        default: {
	  	        } break;
	  	    }
	  	    str_clear(&level_property);
	  	    sub_prop_flag++;

	  	    if (ele == '\n') {
			state->game_level.entities[state->game_level.entity_count] = level_entity;
			state->game_level.entity_count++;
	  	        entity_id_counter++;
	  	        sub_prop_flag = 0;
	  	        memset(&level_entity, 0, sizeof(Entity));
	  	    }
		    SDL_assert(state->game_level.entity_count <= LEVEL_MAX_ENTITIES);
	  	    continue;
	  	}
		default: {
		} break;
	    }
	}
	str_pushc(&level_property, ele);
    }

    // @function: load_entities_info and scale
    state->obstacles.buffer = (EntityInfo*)arena_alloc(level_arena, state->game_level.entity_count*sizeof(EntityInfo));
    state->obstacles.size = 0;
    state->obstacles.capacity = state->game_level.entity_count;
    for (u32 i = 0; i < state->game_level.entity_count; i++) {
	Entity e = state->game_level.entities[i];
	e.position = e.raw_position;
	e.size = e.raw_size * state->atom_size * state->render_scale.x;
	e.bounds = rect(e.position, e.size);

	EntityInfo o;
	o.id = e.id;
	o.index = i;

	switch (e.type) {
	    case PLAYER: {
		state->player = o;
	    } break;
	    case OBSTACLE: 
	    case INVERT_GRAVITY: {
		state->obstacles.buffer[state->obstacles.size] = o;
		state->obstacles.size++;
	    } break;
	    case GOAL: {
		state->goal = o;
	    } break;
	    default: {
	    } break;
	}
	state->game_level.entities[i] = e;
    }

    SDL_free(level_data);
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

Vec2 get_screen_position_from_percent(GameState state, Vec2 v) {
  Vec2 screen_pos = v;
  screen_pos.x = state.render_scale.x*state.screen_size.x*v.x/100.0f;
  screen_pos.y = state.render_scale.y*state.screen_size.y*v.y/100.0f;

  return screen_pos;
}

int main(int argc, char* argv[])
{
  u32 base_scr_width = 1024;
  u32 base_scr_height = 768;

  u32 scr_width = 1280;
  u32 scr_height = 960;
  
  {
      // entity configs setup
    entity_colors[PLAYER] = Vec3{0.45f, 0.8f, 0.2f};
    entity_colors[OBSTACLE] = Vec3{1.0f, 1.0f, 1.0f};
    entity_colors[GOAL] = Vec3{ 0.93f, 0.7f, 0.27f };
    entity_colors[INVERT_GRAVITY] = Vec3{1.0f, 0.0f, 0.0f};
    entity_colors[TELEPORT] = Vec3{0.0f, 0.0f, 0.0f};

    entity_z[OBSTACLE] = -3.0f;
    entity_z[GOAL] = -4.0f;
    {
	entity_z[TELEPORT] = -5.0f; 
	entity_z[INVERT_GRAVITY] = -5.0f;
    }
    entity_z[PLAYER] = -6.0f;
  }

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
  
  GLRenderer renderer;
  memset(&renderer, 0, sizeof(GLRenderer));

  u32 pos_ele_count =  BATCH_SIZE * 4*6;
  u32 color_ele_count = BATCH_SIZE * 3*6;
  // 1GB <= (((1b*1024)kb*1024)mb*1024)mb
  size_t mem_size = GB(1);
  void* batch_memory = calloc(mem_size, sizeof(r32));
  Arena batch_arena;
  // quad batch buffers
  arena_init(&batch_arena, (unsigned char*)batch_memory, mem_size*sizeof(r32));
  array_init(&batch_arena, &(renderer.cq_pos_batch), pos_ele_count);
  array_init(&batch_arena, &(renderer.cq_color_batch), color_ele_count);

  // line batch buffers
  u32 line_pos_ele_count = BATCH_SIZE * 4 * 2;
  u32 line_color_ele_count = BATCH_SIZE * 3 * 2;
  array_init(&batch_arena, &(renderer.line_pos_batch), line_pos_ele_count);
  array_init(&batch_arena, &(renderer.line_color_batch), line_color_ele_count);


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
  renderer.cq_sp = quad_sp;
  renderer.cq_vao = quad_vao;

  renderer.cq_batch_sp = cq_batch_sp;
  gl_setup_colored_quad_optimized(&renderer, cq_batch_sp);

  renderer.line_sp = cq_batch_sp;
  gl_setup_line(&renderer, cq_batch_sp);
  
  
  r32 render_scale = 1.0f; //(r32)scr_width / (r32)base_scr_width;
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
  renderer.ui_text.chunk_size = 32;
  renderer.ui_text.pixel_size = 32*render_scale;
  renderer.ui_text.sp = ui_text_sp;
  renderer.ui_text.transforms = (Mat4*)malloc(
    renderer.ui_text.chunk_size*sizeof(Mat4)
  );
  renderer.ui_text.char_indexes = (s32*)malloc(
    renderer.ui_text.chunk_size*sizeof(s32)
  );
  renderer.ui_text.char_map = (TextChar*)malloc(
    128*sizeof(TextChar)
  );
  gl_setup_text(&(renderer.ui_text), roboto_font_face);
  
  
  // ============
  // setup camera
  Vec3 preset_up_dir = Vec3{0.0f, 1.0f, 0.0f};
  renderer.preset_up_dir = preset_up_dir;
  renderer.cam_update = false;
  renderer.cam_pos = Vec3{0.0f, 0.0f, 1.0f};
  renderer.cam_look = camera_look_around(TO_RAD(0.0f), -TO_RAD(90.0f));
  renderer.cam_view = camera_create4m(
    renderer.cam_pos, 
    add3v(renderer.cam_pos, renderer.cam_look), renderer.preset_up_dir
  );
  renderer.cam_proj = orthographic4m(
    0.0f, (r32)scr_width*render_scale,
    0.0f, (r32)scr_height*render_scale,
    0.1f, 10.0f
  );

  // @section: gameplay variables
  r32 motion_scale = 2.0f;
  r32 gravity_diry = 1.0f;
  r32 fall_accelx = 3.0f*motion_scale;
  r32 move_accelx = 4.0f*motion_scale;
  r32 freefall_accel = -11.8f*motion_scale;
  r32 jump_force = 6.5f*motion_scale;
  r32 effective_force = 0.0f;
  Vec2 camera_pan_slow = Vec2{2.0f, 2.0f}*motion_scale;

  Vec2 player_velocity = Vec2{0.0f, 0.0f};
  Vec2 p_move_dir = Vec2{0.0f, 0.0f};
  // direction in which player is effectively travelling
  Vec2 p_motion_dir = Vec2{0.0f, 0.0f};

  // @thinking: level object handling
  // there should be a most smallest supported unit
  // smallest_size: 16x16
  // object placement should be in pixels 
  // in order to scale to different resolutions it should be multiplied by
  // scaling factor
  Vec2 atom_size = Vec2{64.0f, 64.0f};

  GameState state = {0};
  state.atom_size = atom_size;
  state.world_size = Vec2{(r32)scr_width, (r32)scr_height};
  state.screen_size = Vec2{(r32)scr_width, (r32)scr_height};
  state.render_scale = vec2(render_scale);
  Vec2 camera_screen_size = state.screen_size * state.render_scale;

  // @todo: rename rect members (makes more sense)
  // tl -> lt
  // br -> rb
  {
      // @step: calculate_camera_bounds
      Vec2 cam_lb = Vec2{renderer.cam_pos.x, renderer.cam_pos.y};
      Vec2 cam_rt = Vec2{renderer.cam_pos.x + camera_screen_size.x, renderer.cam_pos.y + camera_screen_size.y};
      state.camera_bounds.lb = cam_lb;
      state.camera_bounds.rt = cam_rt;
  }

  // @section: level elements
  
  // @step: init_level_arena
  size_t max_level_entities = 255;
  size_t arena_mem_size = GB(1);
  void* level_mem = malloc(arena_mem_size);
  Arena level_arena;
  size_t arena_size = max_level_entities*(sizeof(Entity) + sizeof(EntityInfo));
  arena_init(&level_arena, (unsigned char*)level_mem, arena_size);

  Str256 base = str256(base_level_path);
  Str256 _level_name = str256(level_names[state.level_index]);
  Str256 level_path = base;
  str_push256(&level_path, _level_name);
  {
      // setup level state
      load_level(&state, &level_arena, level_path);
      // put camera on player
      Entity player = state.game_level.entities[state.player.index];
      renderer.cam_pos.x = player.position.x;
      renderer.cam_pos.y = player.position.y;
      effective_force = 0.0f;
      player_velocity = Vec2{0.0f, 0.0f};
      gravity_diry = 1.0f;
  }

  // gameplay camera movement stuff
  Vec2 cam_lt_limit = {0};
  Vec2 cam_rb_limit = {0};
  cam_lt_limit = get_screen_position_from_percent(
    state, Vec2{30.0f, 70.0f}
  );
  cam_rb_limit = get_screen_position_from_percent(
    state, Vec2{70.0f, 30.0f}
  );

  Controller controller = {0};
  r32 key_down_time[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  b8 is_key_down_x = false;
  
  // gravity calculations
  b8 was_colliding = 0;
  b8 collidex = 0;
  b8 collidey = 0;
  b8 is_gravity = 0;
  
  b8 game_running = 1;

  FrameTimer timer = frametimer();

#if AUDIO
  // @resume: audio has clicky sound, I need to figure generate audio clips that are better.
  // Why is this difficult

  // @section: audio_setup
  ma_result result;
  ma_engine engine;

  result = ma_engine_init(NULL, &engine);
  if (result != MA_SUCCESS) {
      SDL_Log("Failed to initialise audio engine\n");
      return -1;
  }

  ma_sound jump_sound;
  ma_sound landing_sound;
  ma_sound level_complete_sound;
  const char *sound_path = "assets/audio/jump.wav";
  result = ma_sound_init_from_file(&engine, sound_path, 0, NULL, NULL, &jump_sound);
  if (result != MA_SUCCESS) {
      SDL_Log("Failed to load sound: %s\n", sound_path);
      return -1;
  }

  sound_path = "assets/audio/landing.wav";
  result = ma_sound_init_from_file(&engine, sound_path, 0, NULL, NULL, &landing_sound);
  if (result != MA_SUCCESS) {
      SDL_Log("Failed to load sound: %s\n", sound_path);
      return -1;
  }

  sound_path = "assets/audio/level_complete.wav";
  result = ma_sound_init_from_file(&engine, sound_path, 0, NULL, NULL, &level_complete_sound);
  if (result != MA_SUCCESS) {
      SDL_Log("Failed to load sound: %s\n", sound_path);
      return -1;
  }
#endif

  while (game_running) 
  {
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
	    // @todo: fix this janky manual camera movement 
	    if (ev.key.keysym.sym == SDLK_F5)
	    {
		{
		    // setup level state
		    load_level(&state, &level_arena, level_path);
		    // put camera on player
		    Entity player = state.game_level.entities[state.player.index];
		    renderer.cam_pos.x = player.position.x;
		    renderer.cam_pos.y = player.position.y;
		    effective_force = 0.0f;
		    player_velocity = Vec2{0.0f, 0.0f};
		    gravity_diry = 1.0f;
		}
	    }
#if CAM_MANUAL_MOVE
	    if (ev.key.keysym.sym == SDLK_LEFT)
	    {
		renderer.cam_pos.x -= 40.0f * state.render_scale.x;
		renderer.cam_update = true;
	    }
	    if (ev.key.keysym.sym == SDLK_RIGHT)
	    {
		renderer.cam_pos.x += 40.0f * state.render_scale.x;
		renderer.cam_update = true;
	    }
	    if (ev.key.keysym.sym == SDLK_UP)
	    {
		renderer.cam_pos.y += 40.0f * state.render_scale.x;
		renderer.cam_update = true;
	    }
	    if (ev.key.keysym.sym == SDLK_DOWN)
	    {
		renderer.cam_pos.y -= 40.0f * state.render_scale.x;
		renderer.cam_update = true;
	    }
#endif
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
	    if (ev.key.keysym.sym == SDLK_i)
	    {
		state.flip_gravity = 1;
	    }
          } break;
        default:
          {
            break;
          }
      }
    }

    // @section: state based loading
    if (state.level_state == 1) {
	state.level_index = clampi(state.level_index+1, 0, level_count-1);
	Str256 _level_name = str256(level_names[state.level_index]);
	Str256 level_path = base;
	str_push256(&level_path, _level_name);
	{
	    // setup level state
	    load_level(&state, &level_arena, level_path);
	    // put camera on player
	    Entity player = state.game_level.entities[state.player.index];
	    renderer.cam_pos.x = player.position.x;
	    renderer.cam_pos.y = player.position.y;
	    effective_force = 0.0f;
	    player_velocity = Vec2{0.0f, 0.0f};
	    gravity_diry = 1.0f;
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
    if (state.flip_gravity)
    {
	// @resume: I need to add a buffer zone, something like some iframes, so that once I touch a gravity block
	// I don't reflip gravity if I am in contact with the block for 1-2 seconds right after first colliding
	gravity_diry = gravity_diry > 0.0f ? -0.8f : 1.0f;
	//gravity_diry *= -1.0f;
	state.flip_gravity = 0;
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
	// @section: game_movement

      // calculate force acting on player
      if (collidey) {
	  // @note: can I reduce the states here like I did in the falling case
	  // without separate checks
	  if (collidex) {
	      effective_force = 0.0f;
	  } else if (is_key_down_x) {
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
          net_force = effective_force;
	  if (controller.jump) {
	      // @step: if in the air and jumping in a different direction
	      // allow more immediate feeling force, instead of the jump adding into net_force
	      // which gives off, more of a floaty feeling.
	      r32 threshed_force = roundf(net_force);
	      b8 move_dir_different = (threshed_force >= 0 && p_move_dir.x < 0) || (threshed_force <= 0 && p_move_dir.x > 0);
	      if (move_dir_different) {
		  active_force = p_move_dir.x*fall_accelx/2.0f;
		  net_force = active_force;
	      }
	  } else {
	      if (is_key_down_x) {
		  // player is slowing down, in that case, we allow this movement.
		  b8 move_dir_opposite = (net_force > 0 && p_move_dir.x < 0) || (net_force < 0 && p_move_dir.x > 0);
		  if (move_dir_opposite || ABS(net_force) < fall_accelx*0.15f)
		  {
		      active_force = p_move_dir.x*fall_accelx*timer.tDelta;
		      net_force = clampf(net_force + active_force, -fall_accelx, fall_accelx);
		  }
	      } 
	      if (ABS(net_force) >= fall_accelx) {
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
        player_velocity.x = dx1;
        pd_1.x = dx1;
      }

      {
        // vertical motion when falling
        r32 dy1 = player_velocity.y; 
        dy1 = dy1 + gravity_diry * freefall_accel * timer.tDelta;
        if (controller.jump) {
          dy1 = gravity_diry*jump_force;
	  if (!collidey) {
	      // if we are in the air, the jump force is 75% of normal
	      dy1 = gravity_diry * jump_force * 0.75f;
	  }
        }
        if (dy1 < gravity_diry * -0.01f) {
          p_motion_dir.y = -gravity_diry;
        } else if (dy1 > gravity_diry * 0.01f) {
          p_motion_dir.y = gravity_diry;
        }
        player_velocity.y = dy1;
        pd_1.y = dy1;
      }
    }
    else 
    {
	// @no_clip_movement
        Vec2 dir = get_move_dir(controller);
        pd_1 = dir * 8.0f * render_scale;
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
    Entity player = state.game_level.entities[state.player.index];
    Vec3 next_player_position;
    next_player_position.x = player.position.x + pd_1.x;
    next_player_position.y = player.position.y + pd_1.y;

    Rect player_next = rect(next_player_position, player.size);
    
    b8 is_collide_x = 0;
    b8 is_collide_y = 0;

    for (u32 i = 0; i < state.obstacles.size; i++) {
	// @step: check_obstacle_collisions

	// @func: check_if_player_colliding_with_target
	u32 index = state.obstacles.buffer[i].index;
	Entity e = state.game_level.entities[index];
      	Rect target = e.bounds;

	b8 t_collide_x = 0;
	// need to adjust player position in case of vertical collisions
	// so need to check which player side collides
	b8 t_collide_bottom = 0;
	b8 t_collide_top = 0;

	r32 prev_left   = player.bounds.lb.x;
	r32 prev_bottom = player.bounds.lb.y;
	r32 prev_right  = player.bounds.rt.x;
	r32 prev_top    = player.bounds.rt.y;

	r32 p_left    = player_next.lb.x;
	r32 p_bottom  = player_next.lb.y;
	r32 p_right   = player_next.rt.x;
	r32 p_top     = player_next.rt.y;

	r32 t_left    = target.lb.x;
	r32 t_bottom  = target.lb.y;
	r32 t_right   = target.rt.x;
	r32 t_top     = target.rt.y;

	b8 prev_collide_x = !(prev_left > t_right || prev_right < t_left);
	b8 new_collide_target_top = (p_bottom < t_top && p_top > t_top);
	b8 new_collide_target_bottom = (p_top > t_bottom && p_bottom < t_bottom);
	if (prev_collide_x && new_collide_target_top) {
	  t_collide_top = 1;
	}
	if (prev_collide_x && new_collide_target_bottom) {
	  t_collide_bottom = 1;
	}

	b8 prev_collide_y = !(prev_top < t_bottom || prev_bottom > t_top);
	b8 new_collide_x = !(p_right < t_left || p_left > t_right);
	if (prev_collide_y && new_collide_x) {
	  t_collide_x = 1;
	}

	// @func: update_player_positions_if_sides_colliding
	if (t_collide_top) {
	  player.position.y -= (prev_bottom - t_top - 0.1f);
	} else if (t_collide_bottom) {
	  player.position.y += (t_bottom - prev_top - 0.1f);
	}

	if (e.type == INVERT_GRAVITY && (t_collide_x || t_collide_top || t_collide_bottom)) {
	    // @note: gravity inverter mechanic
	    // 1. touch block, gravity flips
	    // 2. for 2 second, after gravity is flipped, gravity will not be flipped 
	    // (this will collapse various cases where the player is trying to reflip gravity, 
	    // but immediate contact after flipping makes this awkward and infeasible)
	    if (state.gravity_flip_timer <= 0) {
		state.gravity_flip_timer = 1000.0f;
		state.flip_gravity = 1;
	    }
	}

	is_collide_x = is_collide_x || t_collide_x;
	is_collide_y = is_collide_y || t_collide_top || t_collide_bottom;
    }

    if (!is_collide_x) {
      player.position.x = next_player_position.x;
    }
    if (!is_collide_y) {
      player.position.y = next_player_position.y;
    }

    // check collision with goal
    if (!is_collide_x && !is_collide_y) {
	Entity goal = state.game_level.entities[state.goal.index];
	Rect target = goal.bounds;

	state.level_state = aabb_collision_rect(player_next, target);
    }

    // @section: teleport
    b8 inside_teleporter_now = 0;
    b8 teleporting_now = state.teleporting;
    Vec2 teleported_position = Vec2{player.position.x, player.position.y};
    for (u32 i = 0; i < state.game_level.entity_count; i++) {
	/*
	 * @note;
	 * TELEPORT START ...
	 * 1. go inside a teleport block, player marked as in block
	 * 2. hit teleport block center, player marked as teleporting
	 * 3. player teleported to new block
	 * 4. once player exits the new block, player marked as in block false
	 * 5. then player marked as teleporting false
	 * ... TELEPORT COMPLETE
	 */
	Entity e = state.game_level.entities[i];
	if (e.type != TELEPORT) {
	    continue;
	}

	Rect target = e.bounds;

	if (teleporting_now) {
	    // check if player is outside of this teleport block or not
	    b8 t_collide = aabb_collision_rect(player.bounds, target);
	    inside_teleporter_now |= t_collide;

	    continue;
	}
	// check if player is completely inside teleport block
	b8 t_collide = aabb_collision_rect(player.bounds, target);
	if (!t_collide) {
	    continue;
	}

	inside_teleporter_now |= t_collide;
	
	// check if player x-axis is within teleport x-axis
	Vec2 displacement;
	displacement.x = player.position.x - e.position.x;
	displacement.y = player.position.y - e.position.y;

	if (ABS(displacement.x) <= 5.0f*render_scale) {
	    teleporting_now = 1;
	    {
		// @step: teleport_player
		Entity teleport_to = get_entity_by_id(state, e.link_id);
		// set next position
		teleported_position.x = teleport_to.position.x + displacement.x;
		teleported_position.y = teleport_to.position.y + displacement.y;
		// add displacement so it gives a smooth effect of just moving over to someplace
	    }
	}
    }
    {
	// update teleport variable
	state.inside_teleporter = inside_teleporter_now;
	state.teleporting = teleporting_now && state.inside_teleporter;
	if (state.teleporting) {
	    player.position.x = teleported_position.x;
	    player.position.y = teleported_position.y;
	}
    }
    {
	state.gravity_flip_timer = MAX(state.gravity_flip_timer - timer.tDeltaMS, 0.0f);
    }

    {
	// @step: update player variables
	player.bounds = rect(player.position, player.size);
	was_colliding = collidex || collidey;
	collidex = is_collide_x;
	collidey = is_collide_y;

	state.game_level.entities[state.player.index] = player;
    }

    // @func: update_camera
    {
	// camera movement and handling
	// Cases:
	// - A new level loads, the camera position needs to be on the player 
	// (part of level loading) [Focus on Player]
	// - Player is moving and camera needs to follow
	// - Player has stopped moving and camera needs to slowly adjust (linearly)
	// - Player teleports, camera needs to move to the player:
	//  - if player is within view camera needs to slowly adjust the player, 
	//  and pan linearly until player is in level focus
	//  - if player is out of camera view, jump 
	//  (linearly but slightly faster) to the player
	// - If player is outside, pan quickly (linearly) to player
	// - If player is at the boundary of a level, 
	// respect the level boundary and do not center the player. 
	//  Pan camera, up until the edge of the level boundary. 
	//  Do no move the camera beyond the level boundary.
	//
	//  Based off of these cases, this is the behavior I can see:
	//  1. Player is moving at the edges of the screen, follow.
	//  2. Player is stopped, pan slowly until player is in the focus region 
	//  (need to define focus region)
	//  3. Player is outside the screen, pan to player 
	//  (go from current camera position to player position linearly)

	// @step: player is at the edge of the screen
	// get players visible bounds (padding around the player to consider it be visible for the camera)
	Vec2 vis_lb = player.bounds.lb - (Vec2{40.0f, 60.0f} * state.render_scale);
	Vec2 vis_rt = player.bounds.rt + (Vec2{40.0f, 60.0f} * state.render_scale);
	Rect vis_bounds;
	vis_bounds.lb = vis_lb;
	vis_bounds.rt = vis_rt;
	Rect cam_bounds = state.camera_bounds;

	Vec2 camera_center = Vec2{state.camera_bounds.rt.x/2.0f, state.camera_bounds.rt.y/2.0f};
	Vec2 player_camera_offset = player.position.v2() - camera_center;
	// check if vis_bounds inside camera_bounds
	b8 is_player_in_camera = (
		vis_bounds.lb.x >= cam_bounds.lb.x && vis_bounds.lb.y >= cam_bounds.lb.y &&
		vis_bounds.rt.x <= cam_bounds.rt.x && vis_bounds.rt.y <= cam_bounds.rt.y
	);

	if (!is_player_in_camera) {
	    r32 stepx_multiplier = player_camera_offset.x < 0 ? -1.0f : 1.0f;
	    r32 stepy_multiplier = player_camera_offset.y < 0 ? -1.0f : 1.0f;

	    r32 camera_stepx = stepx_multiplier * MIN(ABS(player_camera_offset.x), camera_pan_slow.x);
	    r32 camera_stepy = stepy_multiplier * MIN(ABS(player_camera_offset.y), camera_pan_slow.y);

	    Vec2 distance_scaler;	    
	    {
		// @step: calculate distance scaler
		// @note: this is to help scale how quickly the camera needs to pan
		// this is based off of how far the player is from the camera position.
		// The reason this is discrete instead of continuous is to give better predictability
		// (at this stage)
		// @note: make this continuous, so it pans smoothly. Movement is jerky at step boundaries
		u32 dist_stepx = (u32)SDL_floorf(ABS(player_camera_offset.x) / 100);
		u32 dist_stepy = (u32)SDL_floorf(ABS(player_camera_offset.y) / 100);

		if (dist_stepx >= 0 && dist_stepx < 4) {
		    distance_scaler.x = 8.0f;
		} else if (dist_stepx >= 4 && dist_stepx < 8) {
		    distance_scaler.x = 6.0f;
		} else {
		    distance_scaler.x = 4.0f;
		}

		if (dist_stepy >= 0 && dist_stepy < 4) {
		    distance_scaler.y = 8.0f;
		} else if (dist_stepy >= 4 && dist_stepy < 8) {
		    distance_scaler.y = 6.0f;
		} else {
		    distance_scaler.y = 4.0f;
		}
	    }
	    renderer.cam_pos.x += camera_stepx*timer.tDeltaMS/distance_scaler.x;
	    renderer.cam_pos.y += camera_stepy*timer.tDeltaMS/distance_scaler.y;

	    renderer.cam_update = 1;
	}


	b8 player_moving_up = p_motion_dir.y == gravity_diry*1 && !is_collide_y;
	b8 player_moving_down = p_motion_dir.y == gravity_diry*-1 && !is_collide_y;

	player_camera_offset = player.position.v2() - renderer.cam_pos.v2();
	// @step: player moving at edges of the screen
	if (player_camera_offset.x <= cam_lt_limit.x && p_motion_dir.x == -1) {
	    renderer.cam_pos.x += pd_1.x;
	    renderer.cam_update = 1;
	}
	if (player_camera_offset.y >= cam_lt_limit.y && 
		player_moving_up) {
	    renderer.cam_pos.y += pd_1.y;
	    renderer.cam_update = 1;
	}
	if (player_camera_offset.x >= cam_rb_limit.x && p_motion_dir.x == 1) {
	    renderer.cam_pos.x += pd_1.x;
	    renderer.cam_update = 1;
	}
	if (player_camera_offset.y <= cam_rb_limit.y && 
		player_moving_down) {
	    renderer.cam_pos.y += pd_1.y;
	    renderer.cam_update = 1;
	}

    }

    {
	// @step: update camera variables
	if (renderer.cam_update == true) {
	    renderer.cam_view = camera_create4m(
		    renderer.cam_pos,
		    add3v(renderer.cam_pos, renderer.cam_look),
		    renderer.preset_up_dir
		    );
	    renderer.cam_update = false;
	    {
		// @step: calculate_camera_bounds
		Vec2 cam_lb = Vec2{renderer.cam_pos.x, renderer.cam_pos.y};
		Vec2 cam_rt = Vec2{renderer.cam_pos.x + camera_screen_size.x, renderer.cam_pos.y + camera_screen_size.y};
		state.camera_bounds.lb = cam_lb;
		state.camera_bounds.rt = cam_rt;
	    }
	}
    }
    
    // output

#if AUDIO
    // @section: audio 
    if (controller.jump) {
	ma_sound_start(&jump_sound);
    }
    if ((collidex || collidey) && !was_colliding) {
	//ma_sound_set_pitch(&landing_sound, 0.5f);
	//ma_sound_set_volume(&landing_sound, 0.2);
	ma_sound_start(&landing_sound);
    }
    if (state.level_state == 1) {
	ma_sound_start(&level_complete_sound);
    }
#endif

    glClearColor(0.8f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // @section: rendering
    // draw line
    gl_draw_line(
	    &renderer, 
	    Vec3{0.0f, 500.0f, -1.0f}, 
	    Vec3{1000.0f, 500.0f, -1.0f},
	    Vec3{0.0f, 0.0f, 0.0f});

    gl_line_flush(&renderer);

    // render_entities
    for (int i = 0; i < state.game_level.entity_count; i++) {
	Entity entity = state.game_level.entities[i];
	Vec3 entity_center = Vec3{
	    entity.position.x + entity.size.x/2.0f,
	    entity.position.y + entity.size.y/2.0f, 
	    entity.position.z
	};
	Vec3 color = entity_colors[entity.type];
	gl_draw_colored_quad_optimized(
		&renderer,
		entity_center,
		entity.size,
		color
	);
    }

    gl_cq_flush(&renderer);

    array_clear(&renderer.cq_pos_batch);
    array_clear(&renderer.cq_color_batch);
    renderer.cq_batch_count = 0;
    
    // render ui text
    
    char level_state_output[50];
    sprintf(level_state_output, "is level clear = %d", state.level_state);
    gl_render_text(
	    &renderer,
	    level_state_output,
	    Vec2{600.0f, 900.0f},
	    28.0f,
	    Vec3{0.0f, 0.0f, 0.0f});


    if (is_collide_x || is_collide_y)
    {
      gl_render_text(&renderer,
                     "is colliding",
                     Vec2{500.0f, 700.0f},      // position
                     28.0f,                     // size
                     Vec3{0.0f, 0.0f, 0.0f});   // color
      
      char movedir_output[50];
      sprintf(movedir_output, "move_dir = %f", p_move_dir.x);
      gl_render_text(&renderer,
                     movedir_output,
                     Vec2{500.0f, 60.0f},      // position
                     28.0f,                     // size
                     Vec3{0.0f, 0.0f, 0.0f});   // color

      char speed_output[50];
      sprintf(speed_output, "%f pps", player_velocity.x);
      gl_render_text(&renderer,
                     speed_output,
                     Vec2{500.0f, 100.0f},      // position
                     28.0f,                     // size
                     Vec3{0.0f, 0.0f, 0.0f});   // color
    }
    
    char fmt_buffer[50];

    sprintf(fmt_buffer, "frametime: %f", timer.tDelta);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{900.0f, 90.0f},      // position
                   28.0f*render_scale,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color
    
    //sprintf(fmt_buffer, "inside_teleporter: %d\nteleporting: %d", state.inside_teleporter, state.teleporting);
    //gl_render_text(&renderer,
    //               fmt_buffer,
    //               Vec2{900.0f, 190.0f},      // position
    //               28.0f*render_scale,                     // size
    //               Vec3{0.0f, 0.0f, 0.0f});   // color

    SDL_GL_SwapWindow(window);

    update_frame_timer(&timer);
    enforce_frame_rate(&timer, 60);
  }
  
  //ma_engine_uninit(&engine);
  free(level_mem);
  free(batch_memory);
  free(renderer.ui_text.transforms);
  free(renderer.ui_text.char_indexes);
  free(renderer.ui_text.char_map);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
