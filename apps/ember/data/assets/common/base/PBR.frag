#version 330 core
in vec3 vNormal;
in vec2 vUV;

out vec4 fragColor;

uniform sampler2D AlbedoMap;
uniform sampler2D MetallicMap;
uniform sampler2D RoughnessMap;
uniform sampler2D NormalMap;
uniform sampler2D AOMap;

void main() {
    vec3 albedo = texture(AlbedoMap, vUV).rgb;
    float metallic = texture(MetallicMap, vUV).r;
    float roughness = texture(RoughnessMap, vUV).r;
    vec3 normal = texture(NormalMap, vUV).xyz * 2.0 - 1.0;
    float ao = texture(AOMap, vUV).r;
    vec3 color = albedo * ao;
    fragColor = vec4(color, 1.0);
}
