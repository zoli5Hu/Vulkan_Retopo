#version 450

// Hardkódolt háromszög csúcspontok koordinátái (2D-ben)
vec2 positions[3] = vec2[](
vec2(0.0, -0.5),
vec2(0.5, 0.5),
vec2(-0.5, 0.5)
);

// Hardkódolt színek a csúcspontokhoz (RGB)
vec3 colors[3] = vec3[](
vec3(1.0, 0.0, 0.0), // Piros
vec3(0.0, 1.0, 0.0), // Zöld
vec3(0.0, 0.0, 1.0)  // Kék
);

// Ezt a színt küldjük tovább a Fragment Shadernek
layout(location = 0) out vec3 fragColor;

void main() {
    // A gl_VertexIndex a jelenleg feldolgozott csúcspont sorszáma (0, 1 vagy 2)
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}