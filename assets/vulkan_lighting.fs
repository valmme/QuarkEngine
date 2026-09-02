#version 450

struct Light {
    vec3 position;
    vec3 target;
    vec4 color;
    float attenuation;
    int enabled;
    int type;
};

layout(set = 0, binding = 1) uniform sampler2D albedo;
layout(set = 0, binding = 2) uniform sampler2D shadowMaps[4];
layout(set = 0, binding = 3) uniform ShadowMatrices {
    mat4 lightViews[4];
    mat4 lightProjections[4];
};
layout(set = 0, binding = 4) uniform LightsBlock {
    vec4 ambient;
    vec4 colDiffuse;
    vec4 viewPos;
    Light lights[4];
};

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec3 vWorldPos;
layout(location = 0) out vec4 outColor;

float shadow_factor(int lightIndex, vec3 worldPosition) {
    vec4 lightSpacePosition = lightProjections[lightIndex] * lightViews[lightIndex]
        * vec4(worldPosition, 1.0);
    vec3 projected = lightSpacePosition.xyz / max(lightSpacePosition.w, 0.0001);
    projected.xy = projected.xy * 0.5 + 0.5;

    if (projected.z > 1.0 || projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0)
        return 0.0;

    float currentDepth = projected.z;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(shadowMaps[lightIndex],
                projected.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - 0.004 > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    vec4 texelColor = texture(albedo, vTexCoord) * vColor;
    vec3 normal = normalize(vNormal);
    vec3 viewDirection = normalize(viewPos.xyz - vWorldPos);
    vec3 lightAccumulation = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        if (lights[i].enabled != 1) continue;

        vec3 lightDirection;
        float attenuation = 1.0;
        if (lights[i].type == 0) {
            lightDirection = -normalize(lights[i].target - lights[i].position);
        } else {
            vec3 toLight = lights[i].position - vWorldPos;
            float distanceToLight = length(toLight);
            lightDirection = normalize(toLight);
            attenuation = 1.0 / (1.0 + lights[i].attenuation * distanceToLight * distanceToLight);
        }

        float diffuse = max(dot(normal, lightDirection), 0.0);
        float shadow = shadow_factor(i, vWorldPos);
        lightAccumulation += lights[i].color.rgb * diffuse * attenuation * (1.0 - shadow);

        if (diffuse > 0.0) {
            float highlight = pow(max(dot(viewDirection,
                reflect(-lightDirection, normal)), 0.0), 16.0);
            specular += highlight * attenuation * (1.0 - shadow);
        }
    }

    vec3 color = texelColor.rgb * colDiffuse.rgb * lightAccumulation;
    color += texelColor.rgb * ambient.rgb * colDiffuse.rgb / 10.0;
    color += specular;
    outColor = vec4(pow(color, vec3(1.0 / 2.2)), texelColor.a * colDiffuse.a);
}