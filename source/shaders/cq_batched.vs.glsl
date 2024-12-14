#version 330 core
layout(location=0) in vec4 aPos;
layout(location=1) in vec3 aColor;

out vec4 vertexColor;
uniform mat4 View;
uniform mat4 Projection;

void main() {
  gl_Position = Projection * View * aPos;
  vertexColor = vec4(aColor, 1.0);
}
