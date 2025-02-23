#include "renderer.h"

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

  SDL_free(vs);
  SDL_free(fs);
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
  // @note: this is necessary because clip space is {-1,-1} to {1,1}
  // So, by scaling with the original size we get scaling range of {-32, -32}, {32, 32}
  // doubling our size.
  Mat4 scale = scaling_matrix4m(size.x/2.0f, size.y/2.0f, 0.0f);
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
    (void*)renderer->cq_pos_batch.buffer
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

void gl_setup_line(GLRenderer* renderer, u32 sp) {
    glGenVertexArrays(1, &renderer->line_vao);
    glGenBuffers(1, &renderer->line_vbo);

    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(
	    GL_ARRAY_BUFFER, (
		renderer->line_pos_batch.capacity + 
		renderer->line_color_batch.capacity
		) * sizeof(r32), NULL, GL_DYNAMIC_DRAW
	    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(r32), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
	    1, 3, GL_FLOAT, GL_FALSE, 
	    3 * sizeof(r32), (void*)(
		renderer->line_pos_batch.capacity*sizeof(r32)
		)
	    );

    glBindVertexArray(0);
}

void gl_draw_line(
	GLRenderer *renderer,
	Vec3 start,
	Vec3 end,
	Vec3 color
	) {

    Vec4 vertices[2] = {
	Vec4{start.x, start.y, start.z, 1.0f},
	Vec4{end.x, end.y, end.z, 1.0f}
    };

    array_insert(&renderer->line_pos_batch, vertices[0].data, 4);
    array_insert(&renderer->line_pos_batch, vertices[1].data, 4);
    array_insert(&renderer->line_color_batch, color.data, 3);
    array_insert(&renderer->line_color_batch, color.data, 3);

    renderer->line_batch_count++;
    if(renderer->line_batch_count == BATCH_SIZE) {
	gl_line_flush(renderer);
    }
}

void gl_line_flush(GLRenderer *renderer) {
    glUseProgram(renderer->line_sp);
    glEnable(GL_DEPTH_TEST);

    glUniformMatrix4fv(
	    glGetUniformLocation(renderer->line_sp, "View"),
	    1, GL_FALSE, (renderer->cam_view).buffer
	    );
    glUniformMatrix4fv(
	    glGetUniformLocation(renderer->line_sp, "Projection"),
	    1, GL_FALSE, (renderer->cam_proj).buffer
	    );
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);

    // fill batch data
    // position batch
    glBufferSubData(
	    GL_ARRAY_BUFFER,
	    0,
	    renderer->line_pos_batch.capacity*sizeof(r32),
	    (void*)renderer->line_pos_batch.buffer
	    );
    glBufferSubData(
	    GL_ARRAY_BUFFER,
	    renderer->line_pos_batch.capacity*sizeof(r32),
	    renderer->line_color_batch.capacity*sizeof(r32),
	    (void*)renderer->line_color_batch.buffer
	    );

    glBindVertexArray(renderer->line_vao);
    glDrawArrays(GL_LINES, 0, renderer->line_batch_count*2);

    array_clear(&renderer->line_pos_batch);
    array_clear(&renderer->line_color_batch);
    renderer->line_batch_count = 0;
}

