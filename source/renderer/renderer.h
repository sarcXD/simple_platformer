#pragma once

u32 gl_shader_program(char *vs, char *fs);
u32 gl_shader_program_from_path(const char *vspath, const char *fspath);

u32 gl_setup_colored_quad(u32 sp);
void gl_draw_colored_quad(
	GLRenderer* renderer,
	Vec3 position,
	Vec2 size,
	Vec3 color
    );

void gl_setup_colored_quad_optimized(
	GLRenderer* renderer, 
	u32 sp
	);
void gl_draw_colored_quad_optimised(
	GLRenderer* renderer,
	Vec3 position,
	Vec2 size,
	Vec3 color
	);
void gl_cq_flush(GLRenderer *renderer);
