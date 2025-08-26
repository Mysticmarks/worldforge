#version 330 core
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec3 vertexTangent;

out vec2 vUV;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec3 vViewDir;

uniform mat4 worldViewProj;
uniform mat4 world;
uniform vec3 cameraPos;

void main() {
    vec3 worldPos = (world * vec4(vertexPosition, 1.0)).xyz;

    mat3 worldMat3 = mat3(world);
    vec3 N = normalize(worldMat3 * vertexNormal);
    vec3 T = normalize(worldMat3 * vertexTangent);
    vec3 B = normalize(cross(N, T));

    vUV = vertexUV;
    vNormal = N;
    vTangent = T;
    vBitangent = B;
    vViewDir = normalize(cameraPos - worldPos);

    gl_Position = worldViewProj * vec4(vertexPosition, 1.0);
}
