#ifndef __PROJECT_H__
#define __PROJECT_H__
#include <string>
#include "scene.h"

void project_new(const std::string& folder_path, Scene& scene);
void project_save(const std::string& folder_path, const Scene& scene);
void project_save_scene(const std::string& scene_file_path, const Scene& scene);
bool project_load(const std::string& folder_path, Scene& scene, Shader shader);
std::string project_resolve_root(const std::string& path);
std::string get_project_version(const std::string& path);
bool project_is_valid(const std::string& path);

#endif // __PROJECT_H__
