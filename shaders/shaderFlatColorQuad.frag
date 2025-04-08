#version 450 core

// Input
layout (location = 0) in vec4 InColor;

// Output
layout (location = 0) out vec4 FragmentColor;

void main() {
  FragmentColor = InColor;
}