void gl_setup_text(TextState *uistate) {
    uistate->scale = stbtt_ScaleForPixelHeight(&uistate->font, uistate->pixel_size);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &(uistate->texture_atlas_id));
    glBindTexture(GL_TEXTURE_2D_ARRAY, uistate->texture_atlas_id);

    // generate texture
    glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_R8,
            uistate->pixel_size,
            uistate->pixel_size,
            128,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            0);

    // font vmetrics
    s32 ascent, descent, linegap = 0;
    stbtt_GetFontVMetrics(&uistate->font, &ascent, &descent, &linegap);
    uistate->ascent = ascent;
    uistate->descent = descent;
    uistate->linegap = linegap;

    // font bounding box
    s32 x0, y0, x1, y1 = 0;
    stbtt_GetFontBoundingBox(&uistate->font, &x0, &y0, &x1, &y1);
    uistate->bbox0 = IVec2{x0, y0};
    uistate->bbox1 = IVec2{x1, y1};

    // generate bitmaps
    u32 pixel_size = uistate->pixel_size;
    unsigned char *bitmap_buffer = (unsigned char*)calloc(pixel_size * pixel_size, sizeof(unsigned char));
    for (u32 c = 0; c < 128; c++)
    {
        s32 advance, lsb = 0;
        stbtt_GetCodepointHMetrics(&uistate->font, c, &advance, &lsb);
	s32 bx0, bx1, by0, by1 = 0;
	stbtt_GetCodepointBitmapBox(
		&uistate->font, c,
		uistate->scale, uistate->scale,
		&bx0, &by0,
		&bx1, &by1
		);

	s32 width = bx1 - bx0;
	s32 height = by1 - by0;

        stbtt_MakeCodepointBitmap(
                &uistate->font, 
		bitmap_buffer,
		width,
		height,
		width,
                uistate->scale, 
                uistate->scale,
                c);

        glTexSubImage3D(
		GL_TEXTURE_2D_ARRAY,
		0,
		0, 0, // x, y offset
		int(c),
		width,
          	height,
          	1,
          	GL_RED,
          	GL_UNSIGNED_BYTE,
          	bitmap_buffer
		);
        // set texture options
        glTexParameteri(
		GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE
	);
        glTexParameteri(
                GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE
                );
        glTexParameteri(
                GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR
                );
        glTexParameteri(
                GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR
                );

        TextChar tc;
        tc.size = Vec2{
            (r32)width, (r32)height
        };
	tc.bbox0 = Vec2{
	    (r32)bx0, (r32)by0
	};
	tc.bbox1 = Vec2{
	    (r32)bx1, (r32)by1
	};
        tc.advance = advance;
	tc.lsb = (s32)lsb;
        uistate->char_map[c] = tc;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    r32 vertices[] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };

    glGenVertexArrays(1, &(uistate->vao));
    glGenBuffers(1, &(uistate->vbo));

    glBindVertexArray(uistate->vao);
    glBindBuffer(GL_ARRAY_BUFFER, uistate->vbo);
    glBufferData(
            GL_ARRAY_BUFFER, 
	    sizeof(vertices), 
	    vertices, 
	    GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void gl_render_text(
	GLRenderer *renderer, 
	char *text,
	Vec2 position, 
	Vec3 color, 
	r32 font_size) {
    // render_text
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(renderer->ui_text.sp);
    if (renderer->ui_cam_update) {

	glUniformMatrix4fv(
		glGetUniformLocation(
		    renderer->ui_text.sp, "View"),
		1, GL_FALSE, renderer->ui_cam_view.buffer);

	glUniformMatrix4fv(
		glGetUniformLocation(
		    renderer->ui_text.sp, "Projection"),
		1, GL_FALSE, renderer->cam_proj.buffer);

	renderer->ui_cam_update = 0;
    }
    glUniform3fv(
	    glGetUniformLocation(
		renderer->ui_text.sp, "TextColor"),
	    1, color.data);
    glBindVertexArray(renderer->ui_text.vao);
    glBindTexture(
	    GL_TEXTURE_2D_ARRAY, 
	    renderer->ui_text.texture_atlas_id);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_text.vbo);
    glActiveTexture(GL_TEXTURE0);

    u32 running_index = 0;
    r32 startx = position.x;
    r32 starty = position.y;
    r32 linex = startx;
    r32 liney = starty;
    r32 render_scale = font_size/(r32)renderer->ui_text.pixel_size;
    r32 font_scale = renderer->ui_text.scale*render_scale;
    memset(renderer->ui_text.transforms, 0, renderer->ui_text.chunk_size);
    memset(renderer->ui_text.char_indexes, 0, renderer->ui_text.chunk_size);

    char *char_iter = text;
    r32 baseline = -renderer->ui_text.bbox0.y*font_scale - font_size;
    while (*char_iter != '\0') {
	TextChar render_char = renderer->ui_text.char_map[*char_iter];
	if (*char_iter == ' ') {
	    linex += (font_scale * render_char.advance);
	    char_iter++;
	    continue;
	}
	if (*char_iter == '\t') {
	    linex += (font_scale * render_char.advance);
	    char_iter++;
	    continue;
	}
	if (*char_iter == '\n') {
	    linex = startx;
	    liney = liney - font_scale * (renderer->ui_text.ascent - renderer->ui_text.descent + renderer->ui_text.linegap);
	    char_iter++;
	    continue;
	}
	r32 xpos = linex + (font_scale * render_char.lsb);
	r32 ypos = liney + (baseline - render_scale*render_char.bbox0.y);

	Mat4 sc = scaling_matrix4m(font_size, font_size, 1.0f);
	Mat4 tr = translation_matrix4m(xpos, ypos, 0);
	Mat4 model = multiply4m(tr, sc);
	renderer->ui_text.transforms[running_index] = model;
	renderer->ui_text.char_indexes[running_index] = 
	    int(*char_iter);

	linex += (font_scale * render_char.advance);
	char prev_char = *char_iter;
	char_iter++;
	char curr_char = *char_iter;

	if (curr_char) {
	    s32 kern = font_scale * stbtt_GetCodepointKernAdvance(&renderer->ui_text.font, prev_char, curr_char);
	    linex += kern;
	}
	running_index++;
	if (running_index >= renderer->ui_text.chunk_size) {
	    gl_text_flush(renderer, running_index);
	    running_index = 0;
	}
    }
    gl_text_flush(renderer, running_index);
}

void gl_text_flush(GLRenderer *renderer, u32 render_count) {
    s32 transform_loc = glGetUniformLocation(
	    renderer->ui_text.sp, "LetterTransforms");
    glUniformMatrix4fv(
	    transform_loc, 
	    render_count,
	    GL_FALSE, 
	    &(renderer->ui_text.transforms[0].buffer[0])
	    );

    s32 texture_map_loc = glGetUniformLocation(
	    renderer->ui_text.sp, "TextureMap");
    glUniform1iv(
	    texture_map_loc, 
	    render_count, 
	    renderer->ui_text.char_indexes);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, render_count);

    memset(renderer->ui_text.transforms, 0, render_count);
    memset(renderer->ui_text.char_indexes, 0, render_count);
}
