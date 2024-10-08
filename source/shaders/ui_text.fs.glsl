#version 330 core

in vec2 TexCoords;
flat in int Index;
uniform sampler2DArray TextureAtlas;
uniform int TextureMap[32];
uniform vec3 TextColor;
out vec4 FragColor;

void main() {
  int TextureId = TextureMap[Index];
  vec3 TextureIndexCoords = vec3(TexCoords.xy, TextureId);
  vec4 sampled = vec4(1.0, 1.0, 1.0, texture(TextureAtlas, TextureIndexCoords).r);
  FragColor = sampled * vec4(TextColor, 1);
};
