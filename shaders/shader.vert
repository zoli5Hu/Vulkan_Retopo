#version 450

// Bemenő adatok a C++ kódból (A Vertex struktúrából!)
// A "location" értékek pontosan megegyeznek a Vertex::getAttributeDescriptions()-ben megadottakkal.
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

// Kimenő adat a Fragment Shader felé
layout(location = 0) out vec3 fragColor;

void main() {
    // A gl_Position a Vulkan beépített változója a végső koordinátának
    gl_Position = vec4(inPosition, 0.0, 1.0);

    // A bejövő színt egyszerűen továbbadjuk a pixel-színezőnek
    fragColor = inColor;
}