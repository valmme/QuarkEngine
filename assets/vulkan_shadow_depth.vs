#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 3) in vec4 aNormal;

layout(location = 0) out vec3 vNormal;

void main() {
    gl_Position = aPosition;
    vNormal = normalize(aNormal.xyz);
}