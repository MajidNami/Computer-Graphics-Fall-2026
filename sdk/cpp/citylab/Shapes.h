#pragma once

// Small helpers that emit classic OpenGL geometry (glBegin/glNormal/glTexCoord/
// glVertex) for common shapes. They are ordinary code you could have written
// yourself; read Shapes.cpp to see exactly what each one sends to OpenGL.
//
// Conventions:
//   * box, sphere, torus and plane are centred on the origin;
//   * cylinder, cone and gableRoof stand on y = 0 and grow towards +y;
//   * every face has an outward unit normal and texture coordinates.
//   * uvPerMeter = 0 maps each face to 0..1; a positive value repeats a
//     texture every 1/uvPerMeter metres (use with GL_REPEAT for bricks, tiles).
namespace citylab::shapes {

void box(float width, float height, float depth, float uvPerMeter = 0.0f);
void plane(float width, float depth, float uvPerMeter = 0.0f);  // horizontal, facing +y
void disk(float radius, int slices = 32);                       // horizontal, facing +y
void cylinder(float radius, float height, int slices = 24, bool capped = true);
void cone(float radius, float height, int slices = 24, bool capped = true);
void sphere(float radius, int slices = 24, int stacks = 16);
void torus(float ringRadius, float tubeRadius, int rings = 32, int sides = 12);  // in the XZ plane
void gableRoof(float width, float height, float depth);  // ridge along z

}  // namespace citylab::shapes
