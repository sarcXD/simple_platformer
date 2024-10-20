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
* TODO:
* - Refactor state
* - Some way to make and define levels
* - Level completion Object
* - Implement Broad Phase Collision for efficient collision handling 
* - Efficient Quad Renderer
* - Audio
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
  b8 toggle_gravity;
};

struct Rect {
  Vec2 tl;
  Vec2 br;
  Vec2 size;
  Vec3 position;
};

class FrameTimerV2 {
public:
  FrameTimerV2();
  void update();
  void enforceFramerate(u32 framerate);
public:
  r64 m_tCurr;
  r64 m_tPrev;
  r64 m_tDeltaMS;
  r64 m_tDelta;
};

FrameTimerV2::FrameTimerV2() {
  m_tCurr = SDL_GetTicks64();
  m_tPrev = m_tCurr;
  m_tDeltaMS = m_tCurr - m_tPrev;
  m_tDelta = m_tDeltaMS / 1000.0f;
}

void FrameTimerV2::update() {
  m_tPrev = m_tCurr;
  m_tCurr = SDL_GetTicks64();
  m_tDeltaMS = m_tCurr - m_tPrev;
  m_tDelta = m_tDeltaMS / 1000.0f;
}

void FrameTimerV2::enforceFramerate(u32 target) {
  r64 target_frametime = SDL_floor(
    1000.0f/(r64)target
  );
  while(m_tDeltaMS < target_frametime) {
    m_tCurr = SDL_GetTicks64();
    m_tDeltaMS = m_tCurr - m_tPrev;
    m_tDelta = m_tDeltaMS / 1000.0f;

    // pass time
    continue;
  }
}

struct FrameTimer {
  r64 tCurr;
  r64 tPrev;
  r64 tDeltaMS;
  r64 tDelta;
};

FrameTimer frametimer() {
  FrameTimer res = {};
  res.tCurr = SDL_GetTicks64();
  res.tPrev = res.tCurr;
  res.tDeltaMS = res.tCurr - res.tPrev;
  res.tDelta = res.tDeltaMS / 1000.0f;

  return res;
}

void update_frame_timer(FrameTimer *ft) {
  ft->tCurr = SDL_GetTicks64();
  ft->tPrev = ft->tCurr;
  ft->tDeltaMS = ft->tCurr - ft->tPrev;
  ft->tDelta = ft->tDeltaMS / 1000.0f;
}

void enforce_frame_rate(FrameTimer *ft, u32 target) {
  r64 target_frametime = SDL_floor(
    1000.0f/(r64)target
  );
  while(ft->tDeltaMS < target_frametime) {
    ft->tCurr = SDL_GetTicks64();
    ft->tDeltaMS = ft->tCurr - ft->tPrev;
    ft->tDelta = ft->tDeltaMS / 1000.0f;

    // pass time
    continue;
  }
}

