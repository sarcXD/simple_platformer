#pragma once

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "../core.h"
#include "../math.h"
#include "../array/array.h"

#define BATCH_SIZE 2000

struct TextChar {
  s64 lsb;
  s64 advance;
  Vec2 bbox0;
  Vec2 bbox1;
  Vec2 size;
};

struct TextState {
  r32 scale;
  u32 pixel_size;
  s32 ascent;
  s32 descent;
  s32 linegap;
  u32 texture_atlas_id;
  u32 sp;
  u32 vao;
  u32 vbo;
  u32 chunk_size;
  IVec2 bbox0;
  IVec2 bbox1;
  stbtt_fontinfo font;
  s32* char_indexes;
  Mat4* transforms;
  TextChar* char_map;
};

struct GlQuad {
    u32 sp;
    u32 vao;
};

struct CameraOrtho {
    b8 update;
    Vec3 up;
    Vec3 pos;
    Vec3 look;
    Mat4 view;
    Mat4 proj;
};

struct GLRenderer {
  b8  cq_init;
  GlQuad quad;
  // ui camera
  CameraOrtho ui_cam;

  // camera
  Vec3 preset_up_dir;
  Mat4 cam_proj;
  //b8 ui_cam_update;
  //Vec3 ui_cam_pos;
  //Vec3 ui_cam_look;
  //Mat4 ui_cam_view;
  // game camera
  CameraOrtho game_cam;
  b8   cam_update;
  Vec3 cam_pos;
  Vec3 cam_look;
  Mat4 cam_view;
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

u32 gl_shader_program(char *vs, char *fs);
u32 gl_shader_program_from_path(const char *vspath, const char *fspath);

// ==================== QUADS ====================
u32 gl_setup_quad(u32 sp);
void gl_draw_quad(GlQuad quad,
		  CameraOrtho camera,
		  Vec3 position,
		  Vec2 size,
		  Vec3 color);

// batched renderer
void gl_setup_colored_quad_optimized(
	GLRenderer* renderer, 
	u32 sp);
void gl_draw_colored_quad_optimized(
	GLRenderer* renderer,
	Vec3 position,
	Vec2 size,
	Vec3 color);

void gl_cq_flush(GLRenderer *renderer);

// ==================== LINE ====================
void gl_setup_line(GLRenderer *renderer, u32 sp);
void gl_draw_line(
	GLRenderer *renderer,
	Vec3 start,
	Vec3 end,
	Vec3 color
	);
void gl_line_flush(GLRenderer *renderer);

// ==================== FONT RENDERING ====================
void gl_setup_text(TextState *uistate);
void gl_render_text(GLRenderer *renderer, 
		    char *text, 
		    Vec3 position, 
		    Vec3 color, 
		    r32 font_size);
void gl_text_flush(GLRenderer *renderer, u32 render_count);

