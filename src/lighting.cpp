#include "lighting.h"
#include <cstring>

static bool used[QC_MAX_LIGHTS] = {false};

// lighting
void update_lighting(Shader shader, Lighting& l) {
    l.light.position = l.position;
    l.light.target   = l.target;
    l.light.color    = l.color;
    l.light.enabled  = l.enabled;

    int enabled_int = l.enabled ? 1 : 0;
    int type = l.light.type;
    float position[3] = { l.position.x, l.position.y, l.position.z };
    float target[3] = { l.target.x, l.target.y, l.target.z };
    float color[4] = {l.color.r / 255.f, l.color.g / 255.f, l.color.b / 255.f, l.color.a / 255.f};

    SetShaderValue(shader, l.light.enabledLoc, &enabled_int, SHADER_UNIFORM_INT);
    SetShaderValue(shader, l.light.typeLoc, &type, SHADER_UNIFORM_INT);
    SetShaderValue(shader, l.light.positionLoc, position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, l.light.targetLoc, target, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, l.light.colorLoc, color, SHADER_UNIFORM_VEC4);

    float intensity = l.intensity;
    float range = l.range;

    l.light.attenuation = 1.0f / (range * range > 0.0001f ? range * range : 0.0001f);
    if (l.light.attenuationLoc >= 0)
        SetShaderValue(shader, l.light.attenuationLoc, &l.light.attenuation, SHADER_UNIFORM_FLOAT);

    int intensity_loc = GetShaderLocation(shader, TextFormat("lights[%i].intensity", l.id));
    int range_loc = GetShaderLocation(shader, TextFormat("lights[%i].range", l.id));

    SetShaderValue(shader, intensity_loc, &intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, range_loc, &range, SHADER_UNIFORM_FLOAT);

    if (l.spot_angle_loc == -1)
        l.spot_angle_loc = GetShaderLocation(shader, TextFormat("lights[%i].spotAngle", l.id));

    SetShaderValue(shader, l.spot_angle_loc, &l.spot_angle, SHADER_UNIFORM_FLOAT);
}

void reset_light_registry() {
    for (int i = 0; i < QC_MAX_LIGHTS; i++) {
        used[i] = false;
    }
}

int allocate_light_id() {
    for (int i = 0; i < QC_MAX_LIGHTS; i++) {
        if (!used[i]) { used[i] = true; return i; }
    }

    return -1;
}

void free_light_id(int id) {
    if (id >= 0 && id < QC_MAX_LIGHTS) used[id] = false;
}

Lighting create_lighting(Vec3 pos, Color color) {
    Lighting l   = {};
    l.position   = pos;
    l.target     = Vec3(0,0,0);
    l.color      = color;
    l.enabled    = true;
    l.light.type = LIGHT_POINT;
    l.intensity  = 1.0f;
    l.range      = 5.0f;
    l.spot_angle = 30.0f;
    return l;
}

Light create_light_at_slot(int slot, int type, Vec3 position, Vec3 target, Color color, Shader shader) {
    Light light       = { 0 };
    light.enabled     = true;
    light.type        = type;
    light.position    = position;
    light.target      = target;
    light.color       = color;
    light.enabledLoc  = GetShaderLocation(shader, TextFormat("lights[%i].enabled",  slot));
    light.typeLoc     = GetShaderLocation(shader, TextFormat("lights[%i].type",     slot));
    light.positionLoc = GetShaderLocation(shader, TextFormat("lights[%i].position", slot));
    light.targetLoc   = GetShaderLocation(shader, TextFormat("lights[%i].target",   slot));
    light.colorLoc    = GetShaderLocation(shader, TextFormat("lights[%i].color",    slot));
    return light;
}

static void cache_extended_light_uniform_locations(Lighting& lighting, Shader shader, int slot) {
    lighting.intensity_loc  = GetShaderLocation(shader, TextFormat("lights[%i].intensity", slot));
    lighting.range_loc      = GetShaderLocation(shader, TextFormat("lights[%i].range", slot));
    lighting.spot_angle_loc = GetShaderLocation(shader, TextFormat("lights[%i].spotAngle", slot));
}

void initialize_lighting_uniform_cache(Lighting& lighting, Shader shader, int slot) {
    cache_extended_light_uniform_locations(lighting, shader, slot);
}

