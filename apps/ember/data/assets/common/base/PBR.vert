#version 330 core
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;

out vec3 vNormal;
out vec2 vUV;

uniform mat4 worldViewProj;

void main() {
    vNormal = vertexNormal;
    vUV = vertexUV;
    gl_Position = worldViewProj * vec4(vertexPosition, 1.0);
}
