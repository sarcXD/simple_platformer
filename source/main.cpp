//-----------------------------
#include <stdio.h>
//-----------------------------


//-----------------------------
#include <SDL2/SDL.h>
#include <glad/glad.h>
//-----------------------------

//-----------------------------
#include "SDL2/SDL_events.h"
#include "SDL2/SDL_keycode.h"
#include "SDL2/SDL_video.h"
#include "core.h"
#include "memory/arena.h"
#include "math.h"
#include "array/array.cpp"
#include "renderer/renderer.h"
#include "renderer/renderer.cpp"
//-----------------------------


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

enum ButtonState {
    NONE	= 0,
    HOVER	= 1,
    PRESSED	= 2,
    CLICK	= 3
};

enum GameScreen {
    MAIN_MENU	    = 0,
    PAUSE_MENU	    = 1,
    GAMEPLAY	    = 2,
    SETTINGS_MENU   = 3,
};

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
    DEBUG_LINE = 5,
    TEXT = 6,
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

#define ARR_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#define LEVEL_MAX_ENTITIES 100
static const char* base_level_path = "./levels/";
static const char *level_names[] = {
    "level0.txt",
    "level1.txt",
    "level2.txt",
    "level3.txt",
    "level4.txt",
    "level5.txt",
    "level6.txt",
    "level7.txt",
    "level8.txt",
    "level9.txt",
    "level10.txt",
    "hello_portal.txt",
    "portal_wind_up_no_jump.txt",
    "portal_thereNback.txt",
};
const int level_count = ARR_SIZE(level_names);

struct Level0x1 {
    u32 version = 0x1;
    u32 entity_count;
    Entity *entities;
};

typedef struct Level0x1 Level;


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
    Vec2 screen_size;
    // the scaling factor to increase/decrease size of game assets
    Vec2 render_scale;
    // the smallest size a unit can be. This scales with render_scale
    Vec2 atom_size;
    Rect camera_bounds;
    
    // level
    // 0: in progress, 1: complete
    b8 level_state;
    s32 level_index;
    Str256 level_path_base;
    Str256 level_name;
    Level game_level;
    EntityInfo player;
    EntityInfo goal;
    EntityInfoArr obstacles;
    // interaction
    IVec2 mouse_position;
    b8 mouse_down;
    b8 mouse_up;
    // gameplay
    b8 flip_gravity;
    b8 inside_teleporter;
    b8 teleporting;
    r32 gravity_diry;
    r32 effective_force;
    Vec2 player_velocity;
    r64 gravity_flip_timer;
    // rendering
    GLRenderer renderer;
};

