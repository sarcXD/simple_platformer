#version 330 core
layout(location=0) in vec2 aPos;

uniform mat4 Projection;
uniform mat4 View;
uniform mat4 LetterTransforms[32];
out vec2 TexCoords;
flat out int Index;

void main() {
  gl_Position = Projection * View * LetterTransforms[gl_InstanceID] * vec4(aPos, 0.0, 1.0);
  vec2 tex = aPos;
  TexCoords = tex;
  TexCoords.y = 1.0 - TexCoords.y;
  Index = gl_InstanceID;
}
