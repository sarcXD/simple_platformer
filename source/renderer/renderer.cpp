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
