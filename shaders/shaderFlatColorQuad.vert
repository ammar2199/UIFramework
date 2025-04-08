#version 450 core

// TODO: Use Vertex Buffers.

// Input

// Output
layout (location = 0) out vec4 OutColor;
layout (location = 1) out vec2 textureCoordinates;

// Push constants use std430 layout
// Padding?
layout(push_constant) uniform fb {
  int fbCoords[4]; // left, top, right, bottom
                    // Each element is 4 bytes
  uvec2 frameBufferSize; // 8 bytes
  float rgba[4]; // 12 bytes // XXX: Apparently vec3 doesn't work too well with std140, std340 
                //                  layout.
  float textureCoords[8];
};

void main() {
  float fbCoordX = 0;
  float fbCoordY = 0;
  if (gl_VertexIndex == 0) { // Top Left Vertex
    fbCoordX = fbCoords[0];
    fbCoordY =  fbCoords[1];
  } else if (gl_VertexIndex == 1) { // Top Right Vertex
    fbCoordX = fbCoords[2];
    fbCoordY = fbCoords[1];
  } else if (gl_VertexIndex == 2) { // Bottom Right Vertex
    fbCoordX = fbCoords[2];
    fbCoordY = fbCoords[3];
  } else if (gl_VertexIndex == 3) { // Bottom Right Vertex
    fbCoordX = fbCoords[2];
    fbCoordY = fbCoords[3];
  } else if (gl_VertexIndex == 4) { // Bottom Left Vertex
    fbCoordX = fbCoords[0];
    fbCoordY = fbCoords[3];
  } else if (gl_VertexIndex == 5) { // Top Left Vertex
    fbCoordX = fbCoords[0];
    fbCoordY = fbCoords[1];
  }

  float clipCoordX =  2 * (fbCoordX / frameBufferSize.x) - 1;
  float clipCoordY = 2 * (fbCoordY / frameBufferSize.y) - 1;
  gl_Position = vec4(clipCoordX, clipCoordY, 0.0, 1.0);
  OutColor = vec4(rgba[0], rgba[1], rgba[2], rgba[3]);
}
