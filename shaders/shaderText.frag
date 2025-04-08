#version 450 core

// Input
layout (location = 0) in vec2 textureCoords;

layout (location = 1) in vec3 TextColor;

// Output
layout (location = 0) out vec4 FragmentColor;

// Uniforms
layout (set=0, binding=0) uniform sampler2D textureSampler;

void main() {
  FragmentColor = vec4(TextColor, texture(textureSampler, textureCoords).r);
}
