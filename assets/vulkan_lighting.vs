#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec2 aTexCoord0;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aNormal;
layout(location = 4) in vec4 aWorldPosition;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec3 vNormal;
layout(location = 3) out vec3 vWorldPos;

void main() {
    gl_Position = aPosition;
    vTexCoord = aTexCoord0;
    vColor = aColor;
    vNormal = normalize(aNormal.xyz);
    vWorldPos = aWorldPosition.xyz;
}