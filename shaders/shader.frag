#version 450

// Ezt az adatot kaptuk a Vertex Shadertől
layout(location = 0) in vec3 fragColor;

// Ez lesz a végleges pixel színe a képernyőn (RGBA)
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0); // 1.0 = nem átlátszó
}