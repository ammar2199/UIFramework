#version 450 core

// Input
layout (location = 0) in vec2 textureCoords;

// Output
layout (location = 0) out vec4 FragmentColor;

// Uniforms
layout (set=0, binding=0) uniform sampler2D textureSampler;

void main() {
  FragmentColor = texture(textureSampler, textureCoords);
}