Rect rect(Vec2 position, Vec2 size) {
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
	e.position = Vec3{
	    e.raw_position.x * state->render_scale.x,
	    e.raw_position.y * state->render_scale.y,
	    e.raw_position.z
	};
	e.size = e.raw_size * state->atom_size;
	e.bounds = rect(e.position.v2(), e.size);

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

void setup_level(GameState *state, GLRenderer *renderer, Arena *arena) 
{
    Str256 _level_name = str256(level_names[state->level_index]);
    Str256 level_path = state->level_path_base;
    str_push256(&level_path, _level_name);

    load_level(state, arena, level_path);

    Entity goal = state->game_level.entities[state->goal.index];
    Vec2 scr_dims;
    renderer->cam_pos.x = goal.position.x - (state->screen_size.x/2.0f * state->render_scale.x);
    renderer->cam_pos.y = goal.position.y - (state->screen_size.y/2.0f * state->render_scale.y);
    state->effective_force = 0.0f;
    state->player_velocity = Vec2{0.0f, 0.0f};
    state->gravity_diry = 1.0f;
    renderer->cam_update = 1;
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

struct DropdownOption {
    Str256 label;
};

struct UiButton {
    r32 font_size;
    Vec2 size;
    Vec3 position;

    // behavior
    Vec3 bgd_color_primary;
    Vec3 bgd_color_hover;
    Vec3 bgd_color_pressed;

    Vec3 font_color_primary;
    Vec3 font_color_hover;
    Vec3 font_color_pressed;

    Str256 text;
};

// @description: This is a very scrappy function that goes through the button render
// logic and pre-computes items like text dimensions
// It does not support, tabs and newlines
Vec2 ui_button_get_text_dims(GLRenderer renderer, char *text, r32 font_size) {
    Vec2 max_dims = Vec2{0, 0};

    u32 running_index = 0;
    r32 linex = 0;
    r32 render_scale = font_size/(r32)renderer.ui_text.pixel_size;
    r32 font_scale = renderer.ui_text.scale*render_scale;

    char *char_iter = text;
    while (*char_iter != '\0') {
	TextChar render_char = renderer.ui_text.char_map[*char_iter];
	if (*char_iter == ' ') {
	    linex += (font_scale * render_char.advance);
	    char_iter++;
	    continue;
	}
	if (*char_iter == '\t' || *char_iter == '\n') {
	    char_iter++;
	    continue;
	}

	linex += (font_scale * render_char.advance);
	char prev_char = *char_iter;
	char_iter++;
	char curr_char = *char_iter;

	if (curr_char) {
	    r32 kern = font_scale * stbtt_GetCodepointKernAdvance(&renderer.ui_text.font, prev_char, curr_char);
	    linex += kern;
	}
	if (linex > max_dims.x) {
	    max_dims.x = linex;
	} 
	r32 y1 = render_scale*render_char.size.y;
	if (y1 > max_dims.y) {
	    max_dims.y = y1; 
	}
	running_index++;
	if (running_index >= renderer.ui_text.chunk_size) {
	    return max_dims;
	}
    }
    return max_dims;
}

// @description: This function handles drawing, interaction and rendering logic 
// for an immediate mode button
ButtonState ui_button(GameState state, UiButton button) {
    ButtonState btn_state = ButtonState::NONE;
    Vec2 pos = get_screen_position_from_percent(
	state, button.position.v2()
    );
    Vec2 quad_size = button.size * state.render_scale;

    // @step: get text size and position
    r32 font_size = button.font_size * state.render_scale.y;
    if (!font_size) {
	font_size = 0.6f * quad_size.y;
    }
    Vec2 txt_dims = ui_button_get_text_dims(state.renderer, 
					    button.text.buffer, 
					    font_size);
    r32 txt_base_offsety = -5.0f*state.render_scale.y;
    Vec2 txt_center_offset = Vec2{
	(quad_size.x - txt_dims.x)/2.0f,
	(quad_size.y - txt_dims.y)/2.0f + txt_base_offsety
    };
    Vec2 txt_pos = pos + txt_center_offset;

    // @step: get button color and state
    b8 is_mouse_on_button = 0;
    {
	// :check_if_mouse_on_button
	Rect btn_rect = rect(pos, quad_size);
	is_mouse_on_button = (
	    (state.mouse_position.x >= btn_rect.lb.x && 
	    state.mouse_position.y >= btn_rect.lb.y) &&
	    (state.mouse_position.x <= btn_rect.rt.x &&
	    state.mouse_position.y <= btn_rect.rt.y)
	);
    }
    Vec3 bgd_color = button.bgd_color_primary;
    if (is_mouse_on_button) {
	if (state.mouse_down) {
	    // pressed
	    bgd_color = button.bgd_color_pressed;
	    btn_state = ButtonState::PRESSED;
	} else if (state.mouse_up) {
	    btn_state = ButtonState::CLICK;
	} else {
	    // hover
	    bgd_color = button.bgd_color_hover;
	    btn_state = ButtonState::HOVER;
	}
    }

    gl_draw_quad(
	state.renderer.quad, 
	&state.renderer.ui_cam,
	Vec3{
	    pos.x + quad_size.x/2.0f, 
	    pos.y + quad_size.y/2.0f, 
	    button.position.z,
	}, 
	quad_size, 
	bgd_color);

    gl_render_text(
	&state.renderer,
	button.text.buffer,
	Vec3{txt_pos.x, txt_pos.y, button.position.z},
	Vec3{0.0f, 0.0f, 0.0f},
	font_size);


    return btn_state;
}

// @section: main
int main(int argc, char* argv[])
{
    Vec2 scr_dims = Vec2{1920, 1080};

    Vec2 render_dims = Vec2{1920, 1080};
  
  {
      // entity configs setup
    entity_colors[PLAYER] = Vec3{0.45f, 0.8f, 0.2f};
    entity_colors[OBSTACLE] = Vec3{1.0f, 1.0f, 1.0f};
    entity_colors[GOAL] = Vec3{ 0.93f, 0.7f, 0.27f };
    entity_colors[INVERT_GRAVITY] = Vec3{1.0f, 0.0f, 0.0f};
    entity_colors[TELEPORT] = Vec3{0.0f, 0.0f, 0.0f};

    entity_z[TEXT] = -3.0f;
    entity_z[DEBUG_LINE] = -4.0f;
    r32 entity_base_z = -5.0f;
    entity_z[OBSTACLE] = entity_base_z - 1.0f;
    entity_z[GOAL] = entity_base_z - 2.0f;
    {
	entity_z[TELEPORT] = entity_base_z - 3.0f; 
	entity_z[INVERT_GRAVITY] = entity_base_z - 3.0f;
    }
    entity_z[PLAYER] = entity_base_z - 4.0f;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    printf("Error initialising SDL2: %s\n", SDL_GetError());
    return -1;
  }
  
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  SDL_Window* window = SDL_CreateWindow("simple platformer",
                                        SDL_WINDOWPOS_UNDEFINED, 
                                        SDL_WINDOWPOS_UNDEFINED,
                                        render_dims.x, render_dims.y,
                                        SDL_WINDOW_OPENGL
					| SDL_WINDOW_FULLSCREEN_DESKTOP
					);

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
  
  GameState state = {0};
  enum GameScreen game_screen = GAMEPLAY;
  GLRenderer *renderer = &state.renderer;
  memset(renderer, 0, sizeof(GLRenderer));

  u32 pos_ele_count =  BATCH_SIZE * 4*6;
  u32 color_ele_count = BATCH_SIZE * 3*6;
  // 1GB <= (((1b*1024)kb*1024)mb*1024)mb
  size_t mem_size = GB(1);
  void* batch_memory = calloc(mem_size, sizeof(r32));
  Arena batch_arena;
  // quad batch buffers
  arena_init(&batch_arena, (unsigned char*)batch_memory, mem_size*sizeof(r32));
  array_init(&batch_arena, &(renderer->cq_pos_batch), pos_ele_count);
  array_init(&batch_arena, &(renderer->cq_color_batch), color_ele_count);

  // line batch buffers
  u32 line_pos_ele_count = BATCH_SIZE * 4 * 2;
  u32 line_color_ele_count = BATCH_SIZE * 3 * 2;
  array_init(&batch_arena, &(renderer->line_pos_batch), line_pos_ele_count);
  array_init(&batch_arena, &(renderer->line_color_batch), line_color_ele_count);


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
  u32 quad_vao = gl_setup_quad(quad_sp);
  renderer->quad.sp = quad_sp;
  renderer->quad.vao = quad_vao;

  renderer->cq_batch_sp = cq_batch_sp;
  gl_setup_colored_quad_optimized(renderer, cq_batch_sp);

  renderer->line_sp = cq_batch_sp;
  gl_setup_line_batch(renderer, cq_batch_sp);
  
  
  Vec2 render_scale = Vec2{(r32)render_dims.x/scr_dims.x, (r32)render_dims.y/scr_dims.y};
{
    // ==========
    // setup text
    // setup stb_truetype stuff

    size_t fsize = 0;
    unsigned char *font_buffer = (unsigned char*)SDL_LoadFile("./assets/fonts/Roboto.ttf", &fsize);
    stbtt_InitFont(&renderer->ui_text.font, font_buffer, 0);

    renderer->ui_text.sp = ui_text_sp;
    renderer->ui_text.chunk_size = 128;
    renderer->ui_text.pixel_size = 32*render_scale.x;
    renderer->ui_text.transforms = (Mat4*)malloc(
      renderer->ui_text.chunk_size*sizeof(Mat4)
    );
    renderer->ui_text.char_indexes = (s32*)malloc(
      renderer->ui_text.chunk_size*sizeof(s32)
    );
    renderer->ui_text.char_map = (TextChar*)malloc(
      128*sizeof(TextChar)
    );

    gl_setup_text(&renderer->ui_text);
}

  
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
    0.0f, (r32)render_dims.x,
    0.0f, (r32)render_dims.y,
    0.1f, 15.0f
  );
  // fixed_screen_camera
  renderer->ui_cam.update = 1;
  renderer->ui_cam.pos = renderer->cam_pos;
  renderer->ui_cam.look = renderer->cam_look;
  renderer->ui_cam.view = renderer->cam_view;
  renderer->ui_cam.proj = renderer->cam_proj;

  // @thinking: level object handling
  // there should be a most smallest supported unit
  // smallest_size: 16x16
  // object placement should be in pixels 
  // in order to scale to different resolutions it should be multiplied by
  // scaling factor
  Vec2 atom_size = Vec2{64.0f, 64.0f}*render_scale;

  state.atom_size = atom_size;
  state.screen_size = scr_dims;
  state.render_scale = render_scale;
  Vec2 camera_screen_size = state.screen_size * state.render_scale;
  state.level_path_base = str256(base_level_path);

  // @section: gameplay variables
  u32 jump_count = 1;
  r64 jump_timer = 0;
  r32 motion_scale = 2.0f*state.render_scale.x;
  state.gravity_diry = 1.0f;
  r32 fall_accelx = 3.0f*motion_scale;
  r32 move_accelx = 4.0f*motion_scale;
  r32 max_speedx = 5.0f*motion_scale;
  r32 freefall_accel = -11.8f*motion_scale;
  r32 jump_force = 6.5f*motion_scale;
  state.effective_force = 0.0f;
  Vec2 camera_pan_slow = Vec2{2.0f, 2.0f}*motion_scale;

  state.player_velocity = Vec2{0.0f, 0.0f};
  Vec2 p_move_dir = Vec2{0.0f, 0.0f};
  // direction in which player is effectively travelling
  Vec2 p_motion_dir = Vec2{0.0f, 0.0f};


  // @todo: rename rect members (makes more sense)
  // tl -> lt
  // br -> rb
  state.camera_bounds = rect(renderer->cam_pos.v2(), camera_screen_size);

  // @section: level elements
  
  // @step: init_level_arena
  size_t max_level_entities = 255;
  size_t arena_mem_size = GB(1);
  void* level_mem = malloc(arena_mem_size);
  Arena level_arena;
  size_t arena_size = max_level_entities*(sizeof(Entity) + sizeof(EntityInfo));
  arena_init(&level_arena, (unsigned char*)level_mem, arena_size);
  setup_level(&state, &state.renderer, &level_arena);

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
    b8 is_collide_bottom = 0;
    b8 is_collide_top = 0;
  b8 is_gravity = 0;
  
  b8 game_running = 1;

  FrameTimer timer = frametimer();

  while (game_running) 
  {
    controller.jump = 0;
    controller.toggle_gravity = 0;
    state.mouse_up = 0;

    IVec2 mouse_position_world;
    IVec2 mouse_position_clamped;
    SDL_Event ev;
    while(SDL_PollEvent(&ev))
    {
      switch(ev.type)
      {
        case (SDL_QUIT):
          {
		game_running = 0;
          } break;
	case (SDL_MOUSEBUTTONUP):
	    {
		SDL_GetMouseState(&state.mouse_position.x, &state.mouse_position.y);
		state.mouse_position.y = render_dims.y - state.mouse_position.y;
		state.mouse_down = 0;
		state.mouse_up = 1;
	    } break;
	case (SDL_MOUSEBUTTONDOWN):
	    {
		u32 btn_x = SDL_GetMouseState(&state.mouse_position.x, &state.mouse_position.y);
		state.mouse_position.y = render_dims.y - state.mouse_position.y;
		if (SDL_BUTTON(btn_x) == SDL_BUTTON(SDL_BUTTON_LEFT)) {
		    state.mouse_down = 1;
		    state.mouse_up = 0;
		}
	    } break;
	case (SDL_MOUSEMOTION):
	  {
	      SDL_GetMouseState(&state.mouse_position.x, &state.mouse_position.y);
	      // flip mouse y to map it Y at Top -> Y at Bottom (like in maths)
	      state.mouse_position.y = render_dims.y - state.mouse_position.y;
	      // get mouse world position
	      mouse_position_world.x = state.mouse_position.x + (s32)renderer->cam_pos.x;
	      mouse_position_world.y = state.mouse_position.y + (s32)renderer->cam_pos.y;
	      // clamp mouse position based off of the grids we draw (this will make level object placement easier)
	      mouse_position_clamped.x = mouse_position_world.x - ((mouse_position_world.x) % (s32)(atom_size.x));
	      mouse_position_clamped.y = mouse_position_world.y - ((mouse_position_world.y) % (s32)(atom_size.y));

	  } break;
        case (SDL_KEYDOWN):
          {
	    if (ev.key.keysym.sym == SDLK_f)
	    {
		// maximise/minimize
#if 0
		// @note: This is janky, so will need to find some other workaround for this.
		// get window display index
		s32 display_ind = SDL_GetWindowDisplayIndex(window);
		SDL_Rect win_rect;
		s8 res = SDL_GetDisplayBounds(display_ind, &win_rect);
		if (!res) {
		    static bool fullscreen = true;
		    int desktop_flag = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
		    SDL_SetWindowFullscreen(window, desktop_flag);
		    int ww, wh = 0;
		    SDL_SetWindowSize(window, render_dims.x, render_dims.y);
		    fullscreen = !fullscreen;
		    // @note: I may need to recreate the entire opengl context and what not
		}

		int brk = 1;
#endif
	    }
            if (ev.key.keysym.sym == SDLK_ESCAPE)
            {
		// gamemode paused
		if (game_screen == GAMEPLAY) {
		    game_screen = PAUSE_MENU;
		} else if (game_screen == SETTINGS_MENU) {
		    game_screen = PAUSE_MENU;
		} else if (game_screen == PAUSE_MENU) {
		    game_screen = GAMEPLAY;
		}
            }
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
	    if (ev.key.keysym.sym == SDLK_HOME)
	    {
		state.level_index = MAX(state.level_index - 1, 0);
		setup_level(&state, &state.renderer, &level_arena);
	    }
	    if (ev.key.keysym.sym == SDLK_END)
	    {
		state.level_index = MIN(state.level_index + 1, level_count-1);
		setup_level(&state, &state.renderer, &level_arena);
	    }
	    if (ev.key.keysym.sym == SDLK_F5)
	    {
		setup_level(&state, &state.renderer, &level_arena);
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

    if (game_screen == GAMEPLAY) {
	// @section: state based loading
	if (state.level_state == 1) {
	    state.level_index = clampi(state.level_index+1, 0, level_count-1);
	    setup_level(&state, &state.renderer, &level_arena);
	}
	
	// @section: input processing
	if (controller.toggle_gravity)
	{
	  is_gravity = !is_gravity;
	  state.player_velocity = Vec2{0.0f, 0.0f};
	  p_move_dir.x = 0.0f;
	  state.effective_force = 0.0f;
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

	if (controller.jump && jump_count > 0 && jump_timer > 100.0f) {
	    controller.jump = 1;
	    jump_count--;
	    jump_timer = 0.0f;
	} else {
	    controller.jump = 0;
	}

	// jump increment
	jump_timer += timer.tDeltaMS;
	if (is_collide_bottom == 1 && state.gravity_diry > 0.0f) {
	    jump_count = 1;
	}
	if (is_collide_top == 1 && state.gravity_diry < 0.0f) {
	    jump_count = 1;
	}


	// @section: gravity
	if (state.flip_gravity)
	{
	    // @resume: I need to add a buffer zone, something like some iframes, so that once I touch a gravity block
	    // I don't reflip gravity if I am in contact with the block for 1-2 seconds right after first colliding
	    state.gravity_diry = state.gravity_diry > 0.0f ? -0.8f : 1.0f;
	    //gravity_diry *= -1.0f;
	    state.flip_gravity = 0;
	}
	Vec2 pd_1 = Vec2{0.0f, 0.0f};
	p_motion_dir = {0};
	if (collidey)
	{
	  state.player_velocity.y = 0.0f;
	}
	if (collidex)
	{
	  state.player_velocity.x = 0.0f;
	}
	if (is_gravity)
	{
	    // @section: game_movement

	  // calculate force acting on player
	  if (collidey) {
	      // @note: can I reduce the states here like I did in the falling case
	      // without separate checks
	      if (collidex) {
		  state.effective_force = 0.0f;
	      } else if (is_key_down_x) {
		  r32 updated_force = (
			  state.effective_force + p_move_dir.x*move_accelx*timer.tDelta
			  );
		  updated_force = clampf(
			  updated_force, -max_speedx, max_speedx
			  );
		  state.effective_force = updated_force;
	      } else {
		  r32 friction = 0.0f;
		  if (state.effective_force > 0.0f) {
		    friction = -move_accelx*timer.tDelta;
		  } else if (state.effective_force < 0.0f) {
		    friction = move_accelx*timer.tDelta;
		  }
		  r32 updated_force = state.effective_force + friction;
		  state.effective_force = (
		    ABS(updated_force) < 0.5f ? 
		    0.0f : updated_force
		  );
	      }
	  } else {
	    r32 smoothing_force = state.effective_force;
	    r32 net_force = 0.0f;
	    r32 active_force = 0.0f;
	    if (!collidex) { 
	      net_force = state.effective_force;
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
		      b8 is_force_pos = state.effective_force > 0.0f;
		      b8 is_force_neg = state.effective_force < 0.0f;
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
	    state.effective_force = net_force;
	  }
	  
	  {
	    // horizontal motion setting
	    r32 dx1 = state.effective_force;
	    if ( dx1 == 0.0f ) {
	      p_move_dir.x = 0.0f;
	    }

	    if (dx1 < 0.0f) {
	      p_motion_dir.x = -1.0f;
	    } else if (dx1 > 0.0f) {
	      p_motion_dir.x = 1.0f;
	    }
	    state.player_velocity.x = dx1;
	    pd_1.x = dx1;
	  }

	  {
	    // vertical motion when falling
	    r32 dy1 = state.player_velocity.y; 
	    dy1 = dy1 + state.gravity_diry * freefall_accel * timer.tDelta;
	    if (controller.jump) {
	      dy1 = state.gravity_diry*jump_force;
	      if (!collidey) {
		  // if we are in the air, the jump force is 75% of normal
		  dy1 = state.gravity_diry * jump_force * 0.75f;
	      }
	    }
	    if (dy1 < state.gravity_diry * -0.01f) {
	      p_motion_dir.y = -state.gravity_diry;
	    } else if (dy1 > state.gravity_diry * 0.01f) {
	      p_motion_dir.y = state.gravity_diry;
	    }
	    state.player_velocity.y = dy1;
	    pd_1.y = dy1;
	  }
	}
	else 
	{
	    // @no_clip_movement
	    Vec2 dir = get_move_dir(controller);
	    pd_1 = dir * 8.0f * render_scale.x;
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

	Rect player_next = rect(next_player_position.v2(), player.size);
	
	b8 is_collide_x = 0;
	b8 is_collide_y = 0;
	is_collide_bottom = 0;
	is_collide_top = 0;

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

	    if (!is_collide_y) {
		b8 prev_collide_x = !(prev_left > t_right || prev_right < t_left);
		b8 new_collide_target_top = (p_bottom < t_top && p_top > t_top);
		b8 new_collide_target_bottom = (p_top > t_bottom && p_bottom < t_bottom);
		if (prev_collide_x && new_collide_target_top) {
		  t_collide_top = 1;
		}
		if (prev_collide_x && new_collide_target_bottom) {
		  t_collide_bottom = 1;
		}
	    }

	    {
		b8 prev_collide_y = !(prev_top < t_bottom + 0.2f || prev_bottom > t_top);
		b8 new_collide_x = !(p_right < t_left || p_left > t_right);
		if (prev_collide_y && new_collide_x) {
		  t_collide_x = 1;
		}
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
		    state.gravity_flip_timer = 500.0f;
		    state.flip_gravity = 1;
		}
	    }

	    is_collide_x = is_collide_x || t_collide_x;
	    is_collide_y = is_collide_y || t_collide_top || t_collide_bottom;
	    is_collide_bottom = is_collide_bottom || t_collide_top;
	    is_collide_top = is_collide_top || t_collide_bottom;
	}

	if (!is_collide_x) {
	  player.position.x = next_player_position.x;
	}
	if (!is_collide_y) {
	  player.position.y = next_player_position.y;
	}

	// check collision with goal
	{
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
	    Vec2 player_center = player.position.v2() + player.size/2.0f;
	    Vec2 entity_center = e.position.v2() + e.size/2.0f;
	    Vec2 displacement = player_center - entity_center;

	    if (ABS(displacement.x) <= 5.0f*render_scale.x || ABS(displacement.y) <= 5.0f*render_scale.x) {
		teleporting_now = 1;
		{
		    // @step: teleport_player
		    Entity teleport_to = get_entity_by_id(state, e.link_id);
		    Vec2 teleport_to_center = teleport_to.position.v2() + teleport_to.size/2.0f;
		    // set next position
		    Vec2 teleported_position_center = teleport_to_center + displacement;
		    teleported_position = teleported_position_center - player.size/2.0f;
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
	    player.bounds = rect(player.position.v2(), player.size);
	    was_colliding = collidex || collidey;
	    collidex = is_collide_x;
	    collidey = is_collide_y;

	    state.game_level.entities[state.player.index] = player;
	}

	// @section: camera_update
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
	    Vec2 vis_lb = player.bounds.lb - (Vec2{120.0f, 60.0f} * state.render_scale.x);
	    Vec2 vis_rt = player.bounds.rt + (Vec2{120.0f, 60.0f} * state.render_scale.y);
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
		renderer->cam_pos.x += camera_stepx*timer.tDeltaMS/distance_scaler.x;
		renderer->cam_pos.y += camera_stepy*timer.tDeltaMS/distance_scaler.y;

		renderer->cam_update = 1;
	    }


	    b8 player_moving_up = p_motion_dir.y == state.gravity_diry*1 && !is_collide_y;
	    b8 player_moving_down = p_motion_dir.y == state.gravity_diry*-1 && !is_collide_y;

	    player_camera_offset = player.position.v2() - renderer->cam_pos.v2();
	    // @step: player moving at edges of the screen
	    if (player_camera_offset.x <= cam_lt_limit.x && p_motion_dir.x == -1) {
		renderer->cam_pos.x += pd_1.x;
		renderer->cam_update = 1;
	    }
	    if (player_camera_offset.y >= cam_lt_limit.y && 
		    player_moving_up) {
		renderer->cam_pos.y += pd_1.y;
		renderer->cam_update = 1;
	    }
	    if (player_camera_offset.x >= cam_rb_limit.x && p_motion_dir.x == 1) {
		renderer->cam_pos.x += pd_1.x;
		renderer->cam_update = 1;
	    }
	    if (player_camera_offset.y <= cam_rb_limit.y && 
		    player_moving_down) {
		renderer->cam_pos.y += pd_1.y;
		renderer->cam_update = 1;
	    }

	}
    }

    {
	// @step: update camera variables
	if (renderer->cam_update == true) {
	    renderer->cam_view = camera_create4m(
		    renderer->cam_pos,
		    add3v(renderer->cam_pos, renderer->cam_look),
		    renderer->preset_up_dir
		    );
	    renderer->cam_update = false;
	    state.camera_bounds = rect(renderer->cam_pos.v2(), camera_screen_size);
	}
    }
    
    // output
    glClearColor(0.8f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // @section: rendering
    // @step: render draw lines
    if (game_screen == GAMEPLAY) {
	{
	    // @step: draw vertical lines
	    s32 line_index = (s32)state.camera_bounds.lb.x/atom_size.x;
	    for (s32 x = state.camera_bounds.lb.x; x <= state.camera_bounds.rt.x; x += atom_size.x) {
		s32 offset = line_index*atom_size.x - x;
		Vec3 start = Vec3{ 
		    (r32)(x + offset), 
		    state.camera_bounds.lb.y, 
		    entity_z[DEBUG_LINE]
		};
		Vec3 end = Vec3{
		    (r32)(x + offset),
		    state.camera_bounds.rt.y, 
		    entity_z[DEBUG_LINE]
		};

		gl_draw_line_batch(
			&state.renderer,
			start,
			end,
			Vec3{0.1, 0.1, 0.1}
			);

		line_index++;
	    }

	    line_index = (s32)state.camera_bounds.lb.y/atom_size.y;
	    // @step: draw horizontal lines
	    for (s32 y = state.camera_bounds.lb.y; y <= state.camera_bounds.rt.y; y += atom_size.x) {
		s32 offset = line_index * atom_size.y - y;
		Vec3 start = Vec3{
		    state.camera_bounds.lb.x, 
			(r32)(y + offset), 
			entity_z[DEBUG_LINE]
		};
		Vec3 end = Vec3{
		    state.camera_bounds.rt.x, 
			(r32)(y + offset), 
			entity_z[DEBUG_LINE]
		};

		gl_draw_line_batch(
			&state.renderer,
			start,
			end,
			Vec3{0.1, 0.1, 0.1}
			);
	    
		line_index++;
	    }
	    gl_flush_line_batch(&state.renderer);
	}

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
		    &state.renderer,
		    entity_center,
		    entity.size,
		    color
	    );
	}

	gl_cq_flush(&state.renderer);

	array_clear(&renderer->cq_pos_batch);
	array_clear(&renderer->cq_color_batch);
	renderer->cq_batch_count = 0;
	
	char fmt_buffer[50];
	sprintf(fmt_buffer, "frametime: %f", timer.tDelta);
	gl_render_text(&state.renderer,
		       fmt_buffer,
		       Vec3{900.0f, 90.0f, entity_z[TEXT]},      // position
		       Vec3{0.0f, 0.0f, 0.0f},
		       28.0f*render_scale.x);   // color
	
	sprintf(fmt_buffer, "GridX: %d, GridY: %d", mouse_position_clamped.x, mouse_position_clamped.y);
	gl_render_text(
		&state.renderer,
		fmt_buffer,
		Vec3{0.0f, 0.0f, entity_z[TEXT]},
		Vec3{0.0f, 0.0f, 0.0f}, 
		28.0f*render_scale.x);

    } else {
	    renderer->ui_cam.update = 1;

	    UiButton button = {0};
	    button.size = Vec2{120.0f, 40.0f};
	    button.bgd_color_primary = Vec3{1.0f, 1.0f, 1.0f};
	    button.bgd_color_hover = Vec3{0.5f, 1.0f, 0.5f};
	    button.bgd_color_pressed = Vec3{1.0f, 0.5f, 0.5f};

	    button.text = str256("Resume");
	    button.position = Vec3{10.0f, 40.0f, entity_z[TEXT]}; 
	    if (ui_button(state, button) == ButtonState::CLICK) {
		game_screen = GAMEPLAY;
	    }

	    button.text = str256("Settings");
	    button.position = Vec3{10.0f, 32.0f, entity_z[TEXT]};
	    if (ui_button(state, button) == ButtonState::CLICK) {
		game_screen = SETTINGS_MENU;
	    }

	    button.text = str256("Quit");
	    button.position = Vec3{10.0f, 24.0f, entity_z[TEXT]};
	    if (ui_button(state, button) == ButtonState::CLICK) {
		game_running = 0;
	    }

	    // settings menu
	    if (game_screen == SETTINGS_MENU) {
		UiButton back_button = button;

		back_button.text = str256("Apply");
		back_button.position = Vec3{30.0f, 40.0f, entity_z[TEXT]};
		gl_render_text(
		    renderer, "Resolution", 
		    Vec3{800, 800, entity_z[TEXT]}, 
		    Vec3{0.0f, 0.0f, 0.0f}, 24.0f*state.render_scale.y
		);
		{
		    // @params
		    Vec3 ms_value_pos = Vec3{1000.0f, 800.0f, entity_z[TEXT]};
		    Vec2 ms_value_size = Vec2{120.0f, 40.0f};

		    Vec3 ms_value_pos_adjusted = ms_value_pos; 
		    ms_value_pos_adjusted.x += ms_value_size.x/2.0f;
		    ms_value_pos_adjusted.y += ms_value_size.y/2.0f;

		    // draw multi select value box
		    gl_draw_quad(
			state.renderer.quad,
			&state.renderer.ui_cam,
			ms_value_pos_adjusted,
			ms_value_size,
			Vec3{1.0f, 1.0f, 1.0f}
		    );
		    static bool is_toggle_open = false;
		    {
			Vec3 ms_toggle_pos = Vec3{
			    ms_value_pos.x + ms_value_size.x, 
			    ms_value_pos.y, 
			    ms_value_pos.z
			};
			Vec2 ms_toggle_size = Vec2{40.0f, ms_value_size.y};
			Vec3 ms_toggle_pos_adjusted = ms_toggle_pos;
			ms_toggle_pos_adjusted.x += ms_toggle_size.x/2.0f;
			ms_toggle_pos_adjusted.y += ms_toggle_size.y/2.0f;

			b8 is_mouse_on_button = 0;
			{
				// check_if_mouse_on_button
				Rect btn_rect = rect(ms_toggle_pos.v2(), ms_toggle_size);
				is_mouse_on_button = (
				    (state.mouse_position.x >= btn_rect.lb.x && 
				    state.mouse_position.y >= btn_rect.lb.y) &&
				    (state.mouse_position.x <= btn_rect.rt.x &&
				    state.mouse_position.y <= btn_rect.rt.y)
				);
			}
			Vec3 ms_toggle_color = {0.8f, 0.8f, 0.8f};
			if (is_mouse_on_button) {
			    if (state.mouse_down) {
			        // pressed
			        //btn_state = ButtonState::PRESSED;
				ms_toggle_color = {0.8f, 0.8f, 0.8f};
			    } else if (state.mouse_up) {
			        //btn_state = ButtonState::CLICK;
				is_toggle_open = !is_toggle_open;
			    } else {
			        // hover
			        //btn_state = ButtonState::HOVER;
				ms_toggle_color = {0.6f, 0.6f, 0.6f};
			    }
			}
			gl_draw_quad(
			    state.renderer.quad,
			    &state.renderer.ui_cam,
			    ms_toggle_pos_adjusted,
			    ms_toggle_size,
			    ms_toggle_color
			);
			if (is_toggle_open) {
			    {
				// draw toggle option
				Vec2 ms_option_size = Vec2{ms_value_size.x + ms_toggle_size.x, ms_value_size.y};
				Vec3 ms_option_pos = Vec3{ms_value_pos.x, ms_value_pos.y - ms_option_size.y, ms_value_pos.z};

				gl_draw_quad(
				    state.renderer.quad,
				    &state.renderer.ui_cam,
				    Vec3{ms_option_pos.x + ms_option_size.x/2.0f,
					ms_option_pos.y + ms_option_size.y/2.0f,
					ms_option_pos.z},
				    ms_option_size,
				    Vec3{1.0f, 0.0f, 0.0f}
				);
			    }
			}
		    }

		    // multi-select drop down
		    Str256 dropdown_options[] = {
			str256("2560x1440"),
			str256("1920x1080"),
			str256("1280x720")
		    };


		}
	    }
    }

    char fmt_buffer[50];
    sprintf(fmt_buffer, "MouseX: %d, MouseY: %d", state.mouse_position.x, state.mouse_position.y);
    gl_render_text(
	    &state.renderer,
	    fmt_buffer,
	    Vec3{0.0f, 40.0f, entity_z[TEXT]},
	    Vec3{0.0f, 0.0f, 0.0f}, 
	    28.0f*render_scale.x);

    SDL_GL_SwapWindow(window);

    update_frame_timer(&timer);
    enforce_frame_rate(&timer, 60);
  }
  
  //ma_engine_uninit(&engine);
  free(level_mem);
  free(batch_memory);
  free(state.renderer.ui_text.transforms);
  free(state.renderer.ui_text.char_indexes);
  free(state.renderer.ui_text.char_map);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
