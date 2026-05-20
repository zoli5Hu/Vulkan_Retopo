#version 450

// Az új mátrix-csomag (Uniform Buffer), amit a C++ kódból fogunk küldeni
layout(binding = 0) uniform UniformBufferObject {
    mat4 model; // Az objektum helyzete/forgása a világban
    mat4 view;  // A kamera (szem) helyzete
    mat4 proj;  // A perspektíva (lencse torzítása, látószög)
} ubo;

layout(location = 0) in vec3 inPosition; // Ezt adtuk meg az előbb (3D)
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor; //

void main() {
    // Összeszorozzuk a mátrixokat a pozícióval (Jobbról balra olvasandó!)
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor; //
}