#include <SDL2/SDL.h>
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <ft2build.h>
#include FT_FREETYPE_H

/*
* Project Estimation/Target ~ 1 - 2 months
* Tasks:
* DONE:
* - gravity - very barebones version done
* - horizontal motion on ground
*   - accelerate
*   - constant speed
*   - decelerate to give impression of sliding
* - horizontal motion when falling
*   - inertia-like movement when accelerating prior to falling
*   - loss of inertia when player was at high speed before sliding off
*   - small movement when in free fall
*   - movement and deceleration when not moving in free fall
* TODO:
* - Platform collision
*   - Fix collision and make it work for each object
* - Jumping
* - Level completion
* - Level Selection
* - Level Creation
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

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define R32_MAX ((r32)(size_t) - 1)
#define R64_MAX ((r64)(size_t) - 1)

#include "math.h"

enum PMoveState {
  NO_MOVE     = 0,
  ACCEL       = 1,
  KEEP        = 2,
  DECEL       = 3,
  FALL_FREE   = 4,
  FALL_NORM   = 5,
  FALL_KEEP   = 6,
  FALL_DECEL  = 7
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

struct GLRenderer {
  // colored quad
  b8  cq_init;
  u32 cq_sp;
  u32 cq_vao;
  // camera
  b8   cam_update;
  Vec3 cam_pos;
  Vec3 cam_look;
  Mat4 cam_view;
  Mat4 cam_proj;
  // ui text 
  TextState ui_text;
}; 

struct Controller {
  b8 move_up;
  b8 move_down;
  b8 move_left;
  b8 move_right;
  b8 jump;
};

struct GameState {
  // player
  b8 p_jump_status;
  r32 p_jump_period;
  r32 p_jumpx;
  r32 p_jumpy;
  Vec3 player_position;
  Vec2 player_size;
  // floor
  Vec3 floor_position;
  Vec2 floor_size;
  // wall
  Vec3 wall_position;
  Vec2 wall_size;
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
      1, GL_TRUE, (renderer->cam_proj).buffer
    );
    renderer->cq_init = 1;
  }
  // setting quad size
  Mat4 model = init_value4m(1.0);
  Mat4 scale = scaling_matrix4m(size.x, size.y, 0.0f);
  model = multiply4m(scale, model);
  // setting quad position
  Mat4 translation = translation_matrix4m(position.x, position.y, position.z);
  model = multiply4m(translation, model);
  // setting color
  glUniform3fv(glGetUniformLocation(renderer->cq_sp, "Color"), 1, color.data);
  
  glUniformMatrix4fv(
    glGetUniformLocation(renderer->cq_sp, "Model"), 
    1, GL_TRUE, model.buffer
  );
  glUniformMatrix4fv(
    glGetUniformLocation(renderer->cq_sp, "View"), 
    1, GL_TRUE, (renderer->cam_view).buffer
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
                     1, GL_TRUE, renderer->cam_view.buffer);
  glUniformMatrix4fv(glGetUniformLocation(renderer->ui_text.sp, "Projection"), 
                     1, GL_TRUE, renderer->cam_proj.buffer);
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
                           GL_TRUE, &(renderer->ui_text.transforms[0].buffer[0]));
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
                       GL_TRUE, &(renderer->ui_text.transforms[0].buffer[0]));
    r32 texture_map_loc = glGetUniformLocation(renderer->ui_text.sp, "TextureMap");
    glUniform1iv(texture_map_loc, render_count, renderer->ui_text.char_indexes);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, render_count);
    running_index = 0;
    memset(renderer->ui_text.transforms, 0, render_count);
    memset(renderer->ui_text.char_indexes, 0, render_count);
  }
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
  
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  SDL_Window* window = SDL_CreateWindow("simple platformer",
                                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
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
  SDL_GL_SetSwapInterval(1);
  
  size_t read_count;
  u32 quad_sp = gl_shader_program_from_path(
																						"./source/shaders/colored_quad.vs.glsl", 
																						"./source/shaders/colored_quad.fs.glsl"
																						);
  u32 ui_text_sp = gl_shader_program_from_path(
																							 "./source/shaders/ui_text.vs.glsl",
																							 "./source/shaders/ui_text.fs.glsl"
																							 );
  u32 quad_vao = gl_setup_colored_quad(quad_sp);
  
  GLRenderer renderer;
  renderer.cq_sp = quad_sp;
  renderer.cq_vao = quad_vao;
  
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
															 0, 
															 &roboto_font_face
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
  renderer.ui_text.pixel_size = 512;
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
  renderer.cam_update = 1;
  renderer.cam_pos = Vec3{0.0f, 0.0f, 1.0f};
  renderer.cam_look = camera_look_around(To_Radian(0.0f), -To_Radian(90.0f));
  renderer.cam_view = camera_create4m(
    renderer.cam_pos, 
    add3v(renderer.cam_pos, renderer.cam_look), preset_up_dir
  );
  renderer.cam_proj = orthographic_projection4m(
    0.0f, (r32)scr_width*1.5f, 
    0.0f, (r32)scr_height*1.5f, 
    0.1f, 10.0f
  );
  
  // ======
  // player
  r32 fall_velocity = 3.0f;
  r32 fall_decel = -6.0f;
  r32 move_velocity = 6.0f;
  r32 move_accel = 6.0f;
  r32 move_decel = -8.0f;
  GameState state = {0};
  state.p_jump_period = 1.0f;
  state.player_position = Vec3{0.0f, 70.0f, -1.0f};
  state.player_size = Vec2{40.0f, 40.0f};
  state.floor_position = Vec3{600.0f, 800.0f, -2.0f};
  state.floor_size = Vec2{400.0f, 20.0f};
  state.wall_position = Vec3{170.0f, 100.0f, -1.0f};
  state.wall_size = Vec2{20.0f, 80.0f};
  Controller controller = {0};
  r32 key_down_time[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  
  // gravity calculations
  b8 collidex = 0;
  b8 collidey = 0;
  b8 activate_gravity = 0;
  Vec2 player_move_t = Vec2{0.0f, 0.0f};
  Vec2 player_decel_t = Vec2{0.0f, 0.0f};
  b8 is_player_moving = false;
  
  // player force variables
  r32 gravity = -9.8f;
  r32 player_mass = 1.0f;
  Vec2 player_acceleration = Vec2{0.0f, 0.0f};
  Vec2 player_velocity = Vec2{0.0f, 0.0f};
  Vec2 position_displacement = Vec2{0.0f, 0.0f};
  r32 force_factor = 1.0f;
  PMoveState p_move_state = NO_MOVE;
  Vec2 p_move_dir_old = Vec2{0.0f, 0.0f};
  Vec2 p_move_dir = Vec2{0.0f, 0.0f};
  
  b8 cndbrk = 0; // a conditional debug trick in code
  b8 game_running = 1;
  r32 time_prev = SDL_GetTicks64() / 1000.0f;
  r32 time_curr = time_prev;
  r32 time_delta = time_curr - time_prev;
  while (game_running) 
  {
    time_prev = time_curr;
    time_curr = SDL_GetTicks64() / 1000.0f;
    time_delta = time_curr - time_prev;
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
              key_down_time[PK_A] = time_curr;
            }
            if (ev.key.keysym.sym == SDLK_s)
            {
              controller.move_down = 1;
            }
            if (ev.key.keysym.sym == SDLK_d)
            {
              controller.move_right = 1;
              key_down_time[PK_D] = time_curr;
            }
            if (ev.key.keysym.sym == SDLK_SPACE)
            {
              controller.jump = 1;
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
    // @note: because of how I am handling the different states
    // of movement, there is a bug where if I change direction
    // the wrong motion gets considered, that needs to be fixed
    Vec2 player_force = Vec2{0.0f, 0.0f};
    if (controller.move_up)
    {
      player_force.y = force_factor;
      p_move_dir.y = 1.0f;
    }
    if (controller.move_down)
    {
      player_force.y = -force_factor;
      p_move_dir.y = -1.0f;
    }

    PlatformKey horizontal_move = PK_NIL;
    is_player_moving = false;
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
      player_force.x = -force_factor;
      p_move_dir.x = -1.0f;
      is_player_moving = true;
    } 
    if (horizontal_move == PK_D && controller.move_right) 
    {
      player_force.x = force_factor;
      p_move_dir.x = 1.0f;
      is_player_moving = true;
    }

    if (controller.jump)
    {
      // activate gravity
      controller.jump = 0;
      activate_gravity = !activate_gravity;
      player_velocity = Vec2{0.0f, 0.0f};
      p_move_dir.x = 0.0f;
    }
    
    b8 was_moving_on_ground = p_move_state == ACCEL || p_move_state == KEEP;
    if (activate_gravity) {
      b8 is_move_dir_changed = p_move_dir.x != p_move_dir_old.x;
      if (!collidey) {
        // fall states
        if (is_player_moving) {
          if (is_move_dir_changed) {
            p_move_state = FALL_KEEP;
          } else if (
            was_moving_on_ground || p_move_state == FALL_NORM
          ) {
            p_move_state = FALL_NORM;
          } else if (p_move_state != FALL_KEEP) {
            p_move_state = FALL_KEEP;
          }        
        } else {
          if (p_move_state == NO_MOVE) {
            p_move_state = FALL_FREE;
          }
          if (p_move_state != FALL_FREE) {
            p_move_state = FALL_DECEL;
          }        
        }
      } else {
        // standard motion states
        if (is_player_moving) {
          if (p_move_state != KEEP || is_move_dir_changed) {
            p_move_state = ACCEL;
          }         
        } else {
          if (p_move_state == FALL_FREE) {
            p_move_state = NO_MOVE;
          }
          if (p_move_state != NO_MOVE) {
            p_move_state = DECEL;
          }
        }
      }
    }
    
    // @section: gravity
    p_move_dir_old = p_move_dir;
    Vec2 pd_1 = Vec2{0.0f, 0.0f};
    r32 accel_computed = 0.0f;
    if (collidey)
    {
      player_move_t.y = 0.0f;
      player_velocity.y = 0.0f;
    }
    if (collidex)
    {
      player_move_t.x = 0.0f;
      player_velocity.x = 0.0f;
    }
    if (activate_gravity)
    {
      if (!collidey) {
        // vertical motion when falling
        r32 dy1 = player_velocity.y;
        dy1 = dy1 + (-9.8f)*time_delta;
        player_velocity.y = dy1;
        pd_1.y = dy1;
      }

      // @note: define equations of motion for player horizontal movement
      // phase 1: ramp up
      // y = x => v = v0 + at (v0 = 0)
      // y = 4 => v = 4 (a = 0, v0 = 4)
      // 4 = -1.5x + 1.5
      
      if (!collidex) {
        r32 dx1 = player_velocity.x;
        switch (p_move_state) {
          case ACCEL:
            {
              dx1 += move_accel*time_delta*p_move_dir.x;
            } break;
          case KEEP:
            {
              dx1 = move_velocity*p_move_dir.x;
            } break;
          case DECEL:
            {
              dx1 += move_decel*time_delta*p_move_dir.x;
            } break;
          case FALL_NORM:
            {
              if (ABS(player_velocity.x) <= fall_velocity) {
                dx1 = fall_velocity*p_move_dir.x;
              } else {
                dx1 -= 2.0f*time_delta*p_move_dir.x;
              }
            } break;
          case FALL_KEEP:
            {
              dx1 = fall_velocity*p_move_dir.x;
            } break;
          case FALL_DECEL:
            {
              dx1 += fall_decel*time_delta*p_move_dir.x;
            } break;
          default:
            {
            } break;
        }
        // checks for motion on ground
        if (ABS(dx1) <= 0.5f && p_move_state == DECEL) {
          p_move_state = NO_MOVE;
          p_move_dir.x = 0.0f;
          dx1 = 0.0f;
        }
        if (ABS(dx1) >= move_velocity && was_moving_on_ground) {
          p_move_state = KEEP;
        }

        // checks for motion in air
        if (ABS(dx1) <= fall_velocity && p_move_state == FALL_NORM) {
          p_move_state = FALL_KEEP;
        }
        if (ABS(dx1) <= 0.5f && p_move_state == FALL_DECEL) {
          p_move_state = FALL_FREE;
          p_move_dir.x = 0.0f;
          dx1 = 0.0f;
        }

        accel_computed = (dx1 - player_velocity.x)/time_delta;
        player_velocity.x = dx1;
        pd_1.x = dx1;
      }
    }
    else 
    {
      pd_1 = mul2vf(player_force, 8.0f);
    }
    
    // @section: collision
    // player
    
		// @note: 
    //
    // Approach 1
    // 1. check if player colliding with floor using AABB collision
    // 2. check if player sides colliding with left of floor
    //  2.1. clamp the player next position with bounds
    //  (side-effect) => player horizontal velocity sets to 0
    // 3. check if player sides colliding with right of floor
    //  3.1. clamp the player next position with bounds
    //  (side-effect) => player horizontal velocity sets to 0
    // 4. if neither sides collide then that means it was colliding vertically
    // (with top or bottom)
    //  4.1. clamp the player position
    // 5 if no collision y, update player vertical position
    //  5.1. if no y collision and also no x collision, update x position
    //
    // @verdict:
    // This absolutely sucks, this is awful and downright insane, still it led
    // me to the next discovery.
    //
    // @todo:
    // Approach 2
    // Use surface normals, and direction vectors. Figure out this math part
    // and how exactly those would be computed or would matter, but the main
    // point is, that would allow a far better approach to detect collisions than
    // whatever I am doing right now.
    
    Vec2 next_player_position = Vec2{
      state.player_position.x, 
      state.player_position.y
    };
    next_player_position = next_player_position + pd_1;
    
    // calculate_position_bounds player
    r32 p_left = next_player_position.x - state.player_size.x;
    r32 p_right = next_player_position.x + state.player_size.x;
    r32 p_top = next_player_position.y + state.player_size.y;
    r32 p_bottom = next_player_position.y - state.player_size.y;
    // calculate_position_bounds floor
    r32 f_left = state.floor_position.x - state.floor_size.x;
    r32 f_right = state.floor_position.x + state.floor_size.x;
    r32 f_top = state.floor_position.y + state.floor_size.y;
    r32 f_bottom = state.floor_position.y - state.floor_size.y;
    
#if leniant_platform_collision_check
    b8 is_bottom_on_obj_top = (p_bottom <= f_top + 5.0f) && 
      (p_bottom >= f_top - 5.0f);
    b8 is_top_on_obj_bottom = (p_top <= f_bottom + 5.0f) && 
      (p_top >= f_bottom - 5.0f);
#endif
    b8 is_bottom_on_obj_top = p_bottom == f_top;
    b8 is_top_on_obj_bottom = p_top == f_bottom;
    b8 can_slidex = (p_left < f_right || p_right > f_left) && 
      (is_bottom_on_obj_top || is_top_on_obj_bottom);
    
#if wall_collision_check
    r32 w_left   = state.wall_position.x - state.wall_size.x;
    r32 w_right  = state.wall_position.x + state.wall_size.x;
    r32 w_top    = state.wall_position.y + state.wall_size.y;
    r32 w_bottom = state.wall_position.y - state.wall_size.y;
    b8 wall_colliding = !(p_left > w_right || p_right < w_left || p_bottom > w_top || p_top < w_bottom);
#endif
    
    b8 floor_colliding = !(
      p_left > f_right || p_right < f_left || 
      p_bottom > f_top || p_top < f_bottom
    );
    
    b8 new_collidex = 0;
    b8 new_collidey = 0;
    
    if (floor_colliding) {
      // check which side we are colliding with
      b8 collide_left = p_right <= f_left + 5.0f;
      b8 collide_right = p_left >= f_right - 5.0f;
      if (collide_left) {
        state.player_position.x = clampf(
          next_player_position.x,
          0.0f,
          f_left - state.player_size.x
        );
        new_collidex = 1;
      } else if (collide_right) {
        state.player_position.x = clampf(
          next_player_position.x,
          f_right + state.player_size.x,
          1.5*scr_width
        );
        new_collidex = 1;
      } else {
        new_collidey = 1;
      }
      state.player_position.y = clampf(
        state.player_position.y, 
        f_bottom - state.player_size.y, 
        f_top + state.player_size.y
      );
    }
    if (!new_collidey) {
      state.player_position.y = next_player_position.y;
      if (!new_collidex) {
        state.player_position.x = next_player_position.x;
      }
    }

    if (can_slidex) {
      state.player_position.x = next_player_position.x;
    }
    collidex = new_collidex;
    collidey = new_collidey;
    
    // output
    glClearColor(0.8f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // floor
    gl_draw_colored_quad(&renderer,
                         state.floor_position,
                         state.floor_size,
                         Vec3{1.0f, 1.0f, 1.0f});
    // wall
    gl_draw_colored_quad(&renderer, 
                         state.wall_position,
                         state.wall_size,
                         Vec3{1.0f, 0.0f, 0.0f});
    // player
    gl_draw_colored_quad(&renderer, 
                         state.player_position,            // position
                         state.player_size,         // size
                         Vec3{0.45f, 0.8f, 0.2f});
    
    // render ui text
    gl_render_text(&renderer,
                   "hello sailor!",
                   Vec2{30.0f, 700.0f},       // position
                   28.0f,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color
    
    if (new_collidex || new_collidey)
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
      
      char accel_output[50];
      sprintf(accel_output, "%f pps^2", accel_computed);
      gl_render_text(&renderer,
                     accel_output,
                     Vec2{500.0f, 150.0f},       // position
                     28.0f,                      // size
                     Vec3{0.0f, 0.0f, 0.0f});    // color
    }
    char move_state_output[50];
    switch(p_move_state) {
      case ACCEL:
        {
          sprintf(move_state_output, "move_dir = ACCEL");
        } break;
      case KEEP:
        {
          sprintf(move_state_output, "move_dir = KEEP");
        } break;
      case DECEL:
        {
          sprintf(move_state_output, "move_dir = DECEL");
        } break;
      case FALL_NORM:
        {
          sprintf(move_state_output, "move_dir = FALL_NORM");
        } break;
      case FALL_KEEP:
        {
          sprintf(move_state_output, "move_dir = FALL_KEEP");
        } break;
      case FALL_DECEL:
        {
          sprintf(move_state_output, "move_dir = FALL_DECEL");
        } break;
      case NO_MOVE:
        {
          sprintf(move_state_output, "move_dir = NO_MOVE");
        } break;
      case FALL_FREE:
        {
          sprintf(move_state_output, "move_dir = FALL_FREE");
        } break;
      default:
        break;
    }
    gl_render_text(&renderer,
                   move_state_output,
                   Vec2{900.0f, 60.0f},      // position
                   28.0f,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color
    
    char fmt_buffer[50];
    sprintf(fmt_buffer, "player moving? %d", is_player_moving);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{900.0f, 40.0f},      // position
                   28.0f,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color
    
    sprintf(fmt_buffer, "%f pixels", pd_1.x);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{500.0f, 200.0f},       // position
                   28.0f,                      // size
                   Vec3{0.0f, 0.0f, 0.0f});    // color

    sprintf(fmt_buffer, "can_slide = %d", can_slidex);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{200.0f, 200.0f},       // position
                   28.0f,                      // size
                   Vec3{0.0f, 0.0f, 0.0f});    // color
    sprintf(fmt_buffer, "collide x: %d, y: %d", collidex, collidey);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{200.0f, 600.0f},       // position
                   28.0f,                      // size
                   Vec3{0.0f, 0.0f, 0.0f});    // color
    if (activate_gravity)
    {
      gl_render_text(&renderer,
                     "gravity=1",
                     Vec2{650.0f, 700.0f},
                     18.0f,
                     Vec3{0.2f, 0.8f, 0.0f});
    }
    SDL_GL_SwapWindow(window);
  }
  
  free(renderer.ui_text.transforms);
  free(renderer.ui_text.char_indexes);
  free(renderer.ui_text.char_map);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
