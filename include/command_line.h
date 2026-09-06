#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__
#include <string>

enum class RendererOverride {
    None,
    OpenGL,
    Vulkan
};

enum class TriState {
    Unset,
    On,
    Off
};

struct CommandLineOptions {
    bool headless = false;
    bool test_mode = false;
    std::string project_path;

    RendererOverride renderer_override = RendererOverride::None;
    TriState vsync_override = TriState::Unset;
    int fps_override = -1;

    bool no_plugins = false;
    std::string plugins_dir = "plugins";

    std::string lang_override;
    std::string log_level;

    bool no_autosave = false;
    bool new_project = false;

    bool help_requested = false;
    bool version_requested = false;

    int dump_frames = 0;
};

CommandLineOptions parse_command_line(int argc, char** argv);
void print_usage(const char* program_name);
void print_version();

#endif // __COMMAND_LINE_H__