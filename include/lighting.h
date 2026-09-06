#ifndef __LIGHTING_H__
#define __LIGHTING_H__
#include "QuarkCore/QuarkCore.hpp"
#include "QuarkCore/QuarkLights.hpp"
using namespace qc;

#include <string>

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_AREA        3

struct Lighting {
    int id = -1;

    Light light;
    Vec3 position;
    Vec3 target;
    Vec3 rotation;

    Color color = WHITE;
    bool enabled = true;

    float spot_angle = 30.0f;
    int spot_angle_loc = -1;
    int intensity_loc = -1;
    int range_loc = -1;

    float intensity = 1.0f;
    float range = 5.0f;
};

// lighting
Lighting create_lighting(Vec3 pos, Color color);
Light create_light_at_slot(int slot, int type, Vec3 position, Vec3 target, Color color, Shader shader);
void initialize_lighting_uniform_cache(Lighting& lighting, Shader shader, int slot);
void update_lighting(Shader shader, Lighting& l);
void free_light_id(int id);
int allocate_light_id();
void reset_light_registry();

#endif // __LIGHTING_H__
