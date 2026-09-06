#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragTangent;
uniform sampler2D texture0;
uniform sampler2D normalMap;
uniform sampler2D shadowMaps[4];
uniform mat4 lightViews[4];
uniform mat4 lightProjections[4];
uniform vec4 colDiffuse;
uniform int useTexture;

out vec4 finalColor;

#define MAX_LIGHTS        4
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_AREA        3

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
    float intensity;
    float range;
    float spotAngle;
};

uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;
uniform vec3 emissionColor;
uniform float emissionPower;
uniform int shadowsEnabled;
uniform float shadowBias;
uniform int shadowFilterQuality;

float shadow_factor(int lightIndex, vec3 worldPosition)
{
    vec4 lightSpacePosition = lightProjections[lightIndex] * lightViews[lightIndex]
        * vec4(worldPosition, 1.0);
    if (lightSpacePosition.w <= 0.0) return 0.0;

    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;

    if (projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0 || projected.z > 1.0)
        return 0.0;

    float currentDepth = projected.z;
    float bias = shadowBias;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
    float shadow = 0.0;

    int radius = shadowFilterQuality == 0 ? 0 : shadowFilterQuality == 1 ? 1 : 2;
    int samples = 0;
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            if (abs(x) > radius || abs(y) > radius) continue;
            float closestDepth = texture(shadowMaps[lightIndex],
                projected.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
            samples++;
        }
    }

    return samples > 0 ? shadow / float(samples) : 0.0;
}

void main()
{
    vec4 texelColor = vec4(1.0);

    if (useTexture == 1)
        texelColor = texture(texture0, fragTexCoord);

    vec4 tint       = colDiffuse * fragColor;
    vec3 normal     = normalize(fragNormal);
    vec3 tangent    = normalize(fragTangent);
    if (abs(dot(normal, tangent)) < 0.999f)
    {
        vec3 bitangent = normalize(cross(normal, tangent));
        vec3 mapNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        normal = normalize(mat3(tangent, bitangent, normal) * mapNormal);
    }
    vec3 viewD      = normalize(viewPos - fragPosition);
    vec3 lightAccum = vec3(0.0);
    vec3 specular   = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled != 1) continue;

        vec3 lightDir = vec3(0.0);
        float attenuation = 1.0;
        float lightEnergy = min(lights[i].intensity, 4.0);

        if (lights[i].type == LIGHT_DIRECTIONAL)
        {
            lightDir = normalize(lights[i].position - lights[i].target);
        }

        else if (lights[i].type == LIGHT_POINT)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);
            lightDir = toLight / dist;

            float range = max(lights[i].range, 0.001);
            attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation *= attenuation;
        }

        else if (lights[i].type == LIGHT_SPOT)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);

            lightDir = toLight / dist;
            float range = max(lights[i].range, 0.001);
            attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation *= attenuation;

            vec3 spotDir = normalize(lights[i].position - lights[i].target);
            float theta = dot(lightDir, spotDir);
            float cutoff = cos(radians(lights[i].spotAngle));

            if (theta < cutoff)
                attenuation = 0.0;
            else
                attenuation *= smoothstep(cutoff, cutoff + 0.05, theta);
        }

        else if (lights[i].type == LIGHT_AREA)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);

            lightDir = toLight / dist;

            float range = max(lights[i].range, 0.001);
            attenuation = sqrt(clamp(1.0 - dist / range, 0.0, 1.0));
        }

        float directLight = max(dot(normal, lightDir), 0.0);
        float shadow = shadowsEnabled == 1 ? shadow_factor(i, fragPosition) : 0.0;
        float wrappedLight = clamp((dot(normal, lightDir) + 0.28) / 1.28, 0.0, 1.0);
        float diffuse = max(directLight, wrappedLight * 0.25);

        vec3 bounce = lights[i].color.rgb * attenuation * lightEnergy * 0.08;
        lightAccum += (lights[i].color.rgb * diffuse * attenuation * lightEnergy)
            * (1.0 - shadow) + bounce;

        if (directLight > 0.0)
        {
            float spec = pow(max(dot(viewD, reflect(-lightDir, normal)), 0.0), 24.0);
            specular  += spec * attenuation * lightEnergy * 0.2 * lights[i].color.rgb
                * (1.0 - shadow);
        }
    }

    vec3 color = texelColor.rgb * tint.rgb * (ambient.rgb + lightAccum) + specular;
    color += emissionColor * emissionPower;

    finalColor = vec4(color, texelColor.a * tint.a);
    finalColor = pow(finalColor, vec4(1.0 / 2.2));

    if (finalColor.a < 0.1) discard;
}