struct GameState {
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

void gl_draw_colored_quad(
  GLRenderer* renderer,
  Vec3 position,
  Vec2 size,
  Vec3 color
) {
  //glEnable(GL_DEPTH_TEST);
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
  r32 render_scale = 2.0f;
  r32 movement_to_render_ratio = 1.5f;
  
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
  renderer.cam_update = 1;
  renderer.cam_pos = Vec3{0.0f, 0.0f, 1.0f};
  renderer.cam_look = camera_look_around(TO_RAD(0.0f), -TO_RAD(90.0f));
  renderer.cam_view = camera_create4m(
    renderer.cam_pos, 
    add3v(renderer.cam_pos, renderer.cam_look), preset_up_dir
  );
  renderer.cam_proj = orthographic_projection4m(
    0.0f, (r32)scr_width*render_scale, 
    0.0f, (r32)scr_height*render_scale, 
    0.1f, 10.0f
  );

  // @section: gameplay variables
  r32 fall_accel = 3.0f*movement_to_render_ratio;
  r32 fall_smooth_decel = -2.0f*movement_to_render_ratio;
  r32 move_accel = 6.0f*movement_to_render_ratio;
  r32 freefall_accel = -11.8f*movement_to_render_ratio;
  r32 jump_force = 6.5f*movement_to_render_ratio;
  r32 effective_force = 0.0f;

  GameState state = {0};
  Vec3 player_position = Vec3{0.0f, 70.0f, -1.0f};
  Vec2 player_size = Vec2{40.0f, 40.0f};
  state.player = rect(player_position, player_size);

  Vec3 floor_position = Vec3{600.0f, 800.0f, -2.0f};
  Vec2 floor_size = Vec2{400.0f, 20.0f};
  state.floor = rect(floor_position, floor_size);

  Vec3 wall_position = Vec3{170.0f, 100.0f, -2.0f};
  Vec2 wall_size = Vec2{20.0f, 80.0f};
  state.wall = rect(wall_position, wall_size);

  Controller controller = {0};
  r32 key_down_time[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  b8 is_key_down_x = false;
  
  // gravity calculations
  b8 collidex = 0;
  b8 collidey = 0;
  b8 is_gravity = 0;
  Vec2 player_move_t = Vec2{0.0f, 0.0f};
  Vec2 player_decel_t = Vec2{0.0f, 0.0f};
  
  // player force variables
  r32 gravity = -9.8f;
  r32 player_mass = 1.0f;
  Vec2 player_acceleration = Vec2{0.0f, 0.0f};
  Vec2 player_velocity = Vec2{0.0f, 0.0f};
  Vec2 position_displacement = Vec2{0.0f, 0.0f};

  PMoveState p_move_state = NO_MOVE;
  Vec2 p_move_dir = Vec2{0.0f, 0.0f};
  Vec2 p_force = Vec2{0.0f, 0.0f};

  b8 cndbrk = 0; // a conditional debug trick in code
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
    }
    if (controller.jump) {
      p_force = Vec2{2.0f, 2.0f};
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

    b8 was_moving_on_ground = collidey;
    if (is_gravity) {
      if (!collidey) {
        // fall states
        if (is_key_down_x) {
            p_move_state = FALL_MOVE;
        }       
      } else {
        // standard motion states
        if (is_key_down_x) {
          p_move_state = MOVE;
        }      
      }
    }
    
    // @section: gravity
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
    if (is_gravity)
    {
      // calculate force acting on player
      if (was_moving_on_ground) {
        // @note: can I reduce the states here like I did in the falling case
        // without separate checks
        if (is_key_down_x) {
          r32 updated_force = (
            effective_force + p_move_dir.x*move_accel*timer.tDelta
          );
          updated_force = clampf(
            updated_force, -move_accel, move_accel
          );
          if (controller.jump) {
          }
          effective_force = updated_force;
        } else {
          r32 force_reducer = 0.0f;
          if (effective_force > 0.0f) {
            force_reducer = -move_accel*timer.tDelta;
          } else if (effective_force < 0.0f) {
            force_reducer = move_accel*timer.tDelta;
          }
          r32 updated_force = effective_force + force_reducer;
          effective_force = (
            ABS(updated_force) < 0.5f ? 
            0.0f : updated_force
          );
        }
      } else {
          r32 smoothing_force = effective_force;
          r32 net_force = effective_force;
          r32 active_force = 0.0f;

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
              friction = -fall_accel*timer.tDelta;
            } else if (is_force_neg) {
              friction = fall_accel*timer.tDelta;
            }
            net_force += friction;
          } 

          if (!collidex) {
            active_force = p_move_dir.x*fall_accel;
            r32 interm_total_force = net_force + active_force;
            if (ABS(interm_total_force) > fall_accel) {
              r32 deficit = MIN(fall_accel - ABS(net_force), 0.0f);
              active_force = p_move_dir.x*deficit;
            }
            net_force += active_force;
          }
          effective_force = net_force;
      }
      
      if (!collidex) {
        r32 dx1 = player_velocity.x;
        switch (p_move_state) {
          case MOVE:
          case FALL_MOVE:
            {
              dx1 = effective_force;
            } break;
          default:
            {
            } break;
        }
        // checks for motion on ground
        if ( dx1 == 0.0f ) {
          p_move_dir.x = 0.0f;
          p_move_state = NO_MOVE;
        }

        accel_computed = (dx1 - player_velocity.x)/timer.tDelta;
        player_velocity.x = dx1;
        pd_1.x = dx1;
      } else {
        p_move_dir.x = 0.0f;
        p_move_state = NO_MOVE;
        player_velocity.x = 0.0f;
        pd_1.x = 0.0f;
      }

      {
        // vertical motion when falling
        r32 dy1 = player_velocity.y;
        dy1 = dy1 + freefall_accel*timer.tDelta;
        if (controller.jump) {
          dy1 = jump_force;
        }
        player_velocity.y = dy1;
        pd_1.y = dy1;
      }
    }
    else 
    {
        Vec2 dir = get_move_dir(controller);
        pd_1 = mul2vf(dir, 8.0f);
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
      state.player.position.x = next_player_position.x;

    }
    if (!is_collide_y) {
      state.player.position.y = next_player_position.y;
    }

    state.player = rect(state.player.position, state.player.size);
    collidex = is_collide_x;
    collidey = is_collide_y;
    
    // output
    glClearColor(0.8f, 0.5f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // player
    gl_draw_colored_quad(&renderer, 
                         state.player.position,            // position
                         state.player.size,         // size
                         Vec3{0.45f, 0.8f, 0.2f});
    // floor
    gl_draw_colored_quad(&renderer,
                         state.floor.position,
                         state.floor.size,
                         Vec3{1.0f, 1.0f, 1.0f});
    // wall
    gl_draw_colored_quad(&renderer, 
                         state.wall.position,
                         state.wall.size,
                         Vec3{1.0f, 0.0f, 0.0f});

    // render ui text
    
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
      case NO_MOVE:
        {
          sprintf(move_state_output, "move_dir = NO_MOVE");
        } break;
      case MOVE:
        {
          sprintf(move_state_output, "move_dir = MOVE");
        } break;
      case FALL_MOVE:
        {
          sprintf(move_state_output, "move_dir = FALL_MOVE");
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
    sprintf(fmt_buffer, "player moving? %d", is_key_down_x);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{900.0f, 40.0f},      // position
                   28.0f,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color

    sprintf(fmt_buffer, "frametime: %f", timer.tDeltaMS);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{900.0f, 90.0f},      // position
                   28.0f,                     // size
                   Vec3{0.0f, 0.0f, 0.0f});   // color
    
    sprintf(fmt_buffer, "%f pixels", pd_1.x);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{500.0f, 200.0f},       // position
                   28.0f,                      // size
                   Vec3{0.0f, 0.0f, 0.0f});    // color

    sprintf(fmt_buffer, "collide: x(%d),y(%d)", collidex, collidey);
    gl_render_text(&renderer,
                   fmt_buffer,
                   Vec2{500.0f, 1000.0f},       // position
                   28.0f,                      // size
                   Vec3{0.0f, 0.0f, 0.0f});    // color
    if (is_gravity)
    {
      gl_render_text(
        &renderer,
        "gravity=1",
        Vec2{650.0f, 700.0f},
        18.0f,
        Vec3{0.2f, 0.8f, 0.0f}
      );
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
